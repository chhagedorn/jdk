/*
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
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
 *
 */

#ifndef SHARE_OPTO_PREDICATES_HPP
#define SHARE_OPTO_PREDICATES_HPP

#include "opaquenode.hpp"
#include "opto/cfgnode.hpp"
#include "loopnode.hpp"

/*
 * There are different kinds of predicates throughout the code. We differentiate between the following predicates:
 *
 * - Regular Predicate: This term is used to refer to a Parse Predicate or a Runtime Predicate and can be used to
 *                      distinguish from any Assertion Predicate.
 * - Parse Predicate: Added during parsing to capture the current JVM state. This predicate represents a "placeholder"
 *                    above which more Runtime Predicates can be created later after parsing.
 *
 *                    There are initially three Parse Predicates for each loop:
 *                    - Loop Parse Predicate:             The Parse Predicate added for Loop Predicates.
 *                    - Profiled Loop Parse Predicate:    The Parse Predicate added for Profiled Loop Predicates.
 *                    - Loop Limit Check Parse Predicate: The Parse Predicate added for a Loop Limit Check Predicate.
 * - Runtime Predicate: This term is used to refer to a Hoisted Predicate (either a Loop Predicate or a Profiled Loop
 *                      Predicate) or a Loop Limit Check Predicate. These predicates will be checked at runtime while the
 *                      Parse and Assertion Predicates are always removed before code generation (except for Initialized
 *                      Assertion Predicates which are kept in debug builds while being removed in product builds).
 *     - Hoisted Predicate: Either a Loop Predicate or a Profiled Loop Predicate that was created during Loop Predication
 *                          to hoist a check out of a loop. Each Hoisted Predicate is accompanied by additional
 *                          Assertion Predicates.
 *         - Loop Predicate:     A predicate that can either hoist a loop-invariant check out of a loop or a range check
 *                               of the form "a[i*scale + offset]", where scale and offset are loop-invariant, out of a
 *                               counted loop. A check must be executed in each loop iteration to hoist it. Otherwise, no
 *                               Loop Predicate can be created. This predicate is created during Loop Predication and is
 *                               inserted above the Loop Parse Predicate.
 *         - Profiled Loop:      This predicate is very similar to a Loop Predicate but the hoisted check does not need
 *           Predicate           to be executed in each loop iteration. By using profiling information, only checks with
 *                               a high execution frequency are chosen to be replaced by a Profiled Loop Predicate. This
 *                               predicate is created during Loop Predication and is inserted above the Profiled Loop
 *                               Parse Predicate.
 *     - Loop Limit Check:   This predicate is created when transforming a loop to a counted loop to protect against
 *       Predicate           the case when adding the stride to the induction variable would cause an overflow which
 *                           will not satisfy the loop limit exit condition. This overflow is unexpected for further
 *                           counted loop optimizations and could lead to wrong results. Therefore, when this predicate
 *                           fails at runtime, we must trap and recompile the method without turning the loop into a
 *                           a counted loop to avoid these overflow problems.
 *                           The predicate does not replace an actual check inside the loop. This predicate can only
 *                           be added once above the Loop Limit Check Parse Predicate for a loop.
 * - Assertion Predicate: An always true predicate which will never fail (its range is already covered by an earlier
 *                        Hoisted Predicate or the main-loop entry guard) but is required in order to fold away a dead
 *                        sub loop inside which some data could be proven to be dead (by the type system) and replaced
 *                        by top. Without such Assertion Predicates, we could find that type ranges in Cast and ConvX2Y
 *                        data nodes become impossible and are replaced by top. This is an indicator that the sub loop
 *                        is never executed and must be dead. But there is no way for C2 to prove that the sub loop is
 *                        actually dead. Assertion Predicates come to the rescue to fold such seemingly dead sub loops
 *                        away to avoid a broken graph. Assertion Predicates are left in the graph as a sanity checks in
 *                        debug builds (they must never fail at runtime) while they are being removed in product builds.
 *                        We use special Opaque4 nodes to block some optimizations and replace the Assertion Predicates
 *                        later in product builds.
 *
 *                        There are two kinds of Assertion Predicates:
 *                        - Template Assertion Predicate:    A template for an Assertion Predicate that uses OpaqueLoop*
 *                                                           nodes as placeholders for the init and stride value of a loop.
 *                                                           This predicate does not represent an actual check, yet, and
 *                                                           just serves as a template to create an Initialized Assertion
 *                                                           Predicate for a (sub) loop.
 *                        - Initialized Assertion Predicate: An Assertion Predicate that represents an actual check for a
 *                                                           (sub) loop that was initialized by cloning a Template
 *                                                           Assertion Predicate. The check is always true and is covered
 *                                                           by an earlier check (a Hoisted Predicate or the main-loop
 *                                                           entry guard).
 *
 *                        Assertion Predicates are required when removing a range check from a loop. These are inserted
 *                        either at Loop Predication or at Range Check Elimination:
 *                        - Loop Predication:        A range check inside a loop is replaced by a Hoisted Predicate before
 *                                                   the loop. We add two additional Template Assertion Predicates from
 *                                                   which we can later create Initialized Assertion Predicates. One
 *                                                   would have been enough if the number of array accesses inside a sub
 *                                                   loop does not change. But when unrolling the sub loop, we are
 *                                                   doubling the number of array accesses - we need to cover them all.
 *                                                   To do that, we only need to create an Initialized Assertion Predicate
 *                                                   for the first, initial value and for the last value:
 *                                                   Let a[i] be an array access in the original, not-yet unrolled loop
 *                                                   with stride 1. When unrolling this loop, we double the stride
 *                                                   (i.e. stride 2) and have now two accesses a[i] and a[i+1]. We need
 *                                                   checks for both. When further unrolling this loop, we only need to
 *                                                   keep the checks on the first and last access (e.g. a[i] and a[i+3]
 *                                                   on the next unrolling step as they cover the checks in the middle
 *                                                   for a[i+1] and a[i+2]).
 *                                                   Therefore, we just need to cover:
 *                                                   - Initial value: a[init]
 *                                                   - Last value: a[init + new stride - original stride]
 *                                                   (We could still only use one Template Assertion Predicate to create
 *                                                   both Initialized Assertion Predicates from - might be worth doing
 *                                                   at some point).
 *                                                   When later splitting a loop (pre/main/post, peeling, unrolling),
 *                                                   we create two Initialized Assertion Predicates from the Template
 *                                                   Assertion Predicates by replacing the OpaqueLoop* nodes by actual
 *                                                   values. Initially (before unrolling), both Assertion Predicates are
 *                                                   equal. The Initialized Assertion Predicates are always true because
 *                                                   their range is covered by a corresponding Hoisted Predicate.
 *                        - Range Check Elimination: A range check is removed from the main-loop by changing the pre
 *                                                   and main-loop iterations. We add two additional Template Assertion
 *                                                   Predicates (see explanation in section above) and one Initialized
 *                                                   Assertion Predicate for the just removed range check. When later
 *                                                   unrolling the main-loop, we create two Initialized Assertion
 *                                                   Predicates from the Template Assertion Predicates by replacing the
 *                                                   OpaqueLoop* nodes by actual values for the unrolled loop.
 *                                                   The Initialized Assertion Predicates are always true because we will
 *                                                   never enter the main loop because of the changed pre- and main-loop
 *                                                   exit conditions.
 *                                                   Note that Range Check Elimination could remove additional range
 *                                                   checks which we were not possible to remove with Loop Predication
 *                                                   before (for example, because no Parse Predicates were available
 *                                                   before the loop to create Hoisted Predicates with).
 *
 *
 * In order to group predicates and refer to them throughout the code, we introduce the following additional terms:
 * - Regular Predicate Block: A Regular Predicate Block groups all Runtime Predicates in a Runtime Predicate Block
 *                            together with their dedicated Parse Predicate from which they were created (all predicates
 *                            share the same uncommon trap). The Runtime Predicate Block could be empty (i.e. no
 *                            Runtime Predicates created) and the Parse Predicate could be missing (after removing Parse
 *                            Predicates). There are three such Regular Predicate Blocks:
 *                            - Loop Predicate Block
 *                            - Profiled Loop Predicate Block
 *                            - Loop Limit Check Predicate Block
 * - Runtime Predicate Block: A block containing all Runtime Predicates that share the same uncommon trap (i.e. belonging
 *                            to a single Parse Predicate which is not included in this block). This block could be empty
 *                            if there were no Runtime Predicates created with the Parse Predicate below this block.
 *                            For the time being: We also count Assertion Predicates to this block but that will be
 *                            changed with the redesign of Assertion Predicates where we remove them from this block
 *                            (JDK-8288981).
 *
 * Initially, before applying any loop-splitting optimizations, we find the following structure after Loop Predication
 * (predicates inside square brackets [] do not need to exist if there are no checks to hoist):
 *
 *   [Loop Hoisted Predicate 1 + 2 Template Assertion Predicates]                 \ Runtime       \
 *   [Loop Hoisted Predicate 2 + 2 Template Assertion Predicates]                 | Predicate     |
 *   ...                                                                          | Block         | Loop Predicate Block
 *   [Loop Hoisted Predicate n + 2 Template Assertion Predicates]                 /               |
 * Loop Parse Predicate                                                                           /
 *
 *   [Profiled Loop Hoisted Predicate 1 + 2 Template Assertion Predicates]       \ Runtime       \
 *   [Profiled Loop Hoisted Predicate 2 + 2 Template Assertion Predicates]       | Predicate     | Profiled Loop
 *   ...                                                                         | Block         | Predicate Block
 *   [Profiled Loop Hoisted Predicate m + 2 Template Assertion Predicates]       /               |
 * Profiled Loop Parse Predicate                                                                 /
 *                                                                               \ Runtime
 *   [Loop Limit Check Predicate] (at most 1)                                    / Predicate    \ Loop Limit Check
 * Loop Limit Check Parse Predicate                                                Block        / Predicate Block
 * Loop Head
 *
 * As an example, let's look at how the predicate structure looks for the main-loop after creating pre/main/post loops
 * and applying Range Check Elimination (the order is insignificant):
 *
 * Main Loop entry (zero-trip) guard
 *   [For Loop Predicate 1: 2 Template + 2 Initialized Assertion Predicates]
 *   [For Loop Predicate 2: 2 Template + 2 Initialized Assertion Predicates]
 *   ...
 *   [For Loop Predicate n: 2 Template + 2 Initialized Assertion Predicates]
 *
 *   [For Profiled Loop Predicate 1: 2 Template + 2 Initialized Assertion Predicates]
 *   [For Profiled Loop Predicate 2: 2 Template + 2 Initialized Assertion Predicates]
 *   ...
 *   [For Profiled Loop Predicate m: 2 Template + 2 Initialized Assertion Predicates]
 *
 *   (after unrolling, we have 2 Initialized Assertion Predicates for the Assertion Predicates of Range Check Elimination)
 *   [For Range Check Elimination Check 1: 2 Templates + 1 Initialized Assertion Predicate]
 *   [For Range Check Elimination Check 2: 2 Templates + 1 Initialized Assertion Predicate]
 *   ...
 *   [For Range Check Elimination Check k: 2 Templates + 1 Initialized Assertion Predicate]
 * Main Loop Head
 */

