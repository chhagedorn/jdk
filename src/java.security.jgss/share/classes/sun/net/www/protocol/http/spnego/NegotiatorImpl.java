/*
 * Copyright (c) 2005, 2024, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

package sun.net.www.protocol.http.spnego;

import java.io.IOException;
import java.util.Locale;

import org.ietf.jgss.GSSContext;
import org.ietf.jgss.GSSException;
import org.ietf.jgss.GSSName;
import org.ietf.jgss.Oid;

import sun.net.www.protocol.http.HttpCallerInfo;
import sun.net.www.protocol.http.Negotiator;
import sun.security.jgss.GSSManagerImpl;
import sun.security.jgss.GSSContextImpl;
import sun.security.jgss.GSSUtil;
import sun.security.jgss.HttpCaller;
import sun.security.jgss.krb5.internal.TlsChannelBindingImpl;
import sun.security.util.ChannelBindingException;
import sun.security.util.TlsChannelBinding;

import static sun.security.krb5.internal.Krb5.DEBUG;

/**
 * This class encapsulates all JAAS and JGSS API calls in a separate class
 * outside NegotiateAuthentication.java so that J2SE build can go smoothly
 * without the presence of it.
 *
 * @author weijun.wang@sun.com
 * @since 1.6
 */
public class NegotiatorImpl extends Negotiator {

    private GSSContext context;
    private byte[] oneToken;

    /**
     * Initialize the object, which includes:<ul>
     * <li>Find out what GSS mechanism to use from the system property
     * <code>http.negotiate.mechanism.oid</code>, defaults SPNEGO
     * <li>Creating the GSSName for the target host, "HTTP/"+hostname
     * <li>Creating GSSContext
     * <li>A first call to initSecContext</ul>
     */
    private void init(HttpCallerInfo hci) throws GSSException, ChannelBindingException {
        final Oid oid;

        if (hci.scheme.equalsIgnoreCase("Kerberos")) {
            // we can only use Kerberos mech when the scheme is kerberos
            oid = GSSUtil.GSS_KRB5_MECH_OID;
        } else {
            String pref = System.getProperty("http.auth.preference", "spnego");
            if (pref.equalsIgnoreCase("kerberos")) {
                oid = GSSUtil.GSS_KRB5_MECH_OID;
            } else {
                // currently there is no 3rd mech we can use
                oid = GSSUtil.GSS_SPNEGO_MECH_OID;
            }
        }

        GSSManagerImpl manager = new GSSManagerImpl(
                new HttpCaller(hci));

        // RFC 4559 4.1 uses uppercase service name "HTTP".
        // RFC 4120 6.2.1 demands the host be lowercase
        String peerName = "HTTP@" + hci.host.toLowerCase(Locale.ROOT);

        GSSName serverName = manager.createName(peerName,
                GSSName.NT_HOSTBASED_SERVICE);
        context = manager.createContext(serverName,
                                        oid,
                                        null,
                                        GSSContext.DEFAULT_LIFETIME);

        // Always respect delegation policy in HTTP/SPNEGO.
        if (context instanceof GSSContextImpl) {
            ((GSSContextImpl)context).requestDelegPolicy(true);
        }
        if (hci.serverCert != null) {
            if (DEBUG != null) {
                DEBUG.println("Negotiate: Setting CBT");
            }
            // set the channel binding token
            TlsChannelBinding b = TlsChannelBinding.create(hci.serverCert);
            context.setChannelBinding(new TlsChannelBindingImpl(b.getData()));
        }
        oneToken = context.initSecContext(new byte[0], 0, 0);
    }

    /**
     * Constructor
     * @throws java.io.IOException If negotiator cannot be constructed
     */
    public NegotiatorImpl(HttpCallerInfo hci) throws IOException {
        try {
            init(hci);
        } catch (GSSException | ChannelBindingException e) {
            if (DEBUG != null) {
                DEBUG.println("Negotiate support not initiated, will " +
                        "fallback to other scheme if allowed. Reason:");
                e.printStackTrace();
            }
            try {
                disposeContext();
            } catch (Exception ex) {
                //dispose context silently
            }
            throw new IOException("Negotiate support not initiated", e);
        }
    }

    /**
     * Return the first token of GSS, in SPNEGO, it's called NegTokenInit
     * @return the first token
     */
    @Override
    public byte[] firstToken() {
        return oneToken;
    }

    /**
     * Return the rest tokens of GSS, in SPNEGO, it's called NegTokenTarg
     * @param token the token received from server
     * @return the next token
     * @throws java.io.IOException if the token cannot be created successfully
     */
    @Override
    public byte[] nextToken(byte[] token) throws IOException {
        try {
            if (context == null) {
                throw new IOException("Negotiate support cannot continue. Context is invalidated");
            }
            return context.initSecContext(token, 0, token.length);
        } catch (GSSException e) {
            if (DEBUG != null) {
                DEBUG.println("Negotiate support cannot continue. Reason:");
                e.printStackTrace(DEBUG.getPrintStream());
            }
            throw new IOException("Negotiate support cannot continue", e);
        }
    }

    /**
     * Releases any system resources and cryptographic information stored in
     * the context object and invalidates the context.
     *
     * @throws IOException containing a reason of failure in the cause
     */
    @Override
    public void disposeContext() throws IOException {
        try {
            if (context != null) {
                context.dispose();
            }
        } catch (GSSException e) {
            if (DEBUG != null) {
                DEBUG.println("Cannot release resources. Reason:");
                e.printStackTrace(DEBUG.getPrintStream());
            }
            throw new IOException("Cannot release resources", e);
        };
        context = null;
    }
}
