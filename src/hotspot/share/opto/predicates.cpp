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

#include "precompiled.hpp"
#include "opto/callnode.hpp"
#include "opto/opaquenode.hpp"
#include "opto/predicates.hpp"
#include "opto/rootnode.hpp"

// Walk over all Initialized Assertion Predicates and return the entry into the first Initialized Assertion Predicate
// (i.e. not belonging to an Initialized Assertion Predicate anymore)
Node* AssertionPredicatesWithHalt::find_entry(Node* start_proj) {
  Node* entry = start_proj;
  while (is_assertion_predicate_success_proj(entry)) {
    entry = entry->in(0)->in(0);
  }
  return entry;
}

bool AssertionPredicatesWithHalt::is_assertion_predicate_success_proj(const Node* predicate_proj) {
  if (predicate_proj == nullptr || !predicate_proj->is_IfProj()) {
    return false;
  }
  return has_opaque4(predicate_proj) && has_halt(predicate_proj);
}

// Check if the If node of `predicate_proj` has an Opaque4 node as input.
bool AssertionPredicatesWithHalt::has_opaque4(const Node* predicate_proj) {
  IfNode* iff = predicate_proj->in(0)->as_If();
  return iff->in(1)->Opcode() == Op_Opaque4;
}

// Check if the other projection (UCT projection) of `success_proj` has a Halt node as output.
bool AssertionPredicatesWithHalt::has_halt(const Node* success_proj) {
  ProjNode* other_proj = success_proj->as_IfProj()->other_if_proj();
  return other_proj->outcnt() == 1 && other_proj->unique_out()->Opcode() == Op_Halt;
}

Deoptimization::DeoptReason RuntimePredicate::uncommon_trap_reason(IfProjNode* if_proj) {
    CallStaticJavaNode* uct_call = if_proj->is_uncommon_trap_if_pattern(Deoptimization::Reason_none);
    if (uct_call == nullptr) {
      return Deoptimization::Reason_none;
    }
    int req = uct_call->uncommon_trap_request();
    return Deoptimization::trap_request_reason(req);
}

bool RuntimePredicate::is_success_proj(Node* node) {
  if (node->is_IfProj()) {
    Deoptimization::DeoptReason deopt_reason = uncommon_trap_reason(node->as_IfProj());
    return (deopt_reason == Deoptimization::Reason_loop_limit_check ||
            deopt_reason == Deoptimization::Reason_predicate ||
            deopt_reason == Deoptimization::Reason_profile_predicate);
  } else {
    return false;
  }
}

bool RuntimePredicate::is_success_proj(Node* node, Deoptimization::DeoptReason deopt_reason) {
  if (node->is_IfProj()) {
    return deopt_reason == uncommon_trap_reason(node->as_IfProj());
  } else {
    return false;
  }
}

TemplateAssertionPredicateIterator::TemplateAssertionPredicateIterator(const Predicates& predicates)
    : _current(predicates.template_assertion_predicate_block()->last()) {}

TemplateAssertionPredicateNode* TemplateAssertionPredicateIterator::next() {
  assert(has_next(), "always check has_next() first");
  TemplateAssertionPredicateNode* current = _current->as_TemplateAssertionPredicate();
  _current = _current->in(0);
  return current;
}

TemplateAssertionPredicateBlock::TemplateAssertionPredicateBlock(Node* loop_entry)
    : _entry(loop_entry),
      _first(nullptr),
      _last(nullptr) {
  if (loop_entry->is_TemplateAssertionPredicate()) {
    _last = loop_entry->as_TemplateAssertionPredicate();
    TemplateAssertionPredicateIterator iterator(loop_entry);
    TemplateAssertionPredicateNode* next = _last;
    while (iterator.has_next()) {
      next = iterator.next();
    }
    _first = next;
    _entry = _first->in(0);
  }
}

ParsePredicateIterator::ParsePredicateIterator(const Predicates& predicates) : _current_index(0) {
  const RegularPredicateBlock* loop_limit_check_predicate_block = predicates.loop_limit_check_predicate_block();
  if (loop_limit_check_predicate_block->has_parse_predicate()) {
    _parse_predicates.push(loop_limit_check_predicate_block->parse_predicate());
  }
  if (UseProfiledLoopPredicate) {
    const RegularPredicateBlock* profiled_loop_predicate_block = predicates.profiled_loop_predicate_block();
    if (profiled_loop_predicate_block->has_parse_predicate()) {
      _parse_predicates.push(profiled_loop_predicate_block->parse_predicate());
    }
  }
  if (UseLoopPredicate) {
    const RegularPredicateBlock* loop_predicate_block = predicates.loop_predicate_block();
    if (loop_predicate_block->has_parse_predicate()) {
      _parse_predicates.push(loop_predicate_block->parse_predicate());
    }
  }
}