class Predicates;
class TransformRelatedNodes;
class TransformOpaqueLoopNodes;
class TemplateAssertionPredicateBlock;

enum class AssertionPredicateType {
  None,
  Init_value,
  Last_value
};


// Class to represent a Parse Predicate.
class ParsePredicate : public StackObj {
  ParsePredicateSuccessProj* _success_proj;
  ParsePredicateNode* _parse_predicate_node;
  Node* _entry;

 public:
  ParsePredicate(Node* parse_predicate_proj)
      : _success_proj(parse_predicate_proj->isa_IfTrue()),
        _parse_predicate_node(ParsePredicateNode::is_success_proj(parse_predicate_proj)
                              ? parse_predicate_proj->in(0)->as_ParsePredicate() : nullptr),
        _entry(_parse_predicate_node != nullptr ? _parse_predicate_node->in(0) : parse_predicate_proj) {}

  // Returns the control input node into this Parse Predicate if it is valid. Otherwise, it returns the passed node
  // into the constructor of this class.
  Node* entry() const {
    return _entry;
  }

  // Is this Parse Predicate valid (i.e. passed a Parse Predicate projection to the constructor)?
  bool is_valid() const {
    return _parse_predicate_node != nullptr;
  }

  ParsePredicateNode* node() const {
    assert(is_valid(), "must be valid");
    return _parse_predicate_node;
  }

