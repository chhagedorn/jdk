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
  IfNode* unswitching_candidate = find_unswitching_candidate((const IdealLoopTree *)loop);
  assert(unswitching_candidate != nullptr, "should be at least one");

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

  IfNode* invar_iff = create_slow_version_of_loop(loop, old_new, unswitching_candidate);
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
  for (DUIterator_Fast imax, i = unswitching_candidate->fast_outs(imax); i < imax; i++) {
    ProjNode* proj= unswitching_candidate->fast_out(i)->as_Proj();
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
  _igvn.rehash_node_delayed(unswitching_candidate);
  dominated_by(proj_true->as_IfProj(), unswitching_candidate, false, false);

  IfNode* unswitch_iff_clone = old_new[unswitching_candidate->_idx]->as_If();
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
                  head->_idx, unswitching_candidate->_idx,
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

// Class to create the unswitch If that decides if the slow or fast loop must be executed.
class UnswitchIf {
  IdealLoopTree* _outer_loop;
  Node* _original_loop_entry;
  PhaseIdealLoop* _phase;

 public:
  UnswitchIf(IdealLoopTree* loop)
      : _outer_loop(loop->_head->as_Loop()->is_strip_mined() ? loop->_parent->_parent : loop->_parent),
        _original_loop_entry(loop->_head->as_Loop()->skip_strip_mined()->in(LoopNode::EntryControl)),
        _phase(loop->_phase) {}

  IfNode* create(IfNode* unswitching_candidate) {
    const uint dom_depth = _phase->dom_depth(_original_loop_entry);
    _phase->igvn().rehash_node_delayed(_original_loop_entry);
    Node* unswitching_candidate_bool = unswitching_candidate->in(1)->as_Bool();
    IfNode* unswitch_if =
        (unswitching_candidate->Opcode() == Op_RangeCheck) ?
        new RangeCheckNode(_original_loop_entry, unswitching_candidate_bool, unswitching_candidate->_prob,
                           unswitching_candidate->_fcnt) :
        new IfNode(_original_loop_entry, unswitching_candidate_bool, unswitching_candidate->_prob,
                   unswitching_candidate->_fcnt);
    _phase->register_node(unswitch_if, _outer_loop, _original_loop_entry, dom_depth);
    IfProjNode* if_fast = new IfTrueNode(unswitch_if);
    _phase->register_node(if_fast, _outer_loop, unswitch_if, dom_depth);
    IfProjNode* if_slow = new IfFalseNode(unswitch_if);
    _phase->register_node(if_slow, _outer_loop, unswitch_if, dom_depth);
    return unswitch_if;
  }
};

// Class to clone the Parse Predicates from the original loop to be unswitched to the fast and slow loop.
class UnswitchedParsePredicates {
  LoopNode* _fast_loop_head; // The original loop becomes the fast loop.
  Node* _original_loop_entry;
  const Predicates* _predicates;
  PhaseIdealLoop* _phase;
  uint _first_slow_loop_node_index;

 public:
  UnswitchedParsePredicates(IdealLoopTree* loop, const Predicates* predicates, uint first_slow_loop_node_index)
     : _fast_loop_head(loop->_head->as_Loop()->skip_strip_mined()),
       _original_loop_entry(loop->_head->as_Loop()->skip_strip_mined()->in(LoopNode::EntryControl)),
       _predicates(predicates),
       _phase(loop->_phase),
       _first_slow_loop_node_index(first_slow_loop_node_index) {}

  // Check if there are any Parse Predicates to clone and if the current loop entry has any outside the loop body
  // non-CFG dependencies. If that is the case, we cannot clone the Parse Predicates for the following reason:
  //
  // When peeling or partial peeling a loop, we could have non-CFG nodes N being pinned on some CFG nodes in the peeled
  // section. If all CFG nodes in the peeled section are folded in the next IGVN phase, then the pinned non-CFG nodes N
  // end up at the loop entry of the original loop. If we still find Parse Predicates at that point (i.e. not decided to
  // remove all Parse Predicates and give up on further Loop Predication, yet), then these Parse Predicates can be cloned
  // to the fast and slow loop when unswitching this loop at some point:
  //
  //     Parse Predicates                   Some CFG node                                Some CFG node
  //            |                                |                                     /      |       \
  //  [Assertion Predicates]*   IGVN      Parse Predicates      unswitch             N1  Unswitch If   N2
  //            |               ===>             |                ===>                  /           \
  //     peeled CFG node               [Assertion Predicates]*             Parse Predicates        Parse Predicates
  //     /      |      \                  /      |      \                         |                       |
  //    N1     loop    N2                N1     loop    N2              [Assertion Predicates]*  [Assertion Predicates]*
  //                                                                              |                       |
  //                                                                          fast loop               slow loop
  //
  // This allows some more checks to be hoisted out of the fast and slow loop (i.e. creating Hoisted Predicates between
  // the loop head and the Unswitch If). If one of these new Hoisted Predicates fail at runtime, we deoptimize and jump
  // to the start of the loop in the interpreter, assuming that no statement was executed in the loop body, yet. However,
  // the pinned statements N (N1, N2 in the figure above) could have already been executed with potentially visible side
  // effects (i.e. storing a field). This is wrong. To fix that, we must move these pinned non-CFG nodes BELOW the Hoisted
  // Predicates of the unswitched loops. This is done for nodes being part of the fast and slow loop body (i.e. part of
  // the original loop to be unswitched). But we could also have non-CFG nodes that are not part of the loop body (i.e.
  // only part of the originally peeled section). To force such a node to be executed after the newly created Hoisted
  // Predicates of the fast/slow loop, we would need to clone it such that we can move the original node to the fast loop
  // and the clone to the slow loop (or vice versa).to the slow and the fast loop. This, however, is not supported (yet).
  // Therefore, we cannot clone the Parse Predicates to the fast and slow loop in this case.
  bool can_clone() const {
    const bool has_parse_predicates = _predicates->has_parse_predicates();
    const uint entry_outcnt = _original_loop_entry->outcnt();
    assert(entry_outcnt >= 3, "must have at least unswitch If, fast, and slow loop head after cloning the loop");
    if (entry_outcnt > 3 && has_parse_predicates) {
      // For each data out node:
      // Check if there is a slow node <-> fast node mapping (i.e. the node was part of the original loop to be unswitched)
      // If we find a node without such a mapping, then it was not part of the original loop to be unswitched.
      // In this case, we found a data dependency which disallows the cloning of Parse Predicates.
      // Instead of actually mapping nodes, we can just count the number of slow loop nodes (which can only be mapped
      // to a unique fast loop node), including the loop header nodes and multiply it by 2. This should be equal to the
      // number of out nodes of 'original_entry' minus one for the unswitch If.
      const uint slow_loop_node_count = count_slow_loop_nodes();
      return slow_loop_node_count * 2 == entry_outcnt - 1;
    }
    return has_parse_predicates;
  }

  uint count_slow_loop_nodes() const {
    uint slow_loop_node_count = 0;
    for (DUIterator_Fast imax, i = _original_loop_entry->fast_outs(imax); i < imax; i++) {
      Node* out = _original_loop_entry->fast_out(i);
      if (out->_idx >= _first_slow_loop_node_index) {
        slow_loop_node_count++;
      }
    }
    return slow_loop_node_count;
  }

  // Clone the predicates from the original loop (if there are any) to the fast loop and add them below 'new_entry'.
  // Return the last node in the newly created predicate node chain which is the new entry to the fast loop.
  Node* clone_to_fast_loop(IfTrueNode* fast_loop_unswitch_if_proj) const {
    NewFastLoopParsePredicate new_fast_loop_parse_predicate;
    return clone_parse_predicates_to_loop(fast_loop_unswitch_if_proj, &new_fast_loop_parse_predicate);
  }

  // Clone the predicates from the original loop (if there are any) to the slow loop and add them below 'new_entry'.
  // Return the last node in the newly created predicate node chain which is the new entry to the slow loop.
  Node* clone_to_slow_loop(IfFalseNode* slow_loop_unswitch_if_proj) const {
    NewSlowLoopParsePredicate new_slow_loop_parse_predicate;
    return clone_parse_predicates_to_loop(slow_loop_unswitch_if_proj, &new_slow_loop_parse_predicate);
  }

  Node* clone_parse_predicates_to_loop(IfProjNode* unswitch_if_proj, NewParsePredicate* new_parse_predicate) const {
    ParsePredicates parse_predicates(new_parse_predicate, unswitch_if_proj, _phase, _fast_loop_head);
    return parse_predicates.clone(_predicates);
  }
};

// Class to clone the Template Assertion Predicates from the original loop to be unswitched to the fast and slow loop.
class UnswitchedTemplateAssertionPredicates {
  const TemplateAssertionPredicateBlock* _template_assertion_predicate_block;
  PhaseIdealLoop* _phase;
  uint _first_slow_loop_node_index;

  void rewire_templates(Node* new_entry_to_templates) const {
    TemplateAssertionPredicateNode* first = _template_assertion_predicate_block->first();
    assert(first != nullptr, "must be set");
    _phase->replace_ctrl_same_depth(first, new_entry_to_templates);
  }

  void rewire_unswitch_if(IfNode* unswitch_if_node) const {
    Node* old_templates_entry = _template_assertion_predicate_block->entry();
    _phase->replace_ctrl_same_depth(unswitch_if_node, old_templates_entry);
  }

 public:
  UnswitchedTemplateAssertionPredicates(const TemplateAssertionPredicateBlock* template_assertion_predicate_block,
                                        PhaseIdealLoop* phase, uint first_slow_loop_node_index)
      : _template_assertion_predicate_block(template_assertion_predicate_block),
        _phase(phase),
        _first_slow_loop_node_index(first_slow_loop_node_index) {}
  // Move the old Template Assertion Predicates to the fast loop (which is now below the unswitch If).
  // Return the last Template Assertion Predicate.

  bool has_any() const {
    return _template_assertion_predicate_block->has_any();
  }

  // We can directly re-use the original templates and move them to the fast loop. We can then clone them to the slow
  // loop from there. Return the last Template Assertion Predicate.
  Node* move_to_fast_loop(Node* new_entry_to_templates, IfNode* unswitch_if_node) const {
    rewire_unswitch_if(unswitch_if_node);
    rewire_templates(new_entry_to_templates);
    return _template_assertion_predicate_block->last();
  }

  // Clone the Template Assertion Predicates and return the clone that represents the last Template Assertion Predicate.
  Node* clone_to_slow_loop(Node* new_entry_to_templates) const {
    NodeInClonedLoop node_in_cloned_loop(_first_slow_loop_node_index);
    TemplateAssertionPredicates template_assertion_predicates(_template_assertion_predicate_block, _phase);
    return template_assertion_predicates.clone_templates(new_entry_to_templates, &node_in_cloned_loop);
  }
};

// Class to unswitch a loop and create predicates at the new loops. The newly cloned loop becomes the slow loop while
// the original loop becomes the fast loop.
class UnswitchedLoop {
  LoopNode* _fast_loop_head; // The original loop becomes the fast loop.
  LoopNode* _fast_loop_strip_mined_head;
  IdealLoopTree* _loop;
  Predicates _predicates;
  Node_List* _old_new;
  PhaseIdealLoop* _phase;

  // Create Parse Predicates for the fast and slow loop if possible. Return the new fast loop and slow loop entry in
  // 'fast_loop_entry' and 'slow_loop_entry'.
  void create_parse_predicates(Node*& fast_loop_entry, Node*& slow_loop_entry,
                               const uint first_slow_loop_node_index) const {
    UnswitchedParsePredicates unswitched_parse_predicates(_loop, &_predicates, first_slow_loop_node_index);
    if (unswitched_parse_predicates.can_clone()) {
      fast_loop_entry = unswitched_parse_predicates.clone_to_fast_loop(fast_loop_entry->as_IfTrue());
      slow_loop_entry = unswitched_parse_predicates.clone_to_slow_loop(slow_loop_entry->as_IfFalse());
    }
  }

  // Create Template Assertion Predicates for the fast and slow loop if possible. Return the new fast loop and slow loop
  // entry in 'fast_loop_entry' and 'slow_loop_entry'.
  void create_template_assertion_predicates(Node*& fast_loop_entry, Node*& slow_loop_entry, IfNode* unswitch_if_node,
                                            const uint first_slow_loop_node_index) const {
    const UnswitchedTemplateAssertionPredicates unswitched_template_assertion_predicates(
        _predicates.template_assertion_predicate_block(), _phase, first_slow_loop_node_index);
    if (unswitched_template_assertion_predicates.has_any()) {
      fast_loop_entry = unswitched_template_assertion_predicates.move_to_fast_loop(fast_loop_entry, unswitch_if_node);
      slow_loop_entry = unswitched_template_assertion_predicates.clone_to_slow_loop(slow_loop_entry);
    }
  }

  void fix_ctrl_of_loops(Node* fast_loop_entry, Node* slow_loop_entry) {
    _phase->replace_loop_entry(_fast_loop_strip_mined_head, fast_loop_entry);
    LoopNode* slow_loop_head = _old_new->at(_fast_loop_strip_mined_head->_idx)->as_Loop();
    _phase->replace_loop_entry(slow_loop_head, slow_loop_entry);
  }

 public:
  UnswitchedLoop(IdealLoopTree* loop, Node_List* old_new)
      : _fast_loop_head(loop->_head->as_Loop()),
        _fast_loop_strip_mined_head(_fast_loop_head->skip_strip_mined()),
        _loop(loop),
        _predicates(_fast_loop_strip_mined_head->in(LoopNode::EntryControl)),
        _old_new(old_new),
        _phase(loop->_phase) {}


  // Unswitch on the invariant unswitching candidate If. Return the new if which switches between the slow and fast loop.
  IfNode* unswitch(IfNode* unswitching_candidate) {
    UnswitchIf unswitch_if(_loop);
    IfNode* unswitch_if_node = unswitch_if.create(unswitching_candidate);
    const uint first_slow_loop_node_index = _phase->C->unique();
    _phase->clone_loop(_loop, *_old_new, _phase->dom_depth(_fast_loop_head),
                       PhaseIdealLoop::CloneIncludesStripMined, unswitch_if_node);

    Node* fast_loop_entry = unswitch_if_node->proj_out(true);
    Node* slow_loop_entry = unswitch_if_node->proj_out(false);
    create_parse_predicates(fast_loop_entry, slow_loop_entry, first_slow_loop_node_index);
    create_template_assertion_predicates(fast_loop_entry,slow_loop_entry, unswitch_if_node, first_slow_loop_node_index);

    fix_ctrl_of_loops(fast_loop_entry, slow_loop_entry);
    return unswitch_if_node;
  }
};

//-------------------------create_slow_version_of_loop------------------------
// Create a slow version of the loop by cloning the loop
// and inserting an if to select fast-slow versions.
// Return the inserted if.
IfNode* PhaseIdealLoop::create_slow_version_of_loop(IdealLoopTree* loop, Node_List& old_new,
                                                    IfNode* unswitching_candidate) {
  UnswitchedLoop unswitched_loop(loop, &old_new);
  IfNode* unswitch_if = unswitched_loop.unswitch(unswitching_candidate);
  recompute_dom_depth();
  return unswitch_if;
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