ParsePredicateNode* ParsePredicateIterator::next() {
  assert(has_next(), "always check has_next() first");
  return _parse_predicates.at(_current_index++);
}

void LoopTreeIteratorLoopPredication::walk(IdealLoopTree* loop) {
  if (loop->_child) { // child
    walk(loop->_child);
  }

  if (loop->can_apply_loop_predication()) {
    _callback(loop);
  }

  if (loop->_next) { // sibling
    walk(loop->_next);
  }
}

template<typename PredicateNode, typename Iterator> void UsefulPredicateMarker::mark(const Predicates& predicates) {
  Iterator iterator(predicates);
  while (iterator.has_next()) {
    iterator.next()->mark_useful();
  }
}

template<typename Predicate_List>
void EliminateUselessPredicates::mark_predicates_useless(Predicate_List predicate_list) {
  for (int i = 0; i < predicate_list.length(); i++) {
    predicate_list.at(i)->mark_useless();
  }
}

template<typename Predicate_List>
void EliminateUselessPredicates::add_useless_predicates_to_igvn(Predicate_List predicate_list) {
  for (int i = 0; i < predicate_list.length(); i++) {
    auto predicate_node = predicate_list.at(i);
    if (predicate_node->is_useless()) {
      _igvn->_worklist.push(predicate_node);
    }
  }
}

void EliminateUselessPredicates::eliminate() {
  mark_predicates_useless(C->parse_predicates());
  mark_predicates_useless(C->template_assertion_predicates());

  LoopTreeIteratorLoopPredication iterator(C, _ltree_root, UsefulPredicateMarker::mark_predicates);
  iterator.walk();
  add_useless_predicates_to_igvn(C->parse_predicates());
  add_useless_predicates_to_igvn(C->template_assertion_predicates());
}

void CloneParsePredicates::clone_parse_predicate(const RegularPredicateBlock* regular_predicate_block) {
  if (!regular_predicate_block->is_empty()) {
    ParsePredicateSuccessProj* parse_predicate_proj = regular_predicate_block->parse_predicate_success_proj();
    create_new_parse_predicate(parse_predicate_proj);
  }
}

void CloneParsePredicates::create_new_parse_predicate(ParsePredicateSuccessProj* predicate_proj) {
  _new_entry = _new_parse_predicate->create(_phase, _new_entry, predicate_proj);
  ParsePredicateNode* parse_predicate = _new_entry->in(0)->as_ParsePredicate();
  _phase->igvn().hash_delete(parse_predicate);
}

// Clone the Parse Predicates from the Predicate Blocks to this loop and add them below '_new_entry'.
// Return the last node in the newly created Parse Predicate chain.
Node* CloneParsePredicates::clone(const RegularPredicateBlocks* regular_predicate_blocks) {
  clone_parse_predicate(regular_predicate_blocks->loop_predicate_block());
  clone_parse_predicate(regular_predicate_blocks->profiled_loop_predicate_block());
  if (!_loop_node->is_CountedLoop()) {
    // Don't clone the Loop Limit Check Parse Predicate if we already have a counted loop (a Loop Limit Check Predicate
    // is only created when converting a LoopNode to a CountedLoopNode).
    clone_parse_predicate(regular_predicate_blocks->loop_limit_check_predicate_block());
  }
  return _new_entry;
}

// Creates new Template Assertion Predicates below '_new_entry' on the basis of existing Template Assertion Predicates
// found in 'predicates'. Returns the last node in the newly created Template Assertion Predicate chain.
Node* CreateTemplateAssertionPredicates::create(const Predicates& predicates) {
  TemplateAssertionPredicateIterator template_assertion_predicate_iterator(predicates);
  TemplateAssertionPredicateNode* previous_template_assertion_predicate = nullptr;
  while (template_assertion_predicate_iterator.has_next()) {
    TemplateAssertionPredicateNode* template_assertion_predicate = template_assertion_predicate_iterator.next();
    TemplateAssertionPredicateNode* new_template_assertion_predicate = create_from(template_assertion_predicate);
    if (previous_template_assertion_predicate != nullptr) {
      _phase->igvn().replace_input_of(previous_template_assertion_predicate, 0, new_template_assertion_predicate);
    }
    previous_template_assertion_predicate = new_template_assertion_predicate;
  }
  return _last_template_assertion_predicate;
}