  ParsePredicateSuccessProj* success_proj() const {
    assert(is_valid(), "must be valid");
    return _success_proj;
  }
};

// Utility class for queries on Runtime Predicates.
class RuntimePredicate : public StackObj {
  static Deoptimization::DeoptReason uncommon_trap_reason(IfProjNode* if_proj);
 public:
  static bool is_success_proj(Node* node);
  static bool is_success_proj(Node* node, Deoptimization::DeoptReason deopt_reason);
};

// This class represents a single Runtime Predicate block containing all Runtime Predicates that share the same uncommon
// trap (i.e. Loop Predicates, Profiled Loop Predicates, or a Loop Limit Check Predicate). This block could also be empty
// if we have not created any Runtime Predicates for this block, yet.
class RuntimePredicateBlock : public StackObj {
  // Last node in this block which is either:
  // The IfProj of the last Runtime Predicate (if any)
  // _entry (if block is empty)
  Node* _last;
  Node* _entry;

  // Walk over all Runtime Predicates of this block (if any) and return the first node not belonging to the block
  // anymore (i.e. entry into the first Runtime Predicate in this Runtime Predicate block).
  static Node* find_entry(Node* runtime_predicate_proj, Deoptimization::DeoptReason deopt_reason) {
    Node* entry = runtime_predicate_proj;
    while (RuntimePredicate::is_success_proj(entry, deopt_reason)) {
      assert(entry->in(0)->as_If(), "must be If node");
      entry = entry->in(0)->in(0);
    }
    return entry;
  }

 public:
  RuntimePredicateBlock(Node* runtime_predicate_proj, Deoptimization::DeoptReason deopt_reason)
      : _last(runtime_predicate_proj),
        _entry(find_entry(runtime_predicate_proj, deopt_reason)) {}

  // Returns the control input node into this Runtime Predicate block. This is either:
  // - The control input to the first If node in the block representing a Runtime Predicate if there is at least one
  //   Runtime Predicate.
  // - The same node initially passed to the constructor if this Runtime Predicate block is empty (i.e. no Runtime
  //   Predicate).
  Node* entry() const {
    return _entry;
  }

  bool is_empty() const {
    return _last == _entry;
  }
};

// This class represents a single Regular Predicate block that contains a possibly empty Runtime Predicate block
// (either for Loop Predicates, Profiled Loop Predicates, or a Loop Limit Check Predicate) followed by a Parse Predicate
// which, however, does not need to exist (we could already have decided to remove Parse Predicates for this loop).
class RegularPredicateBlock : public StackObj {
  // Last node in this block which is either:
  // - The IfProj of the Parse Predicate (if it exists).
  // - The IfProj of a Runtime Predicate (if Parse Predicate already removed)
  // _entry (if block is empty)
  Node* _last;
  Deoptimization::DeoptReason _deopt_reason;
  ParsePredicate _parse_predicate; // Could be missing.
  RuntimePredicateBlock _runtime_predicate_block; // Could be empty.
  Node* _entry;

 public:
  RegularPredicateBlock(Node* predicate_proj, Deoptimization::DeoptReason deopt_reason)
      : _last(predicate_proj),
        _deopt_reason(deopt_reason),
        _parse_predicate(predicate_proj),
        _runtime_predicate_block(_parse_predicate.entry(), deopt_reason),
        _entry(_runtime_predicate_block.entry()) {}

  // Returns the control input node into this Regular Predicate block. This is either:
  // - The control input to the first If node in the block representing a Runtime Predicate if there is at least one
  //   Runtime Predicate.
  // - The control input node into the ParsePredicate node if there is only a Parse Predicate and no Runtime Predicate.
  // - The same node initially passed to the constructor if this Regular Predicate block is empty (i.e. no Parse
  //   Predicate or Runtime Predicate).
  Node* entry() const {
    return _entry;
  }

  Deoptimization::DeoptReason deopt_reason() const {
    return _deopt_reason;
  }

  bool is_empty() const {
    return _last == _entry;
  }

  bool has_parse_predicate() const {
    return _parse_predicate.is_valid();
  }

  ParsePredicateNode* parse_predicate() const {
    return _parse_predicate.node();
  }

  ParsePredicateSuccessProj* parse_predicate_success_proj() const {
    return _parse_predicate.success_proj();
  }

  const RuntimePredicateBlock* runtime_predicate_block() const {
    return &_runtime_predicate_block;
  }

  bool has_runtime_predicates() const {
    return !_runtime_predicate_block.is_empty();
  }
};

// This class iterates over the Template Assertion Predicates which are all in the same block below the Predicate Blocks.
class TopDownTemplateAssertionPredicateIterator : public StackObj {
  Node* _current;

 public:
  TopDownTemplateAssertionPredicateIterator(const TemplateAssertionPredicateBlock* template_assertion_predicate_block);

  bool has_next() const {
    return _current != nullptr && _current->is_TemplateAssertionPredicate();
  }

  TemplateAssertionPredicateNode* next();
};

// This class iterates over the Template Assertion Predicates which are all in the same block below the Predicate Blocks.
class TemplateAssertionPredicateIterator : public StackObj {
  Node* _current;

