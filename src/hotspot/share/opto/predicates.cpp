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
#include "opto/addnode.hpp"
#include "opto/callnode.hpp"
#include "opto/castnode.hpp"
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

TopDownTemplateAssertionPredicateIterator::TopDownTemplateAssertionPredicateIterator(
    const TemplateAssertionPredicateBlock* template_assertion_predicate_block)
    : _current(template_assertion_predicate_block->first()) {}

TemplateAssertionPredicateNode* TopDownTemplateAssertionPredicateIterator::next() {
  assert(has_next(), "always check has_next() first");
  TemplateAssertionPredicateNode* current = _current->as_TemplateAssertionPredicate();
  _current = _current->unique_ctrl_out();
  return current;
}

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

void EliminateUselessParsePredicates::eliminate() {
  mark_all_parse_predicates_useless();

  for (LoopTreeIterator iterator(_ltree_root); !iterator.done(); iterator.next()) {
    IdealLoopTree* loop = iterator.current();
    mark_parse_predicates_useful(loop);
  }

  add_useless_predicates_to_igvn();
}

void EliminateUselessParsePredicates::mark_all_parse_predicates_useless() {
  for (int i = 0; i < C->parse_predicates().length(); i++) {
    C->parse_predicates().at(i)->mark_useless();
  }
}

void EliminateUselessParsePredicates::mark_parse_predicates_useful(IdealLoopTree* loop) {
  if (loop->can_apply_loop_predication()) {
    Node* entry = loop->_head->in(LoopNode::EntryControl);
    const Predicates predicates(entry);

    ParsePredicateIterator iterator(predicates);
    while (iterator.has_next()) {
      iterator.next()->mark_useful();
    }
  }
}

