/*
 * Copyright (c) 2006, 2023, Oracle and/or its affiliates. All rights reserved.
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

#include "precompiled.hpp"
#include "memory/allocation.inline.hpp"
#include "opto/connode.hpp"
#include "opto/convertnode.hpp"
#include "opto/loopnode.hpp"
#include "opto/opaquenode.hpp"
#include "opto/predicates.hpp"
#include "opto/rootnode.hpp"

//================= Loop Unswitching =====================
//
// orig:                       transformed:
//                               if (invariant-test) then
//  predicates                     predicates
//  loop                           loop
//    stmt1                          stmt1
//    if (invariant-test) then       stmt2
//      stmt2                        stmt4
//    else                         endloop
//      stmt3                    else
//    endif                        predicates [clone]
//    stmt4                        loop [clone]
//  endloop                          stmt1 [clone]
//                                   stmt3
//                                   stmt4 [clone]
//                                 endloop
//                               endif
//
// Note: the "else" clause may be empty

//------------------------------policy_unswitching-----------------------------
// Return TRUE or FALSE if the loop should be unswitched
// (ie. clone loop with an invariant test that does not exit the loop)
bool IdealLoopTree::policy_unswitching( PhaseIdealLoop *phase ) const {
  if (!LoopUnswitching) {
    return false;
  }
  if (!_head->is_Loop()) {
    return false;
  }

  // If nodes are depleted, some transform has miscalculated its needs.
  assert(!phase->exceeding_node_budget(), "sanity");

  // check for vectorized loops, any unswitching was already applied
  if (_head->is_CountedLoop() && _head->as_CountedLoop()->is_unroll_only()) {
    return false;
  }

  LoopNode* head = _head->as_Loop();
  if (head->unswitch_count() + 1 > head->unswitch_max()) {
    return false;
  }
  if (phase->find_unswitching_candidate(this) == nullptr) {
    return false;
  }

  // Too speculative if running low on nodes.
  return phase->may_require_nodes(est_loop_clone_sz(2));
}

//------------------------------find_unswitching_candidate-----------------------------
// Find candidate "if" for unswitching
IfNode* PhaseIdealLoop::find_unswitching_candidate(const IdealLoopTree *loop) const {

  // Find first invariant test that doesn't exit the loop
  LoopNode *head = loop->_head->as_Loop();
  IfNode* unswitch_iff = nullptr;
  Node* n = head->in(LoopNode::LoopBackControl);
  while (n != head) {
    Node* n_dom = idom(n);
    if (n->is_Region()) {
      if (n_dom->is_If()) {
        IfNode* iff = n_dom->as_If();
        if (iff->in(1)->is_Bool()) {
          BoolNode* bol = iff->in(1)->as_Bool();
          if (bol->in(1)->is_Cmp()) {
            // If condition is invariant and not a loop exit,
            // then found reason to unswitch.
            if (loop->is_invariant(bol) && !loop->is_loop_exit(iff)) {
              unswitch_iff = iff;
            }
          }
        }
      }
    }
    n = n_dom;
  }
  return unswitch_iff;
}

//------------------------------do_unswitching-----------------------------
// Clone loop with an invariant test (that does not exit) and
// insert a clone of the test that selects which version to
// execute.
void PhaseIdealLoop::do_unswitching(IdealLoopTree *loop, Node_List &old_new) {
  // Find first invariant test that doesn't exit the loop
  IfNode* unswitch_candidate_iff = find_unswitching_candidate((const IdealLoopTree *)loop);
  assert(unswitch_candidate_iff != nullptr, "should be at least one");

  LoopNode* head = loop->_head->as_Loop();
#ifndef PRODUCT
  if (TraceLoopOpts) {
    tty->print("Unswitch   %d ", head->unswitch_count()+1);
    loop->dump_head();
  }
#endif

  // Need to revert back to normal loop
  if (head->is_CountedLoop() && !head->as_CountedLoop()->is_normal_loop()) {
    head->as_CountedLoop()->set_normal_loop();
  }

  IfNode* invar_iff = create_slow_version_of_loop(loop, old_new, unswitch_candidate_iff, CloneIncludesStripMined);
  ProjNode* proj_true = invar_iff->proj_out(1);
  verify_fast_loop(head, proj_true);

// Increment unswitch count
  LoopNode* head_clone = old_new[head->_idx]->as_Loop();
  int nct = head->unswitch_count() + 1;
  head->set_unswitch_count(nct);
  head_clone->set_unswitch_count(nct);

  // Hoist invariant casts out of each loop to the appropriate
  // control projection.

  Node_List worklist;
  for (DUIterator_Fast imax, i = unswitch_candidate_iff->fast_outs(imax); i < imax; i++) {
    ProjNode* proj= unswitch_candidate_iff->fast_out(i)->as_Proj();
    // Copy to a worklist for easier manipulation
    for (DUIterator_Fast jmax, j = proj->fast_outs(jmax); j < jmax; j++) {
      Node* use = proj->fast_out(j);
      if (use->Opcode() == Op_CheckCastPP && loop->is_invariant(use->in(1))) {
        worklist.push(use);
      }
    }
    ProjNode* invar_proj = invar_iff->proj_out(proj->_con)->as_Proj();
    while (worklist.size() > 0) {
      Node* use = worklist.pop();
      Node* nuse = use->clone();
      nuse->set_req(0, invar_proj);
      _igvn.replace_input_of(use, 1, nuse);
      register_new_node(nuse, invar_proj);
      // Same for the clone
      Node* use_clone = old_new[use->_idx];
      _igvn.replace_input_of(use_clone, 1, nuse);
    }
  }

  // Hardwire the control paths in the loops into if(true) and if(false)
  _igvn.rehash_node_delayed(unswitch_candidate_iff);
  dominated_by(proj_true->as_IfProj(), unswitch_candidate_iff, false, false);

  IfNode* unswitch_iff_clone = old_new[unswitch_candidate_iff->_idx]->as_If();
  _igvn.rehash_node_delayed(unswitch_iff_clone);
  ProjNode* proj_false = invar_iff->proj_out(0);
  dominated_by(proj_false->as_IfProj(), unswitch_iff_clone, false, false);

  // Reoptimize loops
  loop->record_for_igvn();
  for(int i = loop->_body.size() - 1; i >= 0 ; i--) {
    Node *n = loop->_body[i];
    Node *n_clone = old_new[n->_idx];
    _igvn._worklist.push(n_clone);
  }

#ifndef PRODUCT
  if (TraceLoopUnswitching) {
    tty->print_cr("Loop unswitching orig: %d @ %d  new: %d @ %d",
                  head->_idx, unswitch_candidate_iff->_idx,
                  old_new[head->_idx]->_idx, unswitch_iff_clone->_idx);
  }
#endif

  C->set_major_progress();
}

class NewFastLoopParsePredicate : public NewParsePredicate {
 public:
  Node* create(PhaseIdealLoop* phase, Node* new_entry, ParsePredicateSuccessProj* old_proj) override {
    ParsePredicateNode* parse_predicate = old_proj->in(0)->as_ParsePredicate();
#ifndef PRODUCT
    if (TraceLoopPredicate) {
      tty->print("Created %d ParsePredicated for fast loop", parse_predicate->_idx);
      parse_predicate->dump();
    }
#endif
    return phase->create_new_if_for_predicate(old_proj, new_entry, parse_predicate->deopt_reason(), Op_ParsePredicate, false);
  }
};

class NewSlowLoopParsePredicate : public NewParsePredicate {
 public:
  Node* create(PhaseIdealLoop* phase, Node* new_entry, ParsePredicateSuccessProj* old_proj) override {
    ParsePredicateNode* parse_predicate = old_proj->in(0)->as_ParsePredicate();
#ifndef PRODUCT
    if (TraceLoopPredicate) {
      tty->print("Created %d ParsePredicated for slow loop", parse_predicate->_idx);
      parse_predicate->dump();
    }
#endif
    return phase->create_new_if_for_predicate(old_proj, new_entry, parse_predicate->deopt_reason(), Op_ParsePredicate, true);
  }
};

// Class to clone Parse Predicates from before the unswitch If to the fast and slow loop.
class UnswitchedLoopPredicates {
  LoopNode* _fast_loop_head;
  Predicates* _predicates;
  PhaseIdealLoop* _phase;
  uint _first_slow_loop_node_index;
  bool _can_clone_parse_predicates;

  // When peeling or partial peeling a loop, we could have non-CFG nodes being pinned on some CFG nodes in the peeled
  // section. If all CFG nodes in the peeled section are folded in the next IGVN phase, then the pinned non-CFG nodes
  // end up at the predicates for the original loop (either a Template Assertion Predicate, if we've created any Hoisted
  // Predicate in loop predication, or Parse Predicate otherwise). This allows some more checks to be hoisted out of the
  // fast and slow loop after loop unswitching. If we do that and any of the Hoisted Predicates for the fast or slow loop
  // fail at runtime (hoisted check is wrong at runtime), we deoptimize and jump to the start of the loop, assuming we
  // have not executed any statements, yet. This requires, that all pinned non-CFG nodes are executed AFTER the Hoisted
  // Predicates. We therefore need to update the pins to the fast and slow loop entry node (i.e. after some potentially
  // created Hoisted Predicates). This is automatically done for pinned nodes inside the loop to be unswitched, as these
  // are updated when cloning the predicates to the fast and slow loop. However, we could also have pinned non-CFG nodes
  // that are not part of the original loop and therefore not cloned. But these could be a direct or indirect inputs to
  // nodes inside the loop to be unswitched. We would need to clone all of them to be able to update the pin to the fast
  // and slow loop. This is currently not supported. We therefore must bail out of loop unswitching until this is fixed.
  bool has_no_outside_loop_data_dependencies_from_entry(Node* original_entry) const {
    const uint entry_outcnt = original_entry->outcnt();
    assert(entry_outcnt >= 3, "must have at least unswitch If, fast, and slow loop head after cloning the loop");
    if (entry_outcnt > 3 && _predicates->regular_predicate_blocks()->has_any()) {
      // For each data out node:
      // Check if there is a slow node <-> fast node mapping (i.e. the node was part of the original loop to be unswitched)
      // If we find a node without such a mapping, then it was not part of the original loop to be unswitched.
      // In this case, we found a data dependency which disallows the cloning of Parse Predicates.
      // Instead of actually mapping nodes, we can just count the number of slow loop nodes (which can only be mapped
      // to a unique fast loop node), including the loop header nodes and multiply it by 2. This should be equal to the
      // number of out nodes of 'original_entry' minus one for the unswitch If.
      const uint slow_loop_node_count = count_slow_loop_nodes(original_entry);
      return slow_loop_node_count * 2 == entry_outcnt - 1;
    }
    return true;
  }

  uint count_slow_loop_nodes(const Node* original_entry) const {
    uint slow_loop_node_count = 0;
    for (DUIterator_Fast imax, i = original_entry->fast_outs(imax); i < imax; i++) {
      Node* out = original_entry->fast_out(i);
      if (out->_idx >= _first_slow_loop_node_index) {
        slow_loop_node_count++;
      }
    }
    return slow_loop_node_count;
  }

  Node* clone_parse_predicates_to_loop(Node* new_entry, NewParsePredicate* new_parse_predicate) const {
    if (_can_clone_parse_predicates) {
      CloneParsePredicates clone_parse_predicates(new_parse_predicate, new_entry, _phase, _fast_loop_head);
      return clone_parse_predicates.clone(_predicates->regular_predicate_blocks());
    }
    return new_entry;
  }

 public:
  UnswitchedLoopPredicates(LoopNode* fast_loop_head, Predicates* predicates, PhaseIdealLoop* phase,
                           Node* original_entry, uint first_slow_loop_node_index)
      : _fast_loop_head(fast_loop_head),
        _predicates(predicates),
        _phase(phase),
        _first_slow_loop_node_index(first_slow_loop_node_index),
        _can_clone_parse_predicates(has_no_outside_loop_data_dependencies_from_entry(original_entry)) {}

  // Clone the predicates from the original loop (if there are any) to the fast loop and add them below 'new_entry'.
  // Return the last node in the newly created predicate node chain which is the new entry to the fast loop.
  Node* clone_parse_predicates_to_fast_loop(Node* new_entry) const {
    NewFastLoopParsePredicate new_fast_loop_parse_predicate;
    return clone_parse_predicates_to_loop(new_entry, &new_fast_loop_parse_predicate);
  }

  // Clone the predicates from the original loop (if there are any) to the slow loop and add them below 'new_entry'.
  // Return the last node in the newly created predicate node chain which is the new entry to the slow loop.
  Node* clone_parse_predicates_to_slow_loop(Node* new_entry) const {
    NewSlowLoopParsePredicate new_slow_loop_parse_predicate;
    return clone_parse_predicates_to_loop(new_entry, &new_slow_loop_parse_predicate);
  }

  // Move the old Template Assertion Predicates to the fast loop (which is now below the unswitch If).
  // Return the last Template Assertion Predicate.
  Node* move_template_assertion_predicates_to_fast_loop(Node* new_entry) const {
    TemplateAssertionPredicateNode* first = _predicates->template_assertion_predicate_block()->first();
    assert(first != nullptr, "must be set");
    _phase->igvn().replace_input_of(first, 0, new_entry);
    _phase->set_idom(first, new_entry, _phase->dom_depth(new_entry));
    return _predicates->template_assertion_predicate_block()->last();
  }

  // Clone the Template Assertion Predicates and return the clone that represents the last Template Assertion Predicate.
  Node* clone_template_assertion_predicates_to_slow_loop(Node* new_entry) const {
    DataOutputInClonedLoop data_output_in_cloned_loop(_first_slow_loop_node_index);
    CloneTemplateAssertionPredicates clone_template_assertion_predicates(_phase, &data_output_in_cloned_loop);
    return clone_template_assertion_predicates.clone(*_predicates, new_entry);
  }
};

//-------------------------create_slow_version_of_loop------------------------
// Create a slow version of the loop by cloning the loop
// and inserting an if to select fast-slow versions.
// Return the inserted if.
IfNode* PhaseIdealLoop::create_slow_version_of_loop(IdealLoopTree *loop, Node_List &old_new,
                                                    IfNode* unswitch_candidate_iff, CloneLoopMode mode) {
  LoopNode* head  = loop->_head->as_Loop();
  head->verify_strip_mined(1);
  Node* original_entry = head->skip_strip_mined()->in(LoopNode::EntryControl);
  _igvn.rehash_node_delayed(original_entry);
  IdealLoopTree* outer_loop = loop->_parent;

  Predicates predicates(original_entry);
  const TemplateAssertionPredicateBlock* template_assertion_predicate_block =
      predicates.template_assertion_predicate_block();
  Node* entry = template_assertion_predicate_block->entry();

  // Add test to new "if" outside of loop
  Node *bol = unswitch_candidate_iff->in(1)->as_Bool();
  IfNode* new_unswitch_iff =
      (unswitch_candidate_iff->Opcode() == Op_RangeCheck) ?
          new RangeCheckNode(entry, bol, unswitch_candidate_iff->_prob, unswitch_candidate_iff->_fcnt) :
          new IfNode(entry, bol, unswitch_candidate_iff->_prob, unswitch_candidate_iff->_fcnt);
  register_node(new_unswitch_iff, outer_loop, entry, dom_depth(entry));
  IfProjNode* if_fast = new IfTrueNode(new_unswitch_iff);
  register_node(if_fast, outer_loop, new_unswitch_iff, dom_depth(new_unswitch_iff));
  IfProjNode* if_slow = new IfFalseNode(new_unswitch_iff);
  register_node(if_slow, outer_loop, new_unswitch_iff, dom_depth(new_unswitch_iff));

  // Clone the loop body.  The clone becomes the slow loop.  The
  // original pre-header will (illegally) have 3 control users
  // (old & new loops & new if).
  LoopNode* fast_loop_head = head->skip_strip_mined();
  const uint first_slow_loop_node_index = C->unique();
  clone_loop(loop, old_new, dom_depth(fast_loop_head), mode, new_unswitch_iff);
  assert(old_new[head->_idx]->is_Loop(), "" );

  const UnswitchedLoopPredicates unswitched_loop_predicates(fast_loop_head, &predicates,
                                                            this, original_entry, first_slow_loop_node_index);
  Node* if_fast_loop_entry = unswitched_loop_predicates.clone_parse_predicates_to_fast_loop(if_fast);
  Node* if_slow_loop_entry = unswitched_loop_predicates.clone_parse_predicates_to_slow_loop(if_slow);

  if (template_assertion_predicate_block->has_any()) {
    // Move templates below
    if_fast_loop_entry =
        unswitched_loop_predicates.move_template_assertion_predicates_to_fast_loop(if_fast_loop_entry);
    if_slow_loop_entry =
        unswitched_loop_predicates.clone_template_assertion_predicates_to_slow_loop(if_slow_loop_entry);
  }

  _igvn.replace_input_of(fast_loop_head, LoopNode::EntryControl, if_fast_loop_entry);
  set_idom(fast_loop_head, if_fast_loop_entry, dom_depth(fast_loop_head));
  LoopNode* slow_loop_head = old_new[head->_idx]->as_Loop()->skip_strip_mined();
  _igvn.replace_input_of(slow_loop_head, LoopNode::EntryControl, if_slow_loop_entry);
  set_idom(slow_loop_head, if_slow_loop_entry, dom_depth(fast_loop_head));

  recompute_dom_depth();
  return new_unswitch_iff;
}

#ifdef ASSERT
void PhaseIdealLoop::verify_fast_loop(LoopNode* head, const ProjNode* proj_true) const {
  assert(proj_true->is_IfTrue(), "must be true projection");
  Node* entry = head->skip_strip_mined()->in(LoopNode::EntryControl);
  Predicates predicates(entry);
  if (!predicates.has_any()) {
    // No Parse Predicate.
    Node* uniqc = proj_true->unique_ctrl_out();
    assert((uniqc == head && !head->is_strip_mined()) || (uniqc == head->in(LoopNode::EntryControl)
                                                          && head->is_strip_mined()), "must hold by construction if no predicates");
  } else {
    // There is at least one Parse Predicate. When skipping all predicates/predicate blocks, we should end up at 'proj_true'.
    assert(proj_true == predicates.entry(),
           "must hold by construction if at least one Parse Predicate");
  }
}
#endif // ASSERT

LoopNode* PhaseIdealLoop::create_reserve_version_of_loop(IdealLoopTree *loop, CountedLoopReserveKit* lk) {
  Node_List old_new;
  LoopNode* head  = loop->_head->as_Loop();
  Node*     entry = head->skip_strip_mined()->in(LoopNode::EntryControl);
  _igvn.rehash_node_delayed(entry);
  IdealLoopTree* outer_loop = head->is_strip_mined() ? loop->_parent->_parent : loop->_parent;

  ConINode* const_1 = _igvn.intcon(1);
  set_ctrl(const_1, C->root());
  IfNode* iff = new IfNode(entry, const_1, PROB_MAX, COUNT_UNKNOWN);
  register_node(iff, outer_loop, entry, dom_depth(entry));
  ProjNode* iffast = new IfTrueNode(iff);
  register_node(iffast, outer_loop, iff, dom_depth(iff));
  ProjNode* ifslow = new IfFalseNode(iff);
  register_node(ifslow, outer_loop, iff, dom_depth(iff));

  // Clone the loop body.  The clone becomes the slow loop.  The
  // original pre-header will (illegally) have 3 control users
  // (old & new loops & new if).
  clone_loop(loop, old_new, dom_depth(head), CloneIncludesStripMined, iff);
  assert(old_new[head->_idx]->is_Loop(), "" );

  LoopNode* slow_head = old_new[head->_idx]->as_Loop();

#ifndef PRODUCT
  if (TraceLoopOpts) {
    tty->print_cr("PhaseIdealLoop::create_reserve_version_of_loop:");
    tty->print("\t iff = %d, ", iff->_idx); iff->dump();
    tty->print("\t iffast = %d, ", iffast->_idx); iffast->dump();
    tty->print("\t ifslow = %d, ", ifslow->_idx); ifslow->dump();
    tty->print("\t before replace_input_of: head = %d, ", head->_idx); head->dump();
    tty->print("\t before replace_input_of: slow_head = %d, ", slow_head->_idx); slow_head->dump();
  }
#endif

  // Fast (true) control
  _igvn.replace_input_of(head->skip_strip_mined(), LoopNode::EntryControl, iffast);
  // Slow (false) control
  _igvn.replace_input_of(slow_head->skip_strip_mined(), LoopNode::EntryControl, ifslow);

  recompute_dom_depth();

  lk->set_iff(iff);

#ifndef PRODUCT
  if (TraceLoopOpts ) {
    tty->print("\t after  replace_input_of: head = %d, ", head->_idx); head->dump();
    tty->print("\t after  replace_input_of: slow_head = %d, ", slow_head->_idx); slow_head->dump();
  }
#endif

  return slow_head->as_Loop();
}

CountedLoopReserveKit::CountedLoopReserveKit(PhaseIdealLoop* phase, IdealLoopTree *loop, bool active = true) :
  _phase(phase),
  _lpt(loop),
  _lp(nullptr),
  _iff(nullptr),
  _lp_reserved(nullptr),
  _has_reserved(false),
  _use_new(false),
  _active(active)
  {
    create_reserve();
  };

CountedLoopReserveKit::~CountedLoopReserveKit() {
  if (!_active) {
    return;
  }

  if (_has_reserved && !_use_new) {
    // intcon(0)->iff-node reverts CF to the reserved copy
    ConINode* const_0 = _phase->_igvn.intcon(0);
    _phase->set_ctrl(const_0, _phase->C->root());
    _iff->set_req(1, const_0);

    #ifndef PRODUCT
      if (TraceLoopOpts) {
        tty->print_cr("CountedLoopReserveKit::~CountedLoopReserveKit()");
        tty->print("\t discard loop %d and revert to the reserved loop clone %d: ", _lp->_idx, _lp_reserved->_idx);
        _lp_reserved->dump();
      }
    #endif
  }
}

bool CountedLoopReserveKit::create_reserve() {
  if (!_active) {
    return false;
  }

  if(!_lpt->_head->is_CountedLoop()) {
    if (TraceLoopOpts) {
      tty->print_cr("CountedLoopReserveKit::create_reserve: %d not counted loop", _lpt->_head->_idx);
    }
    return false;
  }
  CountedLoopNode *cl = _lpt->_head->as_CountedLoop();
  if (!cl->is_valid_counted_loop(T_INT)) {
    if (TraceLoopOpts) {
      tty->print_cr("CountedLoopReserveKit::create_reserve: %d not valid counted loop", cl->_idx);
    }
    return false; // skip malformed counted loop
  }
  if (!cl->is_main_loop()) {
    bool loop_not_canonical = true;
    if (cl->is_post_loop() && (cl->slp_max_unroll() > 0)) {
      loop_not_canonical = false;
    }
    // only reject some loop forms
    if (loop_not_canonical) {
      if (TraceLoopOpts) {
        tty->print_cr("CountedLoopReserveKit::create_reserve: %d not canonical loop", cl->_idx);
      }
      return false; // skip normal, pre, and post (conditionally) loops
    }
  }

  _lp = _lpt->_head->as_Loop();
  _lp_reserved = _phase->create_reserve_version_of_loop(_lpt, this);

  if (!_lp_reserved->is_CountedLoop()) {
    return false;
  }

  Node* ifslow_pred = _lp_reserved->skip_strip_mined()->in(LoopNode::EntryControl);

  if (!ifslow_pred->is_IfFalse()) {
    return false;
  }

  Node* iff = ifslow_pred->in(0);
  if (!iff->is_If() || iff != _iff) {
    return false;
  }

  if (iff->in(1)->Opcode() != Op_ConI) {
    return false;
  }

  _has_reserved = true;
  return true;
}