 public:
  TemplateAssertionPredicateIterator(Node* maybe_template_assertion_predicate)
      : _current(maybe_template_assertion_predicate) {}

  bool has_next() const {
    return _current != nullptr && _current->is_TemplateAssertionPredicate();
  }

  TemplateAssertionPredicateNode* next();
};


// This class represents a Template Assertion Predicate Block directly above a loop head.
class TemplateAssertionPredicateBlock : public StackObj {
  Node* _entry;
  TemplateAssertionPredicateNode* _first;
  TemplateAssertionPredicateNode* _last;

 public:
  TemplateAssertionPredicateBlock(Node* loop_entry);

  Node* entry() const {
    return _entry;
  }

  bool has_any() const {
    return _last != nullptr;
  }

  // Returns the first Template Assertion Predicate node of the block or null if there is none.
  TemplateAssertionPredicateNode* first() const {
    return _first;
  }

  // Returns the last Template Assertion Predicate node of the block or null if there is none.
  TemplateAssertionPredicateNode* last() const {
    return _last;
  }

  // Mark all Template Assertion Predicates useless inside this block and add them to the IGVN worklist.
  void mark_useless(PhaseIterGVN* igvn) const {
    TemplateAssertionPredicateIterator iterator(_last);
    while (iterator.has_next()) {
      TemplateAssertionPredicateNode* template_assertion_predicate = iterator.next();
      template_assertion_predicate->mark_useless();
      igvn->_worklist.push(template_assertion_predicate);
    }
  }
};

class InitializedAssertionPredicateBlock : public StackObj {
  Node* _entry;
  IfNode* _first_if;
  Node* _last_initialized_assertion_predicate_proj;

 public:
  InitializedAssertionPredicateBlock(Node* initialized_assertion_predicate_proj);

  bool has_any() const {
    return _last_initialized_assertion_predicate_proj != _entry;
  }

  Node* entry() const {
    return _entry;
  }

  void kill_dead(PhaseIterGVN* igvn) const;
};

class AssertionPredicateBlock : public StackObj {
  Node* _last;
  TemplateAssertionPredicateBlock _template_assertion_predicate_block;
  InitializedAssertionPredicateBlock _initialized_assertion_predicate_block;
  Node* _entry;

 public:
  AssertionPredicateBlock(Node* loop_entry)
      : _last(loop_entry),
        _template_assertion_predicate_block(loop_entry),
        _initialized_assertion_predicate_block(_template_assertion_predicate_block.entry()),
        _entry(_initialized_assertion_predicate_block.entry()) {}

  bool has_any() const {
    return _entry != _last;
  }

  Node* entry() const {
    return _entry;
  }

  const TemplateAssertionPredicateBlock* template_assertion_predicate_block() const {
    return &_template_assertion_predicate_block;
  }

  const InitializedAssertionPredicateBlock* initialized_assertion_predicate_block() const {
    return &_initialized_assertion_predicate_block;
  }
};

// This class takes a loop entry node and finds all the available predicates for the loop.
class Predicates : public StackObj {
  Node* _loop_entry;
  AssertionPredicateBlock _assertion_predicate_block;
  RegularPredicateBlock _loop_limit_check_predicate_block;
  RegularPredicateBlock _profiled_loop_predicate_block;
  RegularPredicateBlock _loop_predicate_block;
  Node* _entry;

 public:
  Predicates(Node* loop_entry)
      : _loop_entry(loop_entry),
        _assertion_predicate_block(loop_entry),
        _loop_limit_check_predicate_block(_assertion_predicate_block.entry(), Deoptimization::Reason_loop_limit_check),
        _profiled_loop_predicate_block(_loop_limit_check_predicate_block.entry(),
                                       Deoptimization::Reason_profile_predicate),
        _loop_predicate_block(_profiled_loop_predicate_block.entry(),
                              Deoptimization::Reason_predicate),
        _entry(_loop_predicate_block.entry()) {}

  // Returns the control input the first predicate if there are any predicates. If there are no predicates, the same
  // node initially passed to the constructor is returned.
  Node* entry() const {
    return _entry;
  }

  bool has_parse_predicates() const {
    return _loop_predicate_block.has_parse_predicate() ||
           _profiled_loop_predicate_block.has_parse_predicate() ||
           _loop_limit_check_predicate_block.has_parse_predicate();
  }

  const RegularPredicateBlock* loop_predicate_block() const {
    return &_loop_predicate_block;
  }

  const RegularPredicateBlock* profiled_loop_predicate_block() const {
    return &_profiled_loop_predicate_block;
  }

  const RegularPredicateBlock* loop_limit_check_predicate_block() const {
    return &_loop_limit_check_predicate_block;
  }

  const TemplateAssertionPredicateBlock* template_assertion_predicate_block() const {
    return _assertion_predicate_block.template_assertion_predicate_block();
  }

  const InitializedAssertionPredicateBlock* initialized_assertion_predicate_block() const {
    return _assertion_predicate_block.initialized_assertion_predicate_block();
  }

  bool has_any() const {
    return _entry != _loop_entry;
  }
};

// This class iterates over the Parse Predicates within a predicate block.
class ParsePredicateIterator : public StackObj {
  GrowableArray<ParsePredicateNode*> _parse_predicates;
  int _current_index;

 public:
  ParsePredicateIterator(const Predicates& predicates);

  bool has_next() const {
    return _current_index < _parse_predicates.length();
  }

  ParsePredicateNode* next();
};

// Eliminate all useless Parse Predicates by marking them as useless and adding them to the IGVN worklist. These are
// then removed in the next IGVN round.
class EliminateUselessParsePredicates : public StackObj {
  Compile* C;
  PhaseIterGVN* _igvn;
  IdealLoopTree* _ltree_root;

  void mark_all_parse_predicates_useless();
  static void mark_parse_predicates_useful(IdealLoopTree* loop);
  void add_useless_predicates_to_igvn();

 public:
  EliminateUselessParsePredicates(Compile* C, PhaseIterGVN* igvn, IdealLoopTree* ltree_root)
      : C(C),
        _igvn(igvn),
        _ltree_root(ltree_root) {}