void EliminateUselessParsePredicates::add_useless_predicates_to_igvn() {
  for (int i = 0; i < C->parse_predicates().length(); i++) {
    ParsePredicateNode* predicate_node = C->parse_predicates().at(i);
    if (predicate_node->is_useless()) {
      _igvn->_worklist.push(predicate_node);
    }
  }
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
Node* ParsePredicates::clone(const Predicates* predicates) {
  clone_parse_predicate(predicates->loop_predicate_block());
  clone_parse_predicate(predicates->profiled_loop_predicate_block());
  if (!_loop_node->is_CountedLoop()) {
    // Don't clone the Loop Limit Check Parse Predicate if we already have a counted loop (a Loop Limit Check Predicate
    // is only created when converting a LoopNode to a CountedLoopNode).
    clone_parse_predicate(predicates->loop_limit_check_predicate_block());
  }
  return _new_entry;
}

// Transform all Template Assertion Predicates inside the Template Assertion Predicate Block.
// Returns the last node in the chain of transformed Template Assertion Predicates.
TemplateAssertionPredicateNode* TemplateAssertionPredicates::create_templates(Node* new_entry,
    NewTemplateAssertionPredicate* new_template_assertion_predicate) {
  TopDownTemplateAssertionPredicateIterator iterator(_template_assertion_predicate_block);
  Node* new_ctrl = new_entry;
  TemplateAssertionPredicateNode* template_assertion_predicate;
  assert(iterator.has_next(), "must have at least one");
  while (iterator.has_next()) {
    template_assertion_predicate = iterator.next();
    new_ctrl = new_template_assertion_predicate->create_from(new_ctrl, template_assertion_predicate);
  }
  return new_ctrl->as_TemplateAssertionPredicate();
}

// Creates new Template Assertion Predicates with the . A new Template Assertion Predicate is inserted
// with the new init and stride values of the target loop for each existing Template Assertion Predicate found at source
// loop.
TemplateAssertionPredicateNode* TemplateAssertionPredicates::create_templates_at_loop(CountedLoopNode* target_loop_head,
                                                                                      NodeInTargetLoop* node_in_target_loop) {
  Node* initial_target_loop_entry = target_loop_head->skip_strip_mined()->in(LoopNode::EntryControl);
  CreateTemplateAssertionPredicate create_template_assertion_predicate(target_loop_head->init_trip(),
                                                                       target_loop_head->stride(),
                                                                       _phase,
                                                                       node_in_target_loop);
  return create_templates(initial_target_loop_entry, &create_template_assertion_predicate);
}

// Update the Template Assertion Predicates of the source loop by creating new Bools for them with new OpaqueLoop* nodes
// using the provided init and stride value.
void TemplateAssertionPredicates::update_templates(Node* new_init, Node* new_stride) {
  CreateOpaqueLoopNodes create_opaque_loop_nodes(new_init, new_stride, _phase);
  TopDownTemplateAssertionPredicateIterator iterator(_template_assertion_predicate_block);
  TemplateAssertionPredicateNode* template_assertion_predicate_node;
  assert(iterator.has_next(), "must have at least one");
  while (iterator.has_next()) {
    template_assertion_predicate_node = iterator.next();
    TemplateAssertionPredicate template_assertion_predicate(template_assertion_predicate_node, &create_opaque_loop_nodes,
                                                            _phase);
    template_assertion_predicate.update_bools();
  }
}

class TransformRelatedNodes : public StackObj {
 public:
  virtual Node* transform_chain_node(Node* node_in_chain) = 0;
  virtual Node* transform_opaque_init(OpaqueLoopInitNode* opaque_init) = 0;
  virtual Node* transform_opaque_stride(OpaqueLoopStrideNode* opaque_stride) = 0;
};

class CloneAll : public TransformRelatedNodes {
  PhaseIdealLoop* _phase;
  Node* _new_ctrl;

 public:
  CloneAll(PhaseIdealLoop* phase, Node* new_ctrl) : _phase(phase), _new_ctrl(new_ctrl) {}

  Node* transform_chain_node(Node* node_in_chain) override {
    return _phase->clone_and_register(node_in_chain, _new_ctrl);
  }

  Node* transform_opaque_init(OpaqueLoopInitNode* opaque_init) override {
    return _phase->clone_and_register(opaque_init, _new_ctrl);
  }

  Node* transform_opaque_stride(OpaqueLoopStrideNode* opaque_stride) override {
    return _phase->clone_and_register(opaque_stride, _new_ctrl);
  }
};

class CloneWithNewOpaqueInitInput : public TransformRelatedNodes {
  PhaseIdealLoop* _phase;
  Node* _new_ctrl;
  Node* _new_opaque_init_input;

 public:
  CloneWithNewOpaqueInitInput(PhaseIdealLoop* phase, Node* new_ctrl, Node* new_opaque_init_input)
      : _phase(phase),
        _new_ctrl(new_ctrl),
        _new_opaque_init_input(new_opaque_init_input) {}

  Node* transform_chain_node(Node* node_in_chain) override {
    return _phase->clone_and_register(node_in_chain, _new_ctrl);
  }

  Node* transform_opaque_init(OpaqueLoopInitNode* opaque_init) override {
    Node* new_opaque_init = _phase->clone_and_register(opaque_init, _new_ctrl);
    _phase->igvn().replace_input_of(new_opaque_init, 1, _new_opaque_init_input);
    return new_opaque_init;
  }

  Node* transform_opaque_stride(OpaqueLoopStrideNode* opaque_stride) override {
    return _phase->clone_and_register(opaque_stride, _new_ctrl);
  }
};

class CloneWithInitialization : public TransformRelatedNodes {
  PhaseIdealLoop* _phase;
  Node* _ctrl;
  Node* _new_init;
  Node* _new_stride;

 public:
  CloneWithInitialization(PhaseIdealLoop* phase, Node* new_ctrl, Node* new_init, Node* new_stride)
      : _phase(phase),
        _new_init(new_init),
        _new_stride(new_stride) {}

  Node* transform_chain_node(Node* node_in_chain) override {
    return _phase->clone_and_register(node_in_chain, _ctrl);
  }

  Node* transform_opaque_init(OpaqueLoopInitNode* opaque_init) override {
    return _new_init;
  }

  Node* transform_opaque_stride(OpaqueLoopStrideNode* opaque_stride) override {
    return _new_stride;
  }
};

class UpdateOpaqueStrideInput : public TransformRelatedNodes {
  PhaseIterGVN* _igvn;
  Node* _new_opaque_stride_input;

 public:
  UpdateOpaqueStrideInput(PhaseIterGVN* igvn, Node* new_opaque_stride_input)
      : _igvn(igvn),
        _new_opaque_stride_input(new_opaque_stride_input) {}

  Node* transform_chain_node(Node* node_in_chain) override {
    return node_in_chain;
  }

  Node* transform_opaque_init(OpaqueLoopInitNode* opaque_init) override {
    return opaque_init;
  }

  Node* transform_opaque_stride(OpaqueLoopStrideNode* opaque_stride) override {
    _igvn->replace_input_of(opaque_stride, 1, _new_opaque_stride_input);
    return opaque_stride;
  }
};



BoolNode* TemplateAssertionPredicateBool::clone(Node* new_ctrl) {
  CloneAll clone_all(_phase, new_ctrl);

}

BoolNode* TemplateAssertionPredicateBool::clone_update_opaque_init(Node* new_opaque_init_input) {
  return nullptr;
}

BoolNode* TemplateAssertionPredicateBool::clone_initialized(Node* new_init, Node* new_stride) {
  return nullptr;
}

void TemplateAssertionPredicateBool::update_opaque_stride(Node* new_opaque_stride_input) {

}

// Create a new Bool node from the provided Template Assertion Predicate Bool.
// Replace found OpaqueLoop* nodes with new_init and new_stride, respectively, if non-null.
// All newly cloned (non-CFG) nodes will get 'ctrl' as new ctrl.
BoolNode* TemplateAssertionPredicateBool::transform(TransformRelatedNodes* transform_related_nodes) {
  Node_Stack to_clone(2);
  to_clone.push(_source_bol, 1);
  const uint idx_before_cloning = C->unique();
  Node* result = nullptr;
  bool found_init = false;
  // Look for the OpaqueLoop* nodes to replace with the strategy defined with 'transform_opaque_loop_nodes'. Clone all
  // nodes in between.
  do {
    Node* n = to_clone.node();
    const uint i = to_clone.index();
    Node* input = n->in(i);
    if (AssertionPredicateBoolOpcodes::is_valid(input)) {
      if (input->is_Opaque1()) {
        if (n->_idx < idx_before_cloning) {
          n = transform_related_nodes->transform_chain_node(n);
        }
        const int op = input->Opcode();
        if (op == Op_OpaqueLoopInit) {
          transform_related_nodes->transform_opaque_init(input->as_OpaqueLoopInit());
          found_init = true;
        } else {
          transform_related_nodes->transform_opaque_stride(input->as_OpaqueLoopStride());
        }
        to_clone.set_node(n);
      } else {
        to_clone.push(input, 1);
        continue;
      }
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
        // We cloned next->in(j), so we also need to clone next on the way back to the BoolNode.
        assert(cur->_idx >= idx_before_cloning || next->in(j)->Opcode() == Op_Opaque1, "new node or Opaque1 being replaced");
        if (next->_idx < idx_before_cloning) {
          assert(!next->is_OpaqueLoopInit() || !next->is_OpaqueLoopStride(), "should be normal chain node");
          next = transform_related_nodes->transform_chain_node(next);
          assert(next->_idx > idx_before_cloning, "should have been cloned");
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

// Create a new Template Assertion Predicate with new Bools based on the provided old Template Assertion Predicate.
TemplateAssertionPredicateNode* TemplateAssertionPredicate::create(Node* new_ctrl,
                                                                   NodeInTargetLoop* node_in_target_loop) {
  // Clone instead of creating a new node to maintain a link to the old node to simplify debugging.
  TemplateAssertionPredicateNode* clone = _template_assertion_predicate->clone()->as_TemplateAssertionPredicate();
  create_bools(new_ctrl, clone);
  _phase->igvn().register_new_node_with_optimizer(clone);
  _phase->igvn().replace_input_of(clone, 0, new_ctrl);
  _phase->set_idom(clone, new_ctrl, _phase->dom_depth(new_ctrl));
  update_data_dependencies(clone, node_in_target_loop);
  return clone->as_TemplateAssertionPredicate();
}

// Create new Bools for the provided Template Assertion Predicate.
void TemplateAssertionPredicate::update_bools() {
  create_bools(_template_assertion_predicate->in(0), _template_assertion_predicate);
}

void TemplateAssertionPredicate::create_bools(Node* new_ctrl,
                                              TemplateAssertionPredicateNode* new_template_assertion_predicate) {
  BoolNode* new_init_bool = _init_value_bool.create(new_ctrl, _transform_opaque_loop_nodes);
  BoolNode* new_last_bool = _last_value_bool.create(new_ctrl, _transform_opaque_loop_nodes);
  _phase->igvn().replace_input_of(new_template_assertion_predicate, TemplateAssertionPredicateNode::InitValue, new_init_bool);
  _phase->igvn().replace_input_of(new_template_assertion_predicate, TemplateAssertionPredicateNode::LastValue, new_last_bool);
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
CreateTemplateAssertionPredicate::create_from(Node* new_ctrl,
                                              TemplateAssertionPredicateNode* template_assertion_predicate_node) {
  TemplateAssertionPredicate template_assertion_predicate(template_assertion_predicate_node,
                                                          &_create_opaque_loop_nodes, _phase);
  return template_assertion_predicate.create(new_ctrl, _node_in_target_loop);
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
IfTrueNode* InitializedAssertionPredicates::create(const TemplateAssertionPredicateBlock* template_assertion_predicate_block) {
  TopDownTemplateAssertionPredicateIterator iterator(template_assertion_predicate_block);
  Node* ctrl = _block_entry;
  assert(iterator.has_next(), "sanity check, must have template predicates");
  while (iterator.has_next()) {
    TemplateAssertionPredicateNode* template_assertion_predicate = iterator.next();
    ctrl = _initialized_init_value_assertion_predicate.create_from(ctrl, template_assertion_predicate);
    ctrl = _initialized_last_value_assertion_predicate.create_from(ctrl, template_assertion_predicate);
  }
  return ctrl->as_IfTrue();
}

// Creates new Assertion Predicates at the 'target_loop_head'. A new Template Assertion Predicate is inserted with the
// new init and stride values of the target loop for each existing Template Assertion Predicate found at source loop.
// Afterward, Initialized Assertion Predicates are created based on the newly created templates.
void AssertionPredicates::create_at_target_loop(CountedLoopNode* target_loop_head, NodeInTargetLoop* node_in_target_loop) {
  if (has_any()) {
    Node* initial_target_loop_entry = target_loop_head->skip_strip_mined()->in(LoopNode::EntryControl);
    TemplateAssertionPredicateNode* new_target_loop_entry = create_templates_at_loop(target_loop_head,
                                                                                     node_in_target_loop);
    const TemplateAssertionPredicateBlock template_assertion_predicate_block(new_target_loop_entry);
    IfTrueNode* last_success_proj = initialize_templates_at_loop(target_loop_head, initial_target_loop_entry,
                                                                 &template_assertion_predicate_block);
    _phase->replace_ctrl_same_depth(template_assertion_predicate_block.first(), last_success_proj);
  }
}

IfTrueNode* AssertionPredicates::initialize_templates_at_loop(
    CountedLoopNode* target_loop_head, Node* initial_target_loop_entry,
    const TemplateAssertionPredicateBlock* template_assertion_predicate_block) const {
  InitializedAssertionPredicates initialized_assertion_predicates(target_loop_head->init_trip(),
                                                                  target_loop_head->stride(),
                                                                  initial_target_loop_entry, _outer_target_loop);
  return initialized_assertion_predicates.create(template_assertion_predicate_block);
}

// Creates new Template Assertion Predicates at the 'target_loop_head'. A new Template Assertion Predicate is inserted
// with the new init and stride values of the target loop for each existing Template Assertion Predicate found at source
// loop.
TemplateAssertionPredicateNode* AssertionPredicates::create_templates_at_loop(CountedLoopNode* target_loop_head,
                                                                              NodeInTargetLoop* node_in_target_loop) {
  TemplateAssertionPredicates template_assertion_predicates(_source_loop_predicates.template_assertion_predicate_block(),
                                                            _phase);
  TemplateAssertionPredicateNode* new_target_loop_entry =
      template_assertion_predicates.create_templates_at_loop(target_loop_head, node_in_target_loop);
  _phase->replace_loop_entry(target_loop_head->skip_strip_mined(), new_target_loop_entry);
  return new_target_loop_entry;
}

// Creates new Assertion Predicates at the 'target_loop_head'. A new Template Assertion Predicate is inserted with the
// new init and stride values of the target loop for each existing Template Assertion Predicate found at source loop.
// The existing Template Assertion Predicates at the source loop are removed.
// Afterward, Initialized Assertion Predicates are created based on the newly created templates.
void AssertionPredicates::replace_to(CountedLoopNode* target_loop_head, NodeInTargetLoop* node_in_target_loop) {
  if (has_any()) {
    create_at_target_loop(target_loop_head, node_in_target_loop);
    _source_loop_predicates.template_assertion_predicate_block()->mark_useless(&_phase->igvn());
  }
}

// Creates new Assertion Predicates at the source loop based on the source loop init and stride values.
void AssertionPredicates::create_at_source_loop(const int if_opcode, const int scale, Node* offset, Node* range,
                                                const bool negate) {
  TemplateAssertionPredicateNode* template_assertion_predicate =
      _phase->add_template_assertion_predicate(if_opcode, _loop, scale, offset, range, negate);
  const InitializedAssertionPredicateBlock* initialized_assertion_predicate_block =
      _source_loop_predicates.initialized_assertion_predicate_block();
  InitializedAssertionPredicates initialized_assertion_predicates(_source_loop_head->init_trip(),
                                                                  _source_loop_head->stride(),
                                                                  initialized_assertion_predicate_block->entry(),
                                                                  _outer_target_loop);
  const TemplateAssertionPredicateBlock template_assertion_predicate_block(template_assertion_predicate);
  initialized_assertion_predicates.create(&template_assertion_predicate_block);
}

// Update existing Assertion Predicates at the source loop with the newly provided stride value. The templates are first
// updated and then new Initialized Assertion Predicates are created based on the updated templates. The previously
// existing Initialized Assertion are no longer needed and killed.
void AssertionPredicates::update_at_source_loop(const int new_stride_con) {
  if (has_any()) {
    // TODO: Monday: Update templates and then for initialized predicate: clone bool and take inputs of OpaqueLoop nodes?
    update_templates(new_stride_con);
    const InitializedAssertionPredicateBlock* initialized_assertion_predicate_block =
        _source_loop_predicates.initialized_assertion_predicate_block();
    Node* new_stride = create_stride(new_stride_con);
    InitializedAssertionPredicates initialized_assertion_predicates(_source_loop_head->init_trip(), new_stride,
                                                                    initialized_assertion_predicate_block->entry(),
                                                                    _outer_target_loop);
    const TemplateAssertionPredicateBlock* template_assertion_predicate_block =
        _source_loop_predicates.template_assertion_predicate_block();
    IfTrueNode* last_success_proj = initialized_assertion_predicates.create(template_assertion_predicate_block);
    _phase->replace_ctrl_same_depth(template_assertion_predicate_block->first(), last_success_proj);
    initialized_assertion_predicate_block->kill_dead(&_phase->igvn());
  }
}

void AssertionPredicates::update_templates(const int new_stride_con) {
  TemplateAssertionPredicates template_assertion_predicates(_source_loop_predicates.template_assertion_predicate_block(),
                                                            _phase);
  Node* new_stride = create_stride(new_stride_con);
  template_assertion_predicates.update_templates(_source_loop_head->init_trip(), new_stride);
}

Node* AssertionPredicates::create_stride(const int stride_con) {
  Node* new_stride = _phase->igvn().intcon(stride_con);
  _phase->set_ctrl(new_stride, _phase->C->root());
  return new_stride;
}

Node* TemplateAssertionPredicateBools::create_last_value(Node* new_ctrl, OpaqueLoopInitNode* opaque_init) {
  Node* init_stride = _loop_head->stride();
  Node* opaque_stride = new OpaqueLoopStrideNode(_phase->C, init_stride);
  _phase->register_new_node(opaque_stride, new_ctrl);
  Node* last_value = new SubINode(opaque_stride, init_stride);
  _phase->register_new_node(last_value, new_ctrl);
  last_value = new AddINode(opaque_init, last_value);
  _phase->register_new_node(last_value, new_ctrl);
  // init + (current stride - initial stride) is within the loop so narrow its type by leveraging the type of the iv Phi
  last_value = new CastIINode(last_value, _loop_head->phi()->bottom_type());
  _phase->register_new_node(last_value, new_ctrl);
  return last_value;
}

// Is current node pointed at by iterator a predicate?
bool PredicatesIterator::is_predicate() const {
  if (_current->is_TemplateAssertionPredicate()) {
    return true;
  } else if (_current->is_IfProj()) {
      Node* if_node = _current->in(0);
      assert(if_node->is_If(), "must be");
      return (if_node->is_ParsePredicate() ||
              RuntimePredicate::is_success_proj(_current) ||
              InitializedAssertionPredicate::is_success_proj(_current));
    }
  return false;
}

// Skip the current predicate pointed at by iterator by returning the input into the predicate. This could possibly be
// a non-predicate node.
Node* PredicatesIterator::skip() {
  assert(is_predicate(), "current must be predicate to go to next one");
  if (_current->is_TemplateAssertionPredicate()) {
    _current = _current->in(0);
  } else {
    _current = _current->in(0)->in(0);
  }
  return _current;
}