TemplateAssertionPredicateNode* CreateTemplateAssertionPredicates::create_from(
    TemplateAssertionPredicateNode* template_assertion_predicate) {
  TemplateAssertionPredicateNode* clone =
      _create_template_assertion_predicate->create_from(template_assertion_predicate, _new_entry);

  if (_last_template_assertion_predicate == nullptr) {
    _last_template_assertion_predicate = clone;
  }
  return clone;
}

// Check if 'n' belongs to the init or last value Template Assertion Predicate bool, including the OpaqueLoop* nodes.
bool TemplateAssertionPredicateBool::is_related_node(Node* n) {
  ResourceMark rm;
  Unique_Node_List list;
  list.push(n);
  for (uint i = 0; i < list.size(); i++) {
    Node* next = list.at(i);
    const int opcode = next->Opcode();
    if (opcode == Op_OpaqueLoopInit || opcode == Op_OpaqueLoopStride) {
      return true;
    } else {
      push_inputs_if_related_node(list, next);
    }
  }
  return false;
}

// Create a new Bool node from the provided Template Assertion Predicate.
// Replace found OpaqueLoop* nodes with new_init and new_stride, respectively, if non-null.
// All newly cloned (non-CFG) nodes will get 'ctrl' as new ctrl.
BoolNode* TemplateAssertionPredicateBool::create(Node* ctrl) {
  Node_Stack to_clone(2);
  to_clone.push(_bol, 1);
  const uint idx_before_cloning = C->unique();
  Node* result = nullptr;
  bool found_init = false;
  // Look for the opaque node to replace with the new value
  // and clone everything in between. We keep the Opaque4 node
  // so the duplicated predicates are eliminated once loop
  // opts are over: they are here only to keep the IR graph
  // consistent.
  do {
    Node* n = to_clone.node();
    const uint i = to_clone.index();
    Node* input = n->in(i);
    if (could_be_related_node(input)) {
      to_clone.push(input, 1);
      continue;
    }
    if (input->is_Opaque1()) {
      if (n->_idx < idx_before_cloning) {
        n = n->clone();
        _phase->register_new_node(n, ctrl);
      }
      const int op = input->Opcode();
      if (op == Op_OpaqueLoopInit) {
        Node* replacement = _replace_opaque_loop_node->replace_init(input->as_OpaqueLoopInit(), ctrl);
        n->set_req(i, replacement);
        found_init = true;
      } else {
        Node* replacement = _replace_opaque_loop_node->replace_stride(input->as_OpaqueLoopStride(), ctrl);
        n->set_req(i, replacement);
      }
      to_clone.set_node(n);
    }
    while (true) {
      Node* cur = to_clone.node();
      uint j = to_clone.index();
      if (j+1 < cur->req()) {
        to_clone.set_index(j+1);
        break;
      }
      to_clone.pop();
      if (to_clone.size() == 0) {
        result = cur;
        break;
      }
      Node* next = to_clone.node();
      j = to_clone.index();
      if (next->in(j) != cur) {
        assert(cur->_idx >= idx_before_cloning || next->in(j)->Opcode() == Op_Opaque1, "new node or Opaque1 being replaced");
        if (next->_idx < idx_before_cloning) {
          next = next->clone();
          _phase->register_new_node(next, ctrl);
          to_clone.set_node(next);
        }
        next->set_req(j, cur);
      }
    }
  } while (result == nullptr);
  assert(result->_idx >= idx_before_cloning, "new node expected");
  assert(found_init, "OpaqueLoopInitNode must always be found");
  return result->as_Bool();
}

Node* CloneOpaqueLoopNodes::replace_init(OpaqueLoopInitNode* init, Node* ctrl) {
  return clone_old(init, ctrl);
}

Node* CloneOpaqueLoopNodes::replace_stride(OpaqueLoopStrideNode* stride, Node* ctrl) {
  return clone_old(stride, ctrl);
}

Node* ReplaceOpaqueLoopInit::replace_init(OpaqueLoopInitNode* init, Node* ctrl) {
  return _new_init;
}