  void eliminate();
};

// Interface to create a new Parse Predicate node (clone of an old Parse Predicate) for either the fast or slow loop.
// The fast loop needs to create some additional clones while the slow loop can reuse old nodes.
class NewParsePredicate : public StackObj {
 public:
  virtual Node* create(PhaseIdealLoop* phase, Node* new_entry, ParsePredicateSuccessProj* old_proj) = 0;
};

// Class to clone Parse Predicates.
class ParsePredicates : public StackObj {
  Node* _new_entry;
  NewParsePredicate* _new_parse_predicate;
  PhaseIdealLoop* _phase;
  LoopNode* _loop_node;

  void clone_parse_predicate(const RegularPredicateBlock* regular_predicate_block);
  void create_new_parse_predicate(ParsePredicateSuccessProj* predicate_proj);

 public:
  ParsePredicates(NewParsePredicate* new_parse_predicate, Node* new_entry, PhaseIdealLoop* phase, LoopNode* loop_node)
      : _new_entry(new_entry),
        _new_parse_predicate(new_parse_predicate),
        _phase(phase),
        _loop_node(loop_node) {}

  Node* clone(const Predicates* predicates);
};

class AssertionPredicateBoolOpcodes : public StackObj {
 public:
  // Is 'n' a node that could be part of an Assertion Predicate bool (i.e. could be found on the input chain of an
  // Assertion Predicate bool up to and including the OpaqueLoop* nodes)?
  static bool is_valid(const Node* n) {
    const int op = n->Opcode();
    return (op == Op_OpaqueLoopInit ||
            op == Op_OpaqueLoopStride ||
            n->is_Bool() ||
            n->is_Cmp() ||
            op == Op_AndL ||
            op == Op_OrL ||
            op == Op_RShiftL ||
            op == Op_LShiftL ||
            op == Op_LShiftI ||
            op == Op_AddL ||
            op == Op_AddI ||
            op == Op_MulL ||
            op == Op_MulI ||
            op == Op_SubL ||
            op == Op_SubI ||
            op == Op_ConvI2L ||
            op == Op_CastII);
  }
};

// Class that represents either the BoolNode for the initial value or the last value of a Template Assertion Predicate.
class TemplateAssertionPredicateBool : public StackObj {
  BoolNode* _source_bol;
  PhaseIdealLoop* _phase;
  Compile* C;

  BoolNode* transform(TransformRelatedNodes* transform_opaque_loop_nodes);
 public:
  TemplateAssertionPredicateBool(BoolNode* source_bol, PhaseIdealLoop* phase)
      : _source_bol(source_bol),
        _phase(phase),
        C(phase->C) {
#ifdef ASSERT
    // We could have multiple Template Assertion Predicates as output if two range checks of the same array become the
    // same at some point. This is okay, as we are always cloning the Template Assertion Predicate bools when creating a
    // new Assertion Predicate from it.
    for (DUIterator_Fast imax, i = source_bol->fast_outs(imax); i < imax; i++) {
      Node* out = source_bol->fast_out(i);
      assert(out->is_TemplateAssertionPredicate(), "must be template assertion predicate");
    }
#endif // ASSERT
  }

  // Create a new BoolNode from the Template Assertion Predicate bool by cloning all nodes on the input chain up to but
  // not including the OpaqueLoop* nodes. These are replaced depending on the strategy. Sets 'ctrl' as new ctrl for all
  // cloned (non-CFG) nodes.

  BoolNode* clone(Node* new_ctrl);
  BoolNode* clone_update_opaque_init(Node* new_opaque_init_input);
  BoolNode* clone_initialized(Node* new_init, Node* new_stride);
  void update_opaque_stride(Node* new_opaque_stride_input);
};

// Interface class for different strategies on how to transform OpaqueLoop* at old Template Assertion Predicate Bools to
// use for a new Template Assertion Predicate. The old OpaqueLoop* nodes could either be:
// - cloned (no modification to init and stride)
// - replaced by new OpaqueLoop* nodes (with new init and stride value)
// - replaced by non-OpaqueLoop* nodes
class TransformOpaqueLoopNodes : public StackObj {
 public:
  virtual Node* transform_init(OpaqueLoopInitNode* init, Node* ctrl) = 0;
  virtual Node* transform_stride(OpaqueLoopStrideNode* stride, Node* ctrl) = 0;
};

// This transformation strategy clones OpaqueLoop* nodes without updating the init and stride input values.
class CloneOpaqueLoopNodes : public TransformOpaqueLoopNodes {
  PhaseIdealLoop* _phase;

  Node* clone_old(const Node* old_node, Node* new_ctrl) {
    Node* clone = old_node->clone();
    _phase->register_new_node(clone, new_ctrl);
    return clone;
  }

 public:
  CloneOpaqueLoopNodes(PhaseIdealLoop* phase) : _phase(phase) {}

  Node* transform_init(OpaqueLoopInitNode* init, Node* ctrl) override {
    return clone_old(init, ctrl);
  }

  Node* transform_stride(OpaqueLoopStrideNode* stride, Node* ctrl) override {
    return clone_old(stride, ctrl);
  }
};

// This transformation strategy creates new OpaqueLoop* nodes by using new init and stride value.
class CreateOpaqueLoopNodes : public TransformOpaqueLoopNodes {
  Node* _new_init;
  Node* _new_stride;
  PhaseIdealLoop* _phase;

  Node* clone(Node* opaque_node, Node* new_input, Node* new_ctrl) {
    Node* opaque_clone = opaque_node->clone();
    _phase->igvn().replace_input_of(opaque_clone, 1, new_input);
    _phase->register_new_node(opaque_clone, new_ctrl);
    return opaque_clone;
  }

 public:
  CreateOpaqueLoopNodes(Node* new_init, Node* new_stride, PhaseIdealLoop* phase)
      : _new_init(new_init),
        _new_stride(new_stride),
        _phase(phase) {}

