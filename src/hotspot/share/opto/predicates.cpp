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

TemplateAssertionPredicateIterator::TemplateAssertionPredicateIterator(
    const TemplateAssertionPredicateBlock* template_assertion_predicate_block)
    : _current(template_assertion_predicate_block->last()) {}

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

InitializedAssertionPredicateBlock::InitializedAssertionPredicateBlock(Node* initialized_assertion_predicate_proj)
    : _entry(initialized_assertion_predicate_proj),
      _first_if(nullptr),
      _last_initialized_assertion_predicate_proj(initialized_assertion_predicate_proj) {
  Node* entry = initialized_assertion_predicate_proj;
  while (InitializedAssertionPredicate::is_success_proj(entry)) {
    _first_if = entry->in(0)->as_If();
    entry = _first_if->in(0);
  }
  _entry = entry;
}

void InitializedAssertionPredicateBlock::kill_dead(PhaseIterGVN* igvn) const {
  if (has_any()) {
    assert(_last_initialized_assertion_predicate_proj->outcnt() == 0, "must be dead in order to kill this block");
    igvn->replace_input_of(_first_if, 0, igvn->C->top());
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

void ParsePredicates::clone_parse_predicate(const RegularPredicateBlock* regular_predicate_block) {
  if (!regular_predicate_block->is_empty()) {
    ParsePredicateSuccessProj* parse_predicate_proj = regular_predicate_block->parse_predicate_success_proj();
    create_new_parse_predicate(parse_predicate_proj);
  }
}

void ParsePredicates::create_new_parse_predicate(ParsePredicateSuccessProj* predicate_proj) {
  _new_entry = _new_parse_predicate->create(_phase, _new_entry, predicate_proj);
  ParsePredicateNode* parse_predicate = _new_entry->in(0)->as_ParsePredicate();
  _phase->igvn().hash_delete(parse_predicate);
}

// Clone the Parse Predicates from the Predicate Blocks to this loop and add them below '_new_entry'.
// Return the last node in the newly created Parse Predicate chain.
Node* ParsePredicates::clone(const RegularPredicateBlocks* regular_predicate_blocks) {
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
TemplateAssertionPredicateNode* TemplateAssertionPredicates::walk_templates(
    NewTemplateAssertionPredicate* _new_template_assertion_predicate) {
  TemplateAssertionPredicateIterator iterator(_template_assertion_predicate_block);
  TemplateAssertionPredicateNode* previous_template_assertion_predicate = nullptr;
  TemplateAssertionPredicateNode* last_template_assertion_predicate = nullptr;
  assert(iterator.has_next(), "must have at least one");
  while (iterator.has_next()) {
    TemplateAssertionPredicateNode* template_assertion_predicate = iterator.next();
    TemplateAssertionPredicateNode* new_template_assertion_predicate =
        _new_template_assertion_predicate->create_from(template_assertion_predicate);
    if (previous_template_assertion_predicate != nullptr) {
      _phase->igvn().replace_input_of(previous_template_assertion_predicate, 0, new_template_assertion_predicate);
    } else {
      last_template_assertion_predicate = new_template_assertion_predicate;
    }
    previous_template_assertion_predicate = new_template_assertion_predicate;
  }
  return last_template_assertion_predicate;
}

void TemplateAssertionPredicates::create_opaque_loop_nodes(Node*& opaque_init, Node*& opaque_stride,
                                                           CountedLoopNode* target_loop_head) {
  Node* target_loop_entry = target_loop_head->skip_strip_mined()->in(LoopNode::EntryControl);
  Compile* C = _phase->C;
  opaque_init= new OpaqueLoopInitNode(C, target_loop_head->init_trip());
  _phase->register_new_node(opaque_init, target_loop_entry);
  opaque_stride= new OpaqueLoopStrideNode(C, target_loop_head->stride());
  _phase->register_new_node(opaque_stride, target_loop_entry);
}

TemplateAssertionPredicateNode* TemplateAssertionPredicates::clone_and_update_to(CountedLoopNode* target_loop_head,
                                                                                 NodeInTargetLoop* node_in_target_loop) {
  Node* opaque_init;
  Node* opaque_stride;
  create_opaque_loop_nodes(opaque_init, opaque_stride, target_loop_head);
  Node* initial_target_loop_entry = target_loop_head->skip_strip_mined()->in(LoopNode::EntryControl);
  CreateTemplateAssertionPredicate create_template_assertion_predicate(opaque_init, opaque_stride,
                                                                       initial_target_loop_entry, _phase,
                                                                       node_in_target_loop);
  return walk_templates(&create_template_assertion_predicate);
}

void TemplateAssertionPredicates::update(CountedLoopNode* loop_head) {
  Node* opaque_init;
  Node* opaque_stride;
  create_opaque_loop_nodes(opaque_init, opaque_stride, loop_head);
  UpdateTemplateAssertionPredicate update_template_assertion_predicate(opaque_init, opaque_stride, _phase);
  walk_templates(&update_template_assertion_predicate);
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

// Create a new Bool node from the provided Template Assertion Predicate Bool.
// Replace found OpaqueLoop* nodes with new_init and new_stride, respectively, if non-null.
// All newly cloned (non-CFG) nodes will get 'ctrl' as new ctrl.
BoolNode* TemplateAssertionPredicateBool::clone(Node* new_ctrl, ReplaceOpaqueLoopNodes* replace_opaque_loop_nodes) {
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
        _phase->register_new_node(n, new_ctrl);
      }
      const int op = input->Opcode();
      if (op == Op_OpaqueLoopInit) {
        Node* replacement = replace_opaque_loop_nodes->replace_init(input->as_OpaqueLoopInit(), new_ctrl);
        n->set_req(i, replacement);
        found_init = true;
      } else {
        Node* replacement = replace_opaque_loop_nodes->replace_stride(input->as_OpaqueLoopStride(), new_ctrl);
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
          _phase->register_new_node(next, new_ctrl);
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

TemplateAssertionPredicateNode* TemplateAssertionPredicate::clone_to(Node* new_entry,
                                                                     NodeInTargetLoop* node_in_target_loop) {
  BoolNode* new_init_bool = _init_value_bool.clone(new_entry, _replace_opaque_loop_nodes);
  BoolNode* new_last_bool = _last_value_bool.clone(new_entry, _replace_opaque_loop_nodes);
  TemplateAssertionPredicateNode* clone = _template_assertion_predicate->clone()->as_TemplateAssertionPredicate();
  _phase->igvn().replace_input_of(clone, TemplateAssertionPredicateNode::InitValue, new_init_bool);
  _phase->igvn().replace_input_of(clone, TemplateAssertionPredicateNode::LastValue, new_last_bool);
  _phase->igvn().register_new_node_with_optimizer(clone);
  _phase->igvn().replace_input_of(clone, 0, new_entry);
  _phase->set_idom(clone, new_entry, _phase->dom_depth(new_entry));
  update_data_dependencies(clone, node_in_target_loop);
  return clone->as_TemplateAssertionPredicate();
}

void TemplateAssertionPredicate::update() {
  Node* current_ctrl = _template_assertion_predicate->in(0);
  BoolNode* new_init_bool = _init_value_bool.clone(current_ctrl, _replace_opaque_loop_nodes);
  BoolNode* new_last_bool = _last_value_bool.clone(current_ctrl, _replace_opaque_loop_nodes);
  _phase->igvn().replace_input_of(_template_assertion_predicate, TemplateAssertionPredicateNode::InitValue, new_init_bool);
  _phase->igvn().replace_input_of(_template_assertion_predicate, TemplateAssertionPredicateNode::LastValue, new_last_bool);
}


void TemplateAssertionPredicate::update_data_dependencies(Node* new_template_assertion_predicate,
                                                          NodeInTargetLoop* node_in_target_loop) {
  for (DUIterator_Fast imax, i = _template_assertion_predicate->fast_outs(imax); i < imax; i++) {
    Node* node = _template_assertion_predicate->fast_out(i);
    if (!node->is_CFG() && node_in_target_loop->check(node)) {
      _phase->igvn().replace_input_of(node, 0, new_template_assertion_predicate);
      --i;
      --imax;
    }
  }
}

TemplateAssertionPredicateNode*
CreateTemplateAssertionPredicate::create_from(TemplateAssertionPredicateNode* template_assertion_predicate_node) {
  TemplateAssertionPredicate template_assertion_predicate(template_assertion_predicate_node,
                                                          &_replace_opaque_loop_nodes, _phase);
  return template_assertion_predicate.clone_to(_new_ctrl, _node_in_target_loop);
}

// Create a new If or RangeCheck node to represent an Initialized Assertion Predicate and return it.
IfNode* InitializedAssertionPredicate::create_if_node(TemplateAssertionPredicateNode* template_assertion_predicate,
                                                      Node* new_ctrl, BoolNode* new_bool,
                                                      AssertionPredicateType assertion_predicate_type) const {
  OpaqueAssertionPredicateNode* opaque_assertion_predicate_node = new OpaqueAssertionPredicateNode(_phase->C, new_bool);
  _phase->register_new_node(opaque_assertion_predicate_node,new_ctrl);
  IfNode* if_node = template_assertion_predicate->create_initialized_assertion_predicate(
      new_ctrl, opaque_assertion_predicate_node, assertion_predicate_type);
  _phase->register_control(if_node, _outer_target_loop, new_ctrl);
  return if_node;
}

// Create the out nodes of a newly created Initialized Assertion Predicate If node which includes the projections and
// the dedicated Halt node.
IfTrueNode* InitializedAssertionPredicate::create_if_node_out_nodes(IfNode* if_node) {
  IfTrueNode* succ_proj = new IfTrueNode(if_node);
  IfFalseNode* fail_proj = new IfFalseNode(if_node);
  _phase->register_control(succ_proj, _outer_target_loop, if_node);
  _phase->register_control(fail_proj, _outer_target_loop, if_node);
  create_halt_node(fail_proj);
  return succ_proj;
}

void InitializedAssertionPredicate::create_halt_node(IfFalseNode* fail_proj) {
  StartNode* start_node = _phase->C->start();
  Node* frame = new ParmNode(start_node, TypeFunc::FramePtr);
  _phase->register_new_node(frame, start_node);
  Node* halt = new HaltNode(fail_proj, frame, "Assertion Predicate cannot fail");
  _phase->igvn().add_input_to(_phase->C->root(), halt);
  _phase->register_control(halt, _outer_target_loop, fail_proj);
}

bool InitializedAssertionPredicate::is_success_proj(const Node* success_proj) {
  if (success_proj == nullptr || !success_proj->is_IfProj()) {
    return false;
  }
  return has_opaque(success_proj) && has_halt(success_proj);
}

// Check if the If node of `predicate_proj` has an OpaqueAssertionPredicate node as input.
bool InitializedAssertionPredicate::has_opaque(const Node* predicate_proj) {
  IfNode* iff = predicate_proj->in(0)->as_If();
  return iff->in(1)->Opcode() == Op_OpaqueAssertionPredicate;
}

// Check if the other projection (UCT projection) of `success_proj` has a Halt node as output.
bool InitializedAssertionPredicate::has_halt(const Node* success_proj) {
  ProjNode* other_proj = success_proj->as_IfProj()->other_if_proj();
  return other_proj->outcnt() == 1 && other_proj->unique_out()->Opcode() == Op_Halt;
}

// Create Initialized Assertion Predicates from templates by iterating over the templates starting from
// 'template_assertion_predicate'.
void InitializedAssertionPredicates::create(TemplateAssertionPredicateNode* template_assertion_predicate) {
  TemplateAssertionPredicateIterator template_assertion_predicate_iterator(template_assertion_predicate);
  Node* hook = new Node(1);
  Node* out_node = hook;
  assert(template_assertion_predicate_iterator.has_next(), "sanity check, must have template predicates");
  while (template_assertion_predicate_iterator.has_next()) {
    template_assertion_predicate = template_assertion_predicate_iterator.next();
    out_node = create_last_value_assertion_predicate(template_assertion_predicate, out_node);
    out_node = create_init_value_assertion_predicate(template_assertion_predicate, out_node);
  }
  _phase->replace_ctrl_same_depth(template_assertion_predicate, hook->in(0));
  hook->destruct(&_phase->igvn());
}

IfNode* InitializedAssertionPredicates::create_init_value_assertion_predicate(
    TemplateAssertionPredicateNode* template_assertion_predicate, Node* out_node) {
  IfTrueNode* initialized_predicate_succ_proj =
      _initialized_init_value_assertion_predicate.create(template_assertion_predicate);
  _phase->replace_ctrl_same_depth(out_node, initialized_predicate_succ_proj);
  return initialized_predicate_succ_proj->in(0)->as_If();;
}

IfNode* InitializedAssertionPredicates::create_last_value_assertion_predicate(
    TemplateAssertionPredicateNode* template_assertion_predicate, Node* out_node) {
  IfTrueNode* initialized_predicate_succ_proj =
      _initialized_last_value_assertion_predicate.create(template_assertion_predicate);
  _phase->replace_ctrl_same_depth(out_node, initialized_predicate_succ_proj);
  return initialized_predicate_succ_proj->in(0)->as_If();
}


// Creates new Assertion Predicates at the 'target_loop_head'. A new Template Assertion Predicate is inserted with the
// new init and stride values of the target loop for each existing Template Assertion Predicate found at source loop.
// Afterward, Initialized Assertion Predicates are created based on the newly created templates.
// 'first_cloned_loop_index' is the first (i.e. smallest) node index found in the cloned loop of the current loop optimization.
void AssertionPredicates::create_at(CountedLoopNode* target_loop_head, const uint first_cloned_loop_node_index) {
  if (has_any()) {
    Node* initial_target_loop_entry = target_loop_head->skip_strip_mined()->in(LoopNode::EntryControl);
    create_template_predicates(target_loop_head, first_cloned_loop_node_index);
    InitializedAssertionPredicates initialized_assertion_predicates(target_loop_head->init_trip(),
                                                                    target_loop_head->stride(),
                                                                    initial_target_loop_entry,_outer_target_loop);
    initialized_assertion_predicates.create(
        target_loop_head->skip_strip_mined()->in(LoopNode::EntryControl)->as_TemplateAssertionPredicate());
  }
}

// Creates new Template Assertion Predicates at the 'target_loop_head'. A new Template Assertion Predicate is inserted
// with the  new init and stride values of the target loop for each existing Template Assertion Predicate found at source
// loop.
void AssertionPredicates::create_template_predicates(CountedLoopNode* target_loop_head,
                                                     const uint first_cloned_loop_node_index) {
  TemplateAssertionPredicates template_assertion_predicates(_source_loop_predicates.template_assertion_predicate_block(),
                                                            _phase);
  NodeInTargetLoop* node_in_target_loop = create_node_in_target_loop(target_loop_head, first_cloned_loop_node_index);
  TemplateAssertionPredicateNode* new_target_loop_entry =
      template_assertion_predicates.clone_and_update_to(target_loop_head, node_in_target_loop);
  _phase->replace_loop_entry(target_loop_head->skip_strip_mined(), new_target_loop_entry);
}

// Creates new Assertion Predicates at the 'target_loop_head'. A new Template Assertion Predicate is inserted with the
// new init and stride values of the target loop for each existing Template Assertion Predicate found at source loop.
// The existing Template Assertion Predicates at the source loop are removed.
// Afterward, Initialized Assertion Predicates are created based on the newly created templates.
// 'first_cloned_loop_index' is the first (i.e. smallest) node index found in the cloned loop of the current loop optimization.
void AssertionPredicates::replace_to(CountedLoopNode* target_loop_head, const uint first_cloned_loop_node_index) {
  if (has_any()) {
    create_at(target_loop_head, first_cloned_loop_node_index);
    _source_loop_predicates.template_assertion_predicate_block()->mark_useless(&_phase->igvn());
  }
}

// Creates new Assertion Predicates at the source loop. The templates are updated and new Initialized Assertion Predicates
// are inserted based on the updated templates. Existing Initialized Assertion Predicates are no longer needed and killed.
void AssertionPredicates::create_at_source_loop(const int new_stride_con) {
  if (has_any()) {
    const InitializedAssertionPredicateBlock* initialized_assertion_predicate_block =
        _source_loop_predicates.initialized_assertion_predicate_block();
    TemplateAssertionPredicates template_assertion_predicates(_source_loop_predicates.template_assertion_predicate_block(),
                                                              _phase);
    template_assertion_predicates.update(_source_loop_head);
    Node* new_stride = _phase->igvn().intcon(new_stride_con);
    _phase->set_ctrl(new_stride, _phase->C->root());

    InitializedAssertionPredicates initialized_assertion_predicates(_source_loop_head->init_trip(), new_stride,
                                                                    initialized_assertion_predicate_block->entry(),
                                                                    _outer_target_loop);
    initialized_assertion_predicates.create(
        _source_loop_head->skip_strip_mined()->in(LoopNode::EntryControl)->as_TemplateAssertionPredicate());
    initialized_assertion_predicate_block->kill_dead(&_phase->igvn());
  }
}