Node* ReplaceOpaqueLoopInit::replace_stride(OpaqueLoopStrideNode* stride, Node* ctrl) {
  // Do nothing.
  return stride;
}

Node* ReplaceOpaqueLoopInitAndStride::replace_init(OpaqueLoopInitNode* init, Node* ctrl) {
  return _new_init;
}

Node* ReplaceOpaqueLoopInitAndStride::replace_stride(OpaqueLoopStrideNode* stride, Node* ctrl) {
  return _new_stride;
}

TemplateAssertionPredicateNode* TemplateAssertionPredicate::clone(Node* new_entry) {
  BoolNode* new_init_bool = _init_value_bool.clone(new_entry);
  BoolNode* new_last_bool = _last_value_bool.clone(new_entry);
  Node* clone = _template_assertion_predicate->clone();
  _phase->igvn().replace_input_of(clone, TemplateAssertionPredicateNode::InitValue, new_init_bool);
  _phase->igvn().replace_input_of(clone, TemplateAssertionPredicateNode::LastValue, new_last_bool);
  _phase->igvn().register_new_node_with_optimizer(clone);
  _phase->igvn().replace_input_of(clone, 0, new_entry);
  _phase->set_idom(clone, new_entry, _phase->dom_depth(new_entry));
  update_data_dependencies(clone);
  return clone->as_TemplateAssertionPredicate();
}

TemplateAssertionPredicateNode* TemplateAssertionPredicate::replace(Node* new_entry) {
  TemplateAssertionPredicateNode* template_assertion_predicate_node = clone(new_entry);
  template_assertion_predicate_node->mark_useless();
  return template_assertion_predicate_node;
}

void TemplateAssertionPredicate::update_data_dependencies(Node* new_template_assertion_predicate) {
  for (DUIterator_Fast imax, i = _template_assertion_predicate->fast_outs(imax); i < imax; i++) {
    Node* node = _template_assertion_predicate->fast_out(i);
    if (_old_template_assertion_predicate_output->should_rewire(node)) {
      _phase->igvn().replace_input_of(node, 0, new_template_assertion_predicate);
      --i;
      --imax;
    }
  }
}

TemplateAssertionPredicateNode*
ReplaceTemplateAssertionPredicate::create_from(TemplateAssertionPredicateNode* template_assertion_predicate_node,
                                               Node* new_ctrl) {
  TemplateAssertionPredicate template_assertion_predicate(template_assertion_predicate_node,
                                                          &_replace_opaque_loop_nodes, _phase,
                                                          _old_template_assertion_predicate_output);
  TemplateAssertionPredicateNode* clone = template_assertion_predicate.clone(new_ctrl);
  template_assertion_predicate_node->mark_useless();
  _phase->igvn()._worklist.push(template_assertion_predicate_node);
  return clone;
}

// Create a new If or RangeCheck node to represent an Initialized Assertion Predicate and return it.
IfNode* InitializedAssertionPredicate::create_iff(TemplateAssertionPredicateNode* template_assertion_predicate,
                                                  ReplaceOpaqueLoopNodes* replace_opaque_loop_nodes, Node* new_ctrl,
                                                  BoolNode* bol) const {
  TemplateAssertionPredicateBool template_assertion_predicate_bool(bol, replace_opaque_loop_nodes, _phase);
  BoolNode* new_bol = template_assertion_predicate_bool.create(new_ctrl);
  IfNode* if_node = template_assertion_predicate->
                                                    create_initialized_assertion_predicate(new_ctrl, new_bol, AssertionPredicateType::Init_value);
  _phase->register_control(if_node, _loop, new_ctrl);
  return if_node;
}

// Create the out nodes of a newly created Initialized Assertion Predicate If node which includes the projections and
// the dedicated Halt node.
IfTrueNode* InitializedAssertionPredicate::create_iff_out_nodes(IfNode* if_node) {
  IfTrueNode* succ_proj = new IfTrueNode(if_node);
  IfFalseNode* fail_proj = new IfFalseNode(if_node);
  _phase->register_control(succ_proj, _loop, if_node);
  _phase->register_control(fail_proj, _loop, if_node);
  create_halt_node(fail_proj);
  return succ_proj;
}

void InitializedAssertionPredicate::create_halt_node(IfFalseNode* fail_proj) {
  StartNode* start_node = _phase->C->start();
  Node* frame = new ParmNode(start_node, TypeFunc::FramePtr);
  _phase->register_new_node(frame, start_node);
  Node* halt = new HaltNode(fail_proj, frame, "Assertion Predicate cannot fail");
  _phase->igvn().add_input_to(_phase->C->root(), halt);
  _phase->register_control(halt, _loop, fail_proj);
}