  Node* transform_init(OpaqueLoopInitNode* init, Node* ctrl) override {
    return clone(init, _new_init, ctrl);
  }
  Node* transform_stride(OpaqueLoopStrideNode* stride, Node* ctrl) override {
    return clone(stride, _new_stride, ctrl);
  }
};

// This transformation strategy replaces only the OpaqueLoopInitNode with a new init node.
class ReplaceOpaqueLoopInit : public TransformOpaqueLoopNodes {
  Node* _new_init;

 public:
  ReplaceOpaqueLoopInit(Node* new_init) : _new_init(new_init) {}

  Node* transform_init(OpaqueLoopInitNode* init, Node* ctrl) override {
    return _new_init;
  }

  Node* transform_stride(OpaqueLoopStrideNode* stride, Node* ctrl) override {
    assert(false, "should not be visited when cloning an init value template assertion bool");
    return stride; // Do nothing.
  }
};

// This transformation strategy replaces all OpaqueLoopNode* with a new init and stride node, respectively.
class ReplaceOpaqueLoopInitAndStride : public TransformOpaqueLoopNodes {
  Node* _new_init;
  Node* _new_stride;

 public:
  ReplaceOpaqueLoopInitAndStride(Node* new_init, Node* new_stride)
      : _new_init(new_init),
        _new_stride(new_stride) {}

  Node* transform_init(OpaqueLoopInitNode* init, Node* ctrl) override {
    return _new_init;
  }

  Node* transform_stride(OpaqueLoopStrideNode* stride, Node* ctrl) override {
    return _new_stride;
  }
};

// Interface to check if an output node of a Template Assertion Predicate node, from which we created a new Template
// Assertion Predicate, belongs to the target loop or not. Based on whether the target loop is the cloned or the original
// loop, the answer can vary and is determined by checking the first node index of the cloned loop and/or the mapping
// of old nodes to cloned nodes.
class NodeInTargetLoop : public StackObj {
 public:
  virtual bool check(Node* node) = 0;
};

// Strategy to check if a node belongs to the cloned loop which is the target loop.
class NodeInClonedLoop : public NodeInTargetLoop {
  uint _first_node_index_in_cloned_loop;

 public:
  explicit NodeInClonedLoop(uint first_node_index_in_cloned_loop)
      : _first_node_index_in_cloned_loop(first_node_index_in_cloned_loop) {}

  bool check(Node* node) override {
    return node->_idx >= _first_node_index_in_cloned_loop;
  }
};

// Strategy to check if a node belongs to the original loop which is the target loop.
class NodeInOriginalLoop : public NodeInTargetLoop {
  uint _first_node_index_in_cloned_loop;
  Node_List* _old_new;

 public:
  explicit NodeInOriginalLoop(uint first_node_index_in_cloned_loop, Node_List* old_new)
       : _first_node_index_in_cloned_loop(first_node_index_in_cloned_loop),
         _old_new(old_new) {}

  // Check if 'node' is not a cloned node (i.e. < _first_node_index_in_cloned_loop) and if we've created a clone from
  // it (with _old_new). If there is a clone, we know that 'node' belongs to the original loop.
  bool check(Node* node) override {
    if (node->_idx < _first_node_index_in_cloned_loop) {
      Node* cloned_node = _old_new->at(node->_idx);
      return cloned_node != nullptr && cloned_node->_idx >= _first_node_index_in_cloned_loop;
    } else {
      return false;
    }
  }
};

// This class provides an interface to update a Template Assertion Predicate by either replacing it completely or to
// only update its Bool nodes.
class TemplateAssertionPredicate : public StackObj {
  TemplateAssertionPredicateNode* _template_assertion_predicate;
  TemplateAssertionPredicateBool _init_value_bool;
  TemplateAssertionPredicateBool _last_value_bool;
  TransformOpaqueLoopNodes* _transform_opaque_loop_nodes;
  PhaseIdealLoop* _phase;

  void create_bools(Node* new_ctrl, TemplateAssertionPredicateNode* new_template_assertion_predicate);
  void update_data_dependencies(Node* template_assertion_predicate, NodeInTargetLoop* node_in_target_loop); // TODO

 public:
  TemplateAssertionPredicate(TemplateAssertionPredicateNode* template_assertion_predicate,
                             TransformOpaqueLoopNodes* transform_opaque_loop_nodes, PhaseIdealLoop* phase)
      : _template_assertion_predicate(template_assertion_predicate),
        _init_value_bool(TemplateAssertionPredicateBool(template_assertion_predicate->in(TemplateAssertionPredicateNode::InitValue)->as_Bool(),
                                                        phase)),
        _last_value_bool(TemplateAssertionPredicateBool(template_assertion_predicate->in(TemplateAssertionPredicateNode::LastValue)->as_Bool(),
                                                        phase)),
        _transform_opaque_loop_nodes(transform_opaque_loop_nodes),
        _phase(phase) {}

  TemplateAssertionPredicateNode* create(Node* new_ctrl, NodeInTargetLoop* _node_in_target_loop);
  void update_bools();
};

// Interface class for different strategies on how to create a Template Assertion Predicate from an existing one.
// A Template Assertion Predicate could either be:
// - cloned with its Bools (no modification to the OpaqueLoop* nodes)
// - newly created with new Bools and new OpaqueLoop* nodes using a new init and stride value.
class NewTemplateAssertionPredicate : public StackObj {
 public:
  virtual TemplateAssertionPredicateNode* create_from(Node* ctrl, TemplateAssertionPredicateNode* template_assertion_predicate_node) = 0;
};

// Strategy to clone an existing Template Assertion Predicate and their Bools, including the OpaqueLoop* nodes. We do
// not provide a new init or stride value for the cloned OpaqueLoop* nodes.
class CloneTemplateAssertionPredicate : public NewTemplateAssertionPredicate {
  PhaseIdealLoop* _phase;
  CloneOpaqueLoopNodes _clone_opaque_loop_nodes;
  NodeInTargetLoop* _node_in_target_loop;

 public:
  CloneTemplateAssertionPredicate(PhaseIdealLoop* phase, NodeInTargetLoop* node_in_target_loop)
      : _phase(phase),
        _clone_opaque_loop_nodes(phase),
        _node_in_target_loop(node_in_target_loop) {}


  TemplateAssertionPredicateNode*
  create_from(Node* new_ctrl, TemplateAssertionPredicateNode* template_assertion_predicate_node) override {
    TemplateAssertionPredicate template_assertion_predicate(template_assertion_predicate_node, &_clone_opaque_loop_nodes,
                                                            _phase);
    return template_assertion_predicate.create(new_ctrl, _node_in_target_loop);
  }
};

// Strategy to update an existing Template Assertion Predicate by providing it new Bools that use new OpaqueLoop* nodes
// with the provided init and stride value.


// Strategy to create a new Template Assertion Predicate with new Bools that use new OpaqueLoop* nodes with the provided
// init and stride value. We always use a dedicated node as control input to the new Template Assertion Predicate and
// as ctrl for its created data nodes in the Bools.
class CreateTemplateAssertionPredicate : public NewTemplateAssertionPredicate {
  PhaseIdealLoop* _phase;
  CreateOpaqueLoopNodes _create_opaque_loop_nodes;
  NodeInTargetLoop* _node_in_target_loop;

 public:
  CreateTemplateAssertionPredicate(Node* new_init, Node* new_stride, PhaseIdealLoop* phase,
                                   NodeInTargetLoop* node_in_target_loop)
      : _phase(phase),
        _create_opaque_loop_nodes(new_init, new_stride, phase),
        _node_in_target_loop(node_in_target_loop) {}

  TemplateAssertionPredicateNode*
  create_from(Node* new_ctrl, TemplateAssertionPredicateNode* template_assertion_predicate_node) override;
};

// This class represents Template Assertion Predicates which can be cloned to a new target loop or updated at a source loop.
class TemplateAssertionPredicates : public StackObj {
  const TemplateAssertionPredicateBlock* _template_assertion_predicate_block;
  PhaseIdealLoop* _phase;

  TemplateAssertionPredicateNode* create_templates(Node* new_entry,
                                                   NewTemplateAssertionPredicate* new_template_assertion_predicate);
 public:
  TemplateAssertionPredicates(const TemplateAssertionPredicateBlock* template_assertion_predicate_block, PhaseIdealLoop* phase)
      : _template_assertion_predicate_block(template_assertion_predicate_block),
        _phase(phase) {}

  // Replace the Template Assertion Predicates found in 'predicates' by cloning them and replace the OpaqueLoop* nodes
  // with the provided 'new_init' and 'new_stride' nodes. Returns the last node in the newly cloned Template Assertion
  // Predicate chain.
  TemplateAssertionPredicateNode* clone_templates(Node* new_entry,
                                                  NodeInTargetLoop* node_in_target_loop) {
    CloneTemplateAssertionPredicate clone_template_assertion_predicate(_phase, node_in_target_loop);
    return create_templates(new_entry, &clone_template_assertion_predicate);
  }

  TemplateAssertionPredicateNode* create_templates_at_loop(CountedLoopNode* target_loop_head,
                                                           NodeInTargetLoop* node_in_target_loop);

  void update_templates(Node* new_init, Node* new_stride);
};

// This class creates a new Initialized Assertion Predicate.
class InitializedAssertionPredicate {
  IdealLoopTree* _outer_target_loop;
  PhaseIdealLoop* _phase;

  IfNode* create_if_node(TemplateAssertionPredicateNode* template_assertion_predicate, Node* new_ctrl, BoolNode* new_bool,
                         AssertionPredicateType assertion_predicate_type) const;
  IfTrueNode* create_if_node_out_nodes(IfNode* if_node);
  void create_halt_node(IfFalseNode* fail_proj);
  static bool has_opaque(const Node* predicate_proj);
  static bool has_halt(const Node* success_proj);

 public:
  InitializedAssertionPredicate(IdealLoopTree* outer_target_loop)
      : _outer_target_loop(outer_target_loop),
        _phase(outer_target_loop->_phase) {}

  IfTrueNode* create(TemplateAssertionPredicateNode* template_assertion_predicate, Node* new_ctrl, BoolNode* new_bool,
                     AssertionPredicateType assertion_predicate_type) {
    IfNode* if_node = create_if_node(template_assertion_predicate, new_ctrl, new_bool, assertion_predicate_type);
    return create_if_node_out_nodes(if_node);
  }

  static bool is_success_proj(const Node* success_proj);
};

class InitializedInitValueAssertionPredicate : public StackObj {
  InitializedAssertionPredicate _initialized_assertion_predicate;
  ReplaceOpaqueLoopInit _replace_opaque_loop_init;
  PhaseIdealLoop* _phase;

 public:
  InitializedInitValueAssertionPredicate(Node* new_init, IdealLoopTree* outer_target_loop)
      : _initialized_assertion_predicate(outer_target_loop),
        // Skip CastII from pre/main/post
        _replace_opaque_loop_init(new_init->uncast()),
        _phase(outer_target_loop->_phase) {}

  IfTrueNode* create_from(Node* new_ctrl, TemplateAssertionPredicateNode* template_assertion_predicate) {
    BoolNode* template_init_value_bool = template_assertion_predicate->in(TemplateAssertionPredicateNode::InitValue)->as_Bool();
    TemplateAssertionPredicateBool template_assertion_predicate_bool(template_init_value_bool, _phase);
    BoolNode* new_bool = template_assertion_predicate_bool.create(new_ctrl, &_replace_opaque_loop_init);
    return _initialized_assertion_predicate.create(template_assertion_predicate, new_ctrl, new_bool,
                                                   AssertionPredicateType::Init_value);
  }
};