// Create Initialized Assertion Predicates from templates by iterating over the templates starting from
// 'template_assertion_predicate'.
void InitializedAssertionPredicates::create(TemplateAssertionPredicateNode* template_assertion_predicate) {
  TemplateAssertionPredicateIterator template_assertion_predicate_iterator(template_assertion_predicate);
  IfTrueNode* previous_initialized_predicate_succ_proj = nullptr;
  while (template_assertion_predicate_iterator.has_next()) {
    TemplateAssertionPredicateNode* template_assertion_predicate = template_assertion_predicate_iterator.next();
    IfTrueNode* initialized_predicate_succ_proj =
        _new_initialized_assertion_predicate->create_from(template_assertion_predicate);

    if (previous_initialized_predicate_succ_proj != nullptr) {
      _igvn->replace_input_of(previous_initialized_predicate_succ_proj, 0, initialized_predicate_succ_proj);
    } else {
      _igvn->replace_input_of(template_assertion_predicate, 0, initialized_predicate_succ_proj);
    }
    previous_initialized_predicate_succ_proj = initialized_predicate_succ_proj;
  }
}

// Creates new Template Assertion Predicates by cloning them from the source loop to the target loop and replacing
// the OpaqueLoop* nodes with the new init and stride value of the target loop.
void AssertionPredicatesAtTargetLoop::create_new_templates(
    OldTemplateAssertionPredicateOutput* old_template_assertion_predicate_output) {
  assert(must_create(), "no template assertion predicates");
  Node* target_loop_entry = _target_strip_mined_head->in(LoopNode::EntryControl);
  Compile* C = _phase->C;
  Node* opaque_init = new OpaqueLoopInitNode(C, _target_loop_head->init_trip());
  _phase->register_new_node(opaque_init, target_loop_entry);
  Node* opaque_stride = new OpaqueLoopStrideNode(C, _target_loop_head->stride());
  _phase->register_new_node(opaque_stride, target_loop_entry);
  ReplaceTemplateAssertionPredicates replace_template_assertion_predicates(_phase,
                                                                           old_template_assertion_predicate_output);
  Node* new_target_loop_entry = replace_template_assertion_predicates.replace(_source_loop_predicates, target_loop_entry,
                                                                              opaque_init, opaque_stride);
  _igvn->replace_input_of(_target_strip_mined_head, LoopNode::EntryControl, new_target_loop_entry);
  _phase->set_idom(_target_strip_mined_head, new_target_loop_entry, _phase->dom_depth(_target_strip_mined_head));
}

// Create Initialized Assertion Predicates for the init value from the Template Assertion Predicates.
void AssertionPredicatesAtTargetLoop::create_initialized_init_value_predicates() {
  Node* current_target_loop_entry = _target_strip_mined_head->in(LoopNode::EntryControl);
  assert(current_target_loop_entry->is_TemplateAssertionPredicate(), "must have created template predicate before");
  NewInitializedInitValueAssertionPredicate new_initialized_init_value_assertion_predicate(
      _target_loop_head->init_trip(), _old_target_loop_entry, _outer_loop, _phase);
  InitializedAssertionPredicates initialized_assertion_predicates(&new_initialized_init_value_assertion_predicate,
                                                                  _igvn);
  initialized_assertion_predicates.create(current_target_loop_entry->as_TemplateAssertionPredicate());
}

// Create Initialized Assertion Predicates for the last value from the Template Assertion Predicates.
void AssertionPredicatesAtTargetLoop::create_initialized_last_value_predicates() {
  Node* current_target_loop_entry = _target_strip_mined_head->in(LoopNode::EntryControl);
  assert(current_target_loop_entry->is_TemplateAssertionPredicate(), "must have created template predicate before");
  NewInitializedLastValueAssertionPredicate new_initialized_last_value_assertion_predicate(
      _target_loop_head->init_trip(), _target_loop_head->stride(), _old_target_loop_entry, _outer_loop, _phase);
  InitializedAssertionPredicates initialized_assertion_predicates(&new_initialized_last_value_assertion_predicate,
                                                                  _igvn);
  initialized_assertion_predicates.create(current_target_loop_entry->as_TemplateAssertionPredicate());
}