class InitializedLastValueAssertionPredicate : public StackObj {
  InitializedAssertionPredicate _initialized_assertion_predicate;
  ReplaceOpaqueLoopInitAndStride _replace_opaque_loop_init_and_stride;
  PhaseIdealLoop* _phase;
 public:
  InitializedLastValueAssertionPredicate(Node* new_init, Node* new_stride,IdealLoopTree* outer_target_loop)
      : _initialized_assertion_predicate(outer_target_loop),
        // Skip CastII from pre/main/post
        _replace_opaque_loop_init_and_stride(new_init->uncast(), new_stride),
        _phase(outer_target_loop->_phase) {}


  IfTrueNode* create_from(Node* new_ctrl, TemplateAssertionPredicateNode* template_assertion_predicate) {
    BoolNode* template_last_value_bool = template_assertion_predicate->in(TemplateAssertionPredicateNode::LastValue)->as_Bool();
    TemplateAssertionPredicateBool template_assertion_predicate_bool(template_last_value_bool, _phase);
    BoolNode* new_bool = template_assertion_predicate_bool.create(new_ctrl, &_replace_opaque_loop_init_and_stride);
    return _initialized_assertion_predicate.create(template_assertion_predicate, new_ctrl, new_bool,
                                                   AssertionPredicateType::Last_value);
  }
};

// This class creates Initialized Assertion Predicates from Template Assertion Predicates at a loop.
class InitializedAssertionPredicates {
  PhaseIdealLoop* _phase;
  Node* _block_entry;
  InitializedInitValueAssertionPredicate _initialized_init_value_assertion_predicate;
  InitializedLastValueAssertionPredicate _initialized_last_value_assertion_predicate;

 public:
  InitializedAssertionPredicates(Node* new_init, Node* new_stride, Node* block_entry, IdealLoopTree* outer_target_loop)
      : _phase(outer_target_loop->_phase),
        _block_entry(block_entry),
        // Skip over a possible cast node added by PhaseIdealLoop::cast_incr_before_loop().
        _initialized_init_value_assertion_predicate(new_init->uncast(), outer_target_loop),
        _initialized_last_value_assertion_predicate(new_init->uncast(), new_stride, outer_target_loop) {
  }

  IfTrueNode* create(const TemplateAssertionPredicateBlock* template_assertion_predicate_block);
};

// Class to create Assertion Predicates at the target loop by moving the templates from the source to the target loop
// and creating initialized predicates from them.
class AssertionPredicates {
  CountedLoopNode* _source_loop_head;
  Predicates _source_loop_predicates;
  IdealLoopTree* _loop;
  IdealLoopTree* _outer_target_loop;
  PhaseIdealLoop* _phase;

  TemplateAssertionPredicateNode* create_templates_at_loop(CountedLoopNode* target_loop_head,
                                                           NodeInTargetLoop* node_in_target_loop);

  Node* create_stride(int stride_con);
  void update_templates(int new_stride_con);

 public:
  AssertionPredicates(CountedLoopNode* source_loop_head, IdealLoopTree* loop)
      : _source_loop_head(source_loop_head),
        _source_loop_predicates(source_loop_head->skip_strip_mined()->in(LoopNode::EntryControl)),
        _loop(loop),
        _outer_target_loop(loop->head()->as_CountedLoop()->is_strip_mined() ? loop->_parent->_parent : loop->_parent),
        _phase(loop->_phase) {}

  bool has_any() const {
    return _source_loop_predicates.template_assertion_predicate_block()->has_any();
  }

  void create_at_target_loop(CountedLoopNode* target_loop_head, NodeInTargetLoop* node_in_target_loop);
  void replace_to(CountedLoopNode* target_loop_head, NodeInTargetLoop* node_in_target_loop);
  void create_at_source_loop(int if_opcode, int scale, Node* offset, Node* range, bool negate);
  void update_at_source_loop(int new_stride_con);

  IfTrueNode* initialize_templates_at_loop(CountedLoopNode* target_loop_head, Node* initial_target_loop_entry,
                                           const TemplateAssertionPredicateBlock* template_assertion_predicate_block) const;
};

// Class to create bool nodes for a new Template Assertion Predicate.
class TemplateAssertionPredicateBools : public StackObj {
  IdealLoopTree* _loop;
  PhaseIdealLoop* _phase;
  CountedLoopNode* _loop_head;
  jint _stride;
  int _scale;
  Node* _offset;
  Node* _range;
  bool _upper;
  bool _negate;

  Node* create_last_value(Node* new_ctrl, OpaqueLoopInitNode* opaque_init);

 public:
  TemplateAssertionPredicateBools(IdealLoopTree* loop, int scale, Node* offset, Node* range, bool negate)
      : _loop(loop),
        _phase(loop->_phase),
        _loop_head(loop->_head->as_CountedLoop()),
        _stride(_loop_head->stride()->get_int()),
        _scale(scale),
        _offset(offset),
        _range(range),
        _upper((_stride > 0) != (_scale > 0)),  // Make sure, rc_predicate() chooses "scale*init + offset" case.
        _negate(negate) {}

  BoolNode* create_for_init_value(Node* new_ctrl, OpaqueLoopInitNode* opaque_loop_init, bool& overflow) {
    return _phase->rc_predicate(_loop, new_ctrl, _scale, _offset, opaque_loop_init, nullptr, _stride, _range, _upper,
                                overflow,_negate);
  }

  BoolNode* create_for_last_value(Node* new_ctrl, OpaqueLoopInitNode* opaque_init, bool& overflow) {
    Node* last_value = create_last_value(new_ctrl, opaque_init);
    return _phase->rc_predicate(_loop, new_ctrl, _scale, _offset, last_value, nullptr, _stride, _range, _upper,
                                overflow,_negate);
  }
};

// Predicate iterator to skip over predicates. This iterator can be used with any predicate node within a chain of
// predicates.
class PredicatesIterator : public StackObj {
  Node* _current;

 public:
  PredicatesIterator(Node* start) : _current(start) {}

  bool is_predicate() const;
  Node* skip();
};

#endif // SHARE_OPTO_PREDICATES_HPP
