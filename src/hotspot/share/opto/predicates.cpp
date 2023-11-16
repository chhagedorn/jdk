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

// Returns the Parse Predicate node if the provided node is a Parse Predicate success proj. Otherwise, return null.
ParsePredicateNode* ParsePredicate::init_parse_predicate(Node* parse_predicate_proj,
                                                         Deoptimization::DeoptReason deopt_reason) {
  assert(parse_predicate_proj != nullptr, "must not be null");
  if (parse_predicate_proj->is_IfTrue() && parse_predicate_proj->in(0)->is_ParsePredicate()) {
    ParsePredicateNode* parse_predicate_node = parse_predicate_proj->in(0)->as_ParsePredicate();
    if (parse_predicate_node->deopt_reason() == deopt_reason) {
      return parse_predicate_node;
    }
  }
  return nullptr;
}

void EliminateUselessParsePredicates::eliminate() {
  mark_all_parse_predicates_useless();
  for (LoopTreeIterator iterator(_ltree_root); !iterator.done(); iterator.next()) {
    IdealLoopTree* loop = iterator.current();
    mark_parse_predicates_useful(loop);
  }
  add_useless_predicates_to_igvn_worklist();
}

void EliminateUselessParsePredicates::mark_all_parse_predicates_useless() {
  for (ParsePredicateNode* parse_predicate : C->parse_predicates()) {
    parse_predicate->mark_useless();
  }
}

// This visitor marks all visited Parse Predicates useful.
class ParsePredicateUsefulMarker : public PredicateVisitor {
 public:
  using PredicateVisitor::visit;

  void visit(ParsePredicate& parse_predicate) override {
    parse_predicate.head()->mark_useful();
  }
};

// Mark all Parse Predicates 'loop' as useful. If 'loop' represents an outer strip mined loop, we can skip it because
// we have already processed the predicates before when we visited its counted (inner) loop.
void EliminateUselessParsePredicates::mark_parse_predicates_useful(IdealLoopTree* loop) {
  if (loop->can_apply_loop_predication() && !loop->_head->is_OuterStripMinedLoop()) {
    ParsePredicateUsefulMarker useful_marker;
    Node* entry = loop->_head->as_Loop()->skip_strip_mined()->in(LoopNode::EntryControl);
    PredicatesForLoop predicates_for_loop(entry, &useful_marker);
    predicates_for_loop.for_each();
  }
}

void EliminateUselessParsePredicates::add_useless_predicates_to_igvn_worklist() {
  for (ParsePredicateNode* parse_predicate : C->parse_predicates()) {
    if (parse_predicate->is_useless()) {
      _igvn->_worklist.push(parse_predicate);
    }
  }
}

bool RuntimePredicate::is_success_proj(Node* maybe_success_proj) {
  if (may_be_runtime_predicate_if(maybe_success_proj)) {
    IfProjNode* success_proj = maybe_success_proj->as_IfProj();
    if (is_being_folded_without_uncommon_proj(success_proj)) {
      return true;
    }
    const Deoptimization::DeoptReason deopt_reason = uncommon_trap_reason(success_proj);
    return (deopt_reason == Deoptimization::Reason_loop_limit_check ||
            deopt_reason == Deoptimization::Reason_predicate ||
            deopt_reason == Deoptimization::Reason_profile_predicate);
  } else {
    return false;
  }
}

bool RuntimePredicate::may_be_runtime_predicate_if(Node* node) {
  if (node->is_IfProj()) {
    const IfNode* if_node = node->in(0)->as_If();
    const int opcode_if = if_node->Opcode();
    if ((opcode_if == Op_If && !if_node->is_zero_trip_guard())
        || opcode_if == Op_RangeCheck) {
      return true;
    }
  }
  return false;
}

// Has the If node only the success projection left due to already folding the uncommon projection because of a constant
// bool input? This can happen during IGVN. Treat this case as being a Runtime Predicate to not miss other Predicates
// above this node when iterating through them.
bool RuntimePredicate::is_being_folded_without_uncommon_proj(const IfProjNode* success_proj) {
  IfNode* if_node = success_proj->in(0)->as_If();
  return if_node->in(1)->is_ConI() && if_node->outcnt() == 1;
}

Deoptimization::DeoptReason RuntimePredicate::uncommon_trap_reason(IfProjNode* if_proj) {
    CallStaticJavaNode* uct_call = if_proj->is_uncommon_trap_if_pattern();
    if (uct_call == nullptr) {
      return Deoptimization::Reason_none;
    }
    return Deoptimization::trap_request_reason(uct_call->uncommon_trap_request());
}

bool RuntimePredicate::is_success_proj(Node* maybe_success_proj, Deoptimization::DeoptReason deopt_reason) {
  if (may_be_runtime_predicate_if(maybe_success_proj)) {
    IfProjNode* success_proj = maybe_success_proj->as_IfProj();
    return is_being_folded_without_uncommon_proj(success_proj)
           || deopt_reason == uncommon_trap_reason(success_proj);
  } else {
    return false;
  }
}

// Insert a new predicate above the current tail of the chain. The control input of the current tail is set to the new
// predicate. The new predicate becomes the new tail of this chain.
void PredicateChain::insert_new_predicate(Predicate& new_predicate) {
  Node* new_entry_for_tail = new_predicate.tail();
  if (_tail->is_Loop()) {
    _phase->replace_loop_entry(_tail->as_Loop(), new_entry_for_tail);
  } else {
    _phase->replace_control_same_loop(_tail, new_entry_for_tail);
  }
  _tail = new_predicate.head();
}

// Insert an existing predicate into the chain by setting it as new tail. There is no control update involved.
void PredicateChain::insert_existing_predicate(Predicate& existing_predicate) {
  _tail = existing_predicate.head();
}

// This visitor clones all visited Template Assertion Predicates and sets a new input for the cloned OpaqueLoopInitNodes.
// Afterward, we initialize the template by creating an InitializedAssertionPredicate for the init and last value.
class CloneAndInitAssertionPredicates : public PredicateVisitor {
  Node* _old_target_loop_entry;
  Node* _new_init;
  PhaseIdealLoop* _phase;
  TemplateAssertionPredicateDataOutput* _node_in_target_loop;
  PredicateChain _predicate_chain;

  TemplateAssertionPredicate create_new_template(TemplateAssertionPredicate& template_assertion_predicate) {
    TemplateAssertionPredicate new_template =
        template_assertion_predicate.clone_update_opaque_init(_old_target_loop_entry, _new_init, _node_in_target_loop,
                                                               _phase);
    _predicate_chain.insert_new_predicate(new_template);
    return new_template;
  }

 public:
  using PredicateVisitor::visit;

  CloneAndInitAssertionPredicates(CountedLoopNode* target_loop_head, TemplateAssertionPredicateDataOutput* node_in_target_loop,
                                  PhaseIdealLoop* phase)
      : _old_target_loop_entry(target_loop_head->skip_strip_mined()->in(LoopNode::EntryControl)),
        _new_init(target_loop_head->init_trip()),
        _phase(phase),
        _node_in_target_loop(node_in_target_loop),
        _predicate_chain(target_loop_head, phase) {}

  void visit(TemplateAssertionPredicate& template_assertion_predicate) override {
    TemplateAssertionPredicate new_template = create_new_template(template_assertion_predicate);
    new_template.initialize(_phase, _predicate_chain);
  }
};

// Creates new Assertion Predicates at the 'target_loop_head'. A new Template Assertion Predicate is inserted with the
// new init and stride values of the target loop for each existing Template Assertion Predicate found at source loop.
// For each new Template Assertion Predicate, an init and last value Initialized Assertion Predicate is created.
void AssertionPredicates::clone_to_loop(CountedLoopNode* target_loop_head, TemplateAssertionPredicateDataOutput* node_in_target_loop) {
  CloneAndInitAssertionPredicates clone_and_init_assertion_predicates(target_loop_head, node_in_target_loop, _phase);
  Node* source_loop_entry = _source_loop_head->skip_strip_mined()->in(LoopNode::EntryControl);
  PredicatesForLoop predicates_for_loop(source_loop_entry, &clone_and_init_assertion_predicates);
  predicates_for_loop.for_each();
}

// This visitor clones all visited Template Assertion Predicates and sets a new input for the cloned OpaqueLoopInitNodes.
// Afterward, we initialize the template by creating an InitializedAssertionPredicate for the init and last value.
// The visited Template Assertion Predicates are killed.
class MoveAndInitAssertionPredicates : public PredicateVisitor {
  CloneAndInitAssertionPredicates _clone_and_init_assertion_predicates;
  PhaseIterGVN* _igvn;

 public:
  using PredicateVisitor::visit;

  MoveAndInitAssertionPredicates(CountedLoopNode* target_loop_head, TemplateAssertionPredicateDataOutput* node_in_target_loop,
                                 PhaseIdealLoop* phase)
      : _clone_and_init_assertion_predicates(target_loop_head, node_in_target_loop, phase),
        _igvn(&phase->igvn()) {}

  void visit(TemplateAssertionPredicate& template_assertion_predicate) override {
    _clone_and_init_assertion_predicates.visit(template_assertion_predicate);
    template_assertion_predicate.kill(_igvn);
  }
};

// Creates new Assertion Predicates at the 'target_loop_head'. A new Template Assertion Predicate is inserted with the
// new init and stride values of the target loop for each existing Template Assertion Predicate found at source loop.
// For each new Template Assertion Predicate, an init and last value Initialized Assertion Predicate is created.
// The existing Template Assertion Predicates at the source loop are removed.
void AssertionPredicates::move_to_loop(CountedLoopNode* target_loop_head, TemplateAssertionPredicateDataOutput* node_in_target_loop) {
  MoveAndInitAssertionPredicates move_and_init_assertion_predicates(target_loop_head, node_in_target_loop, _phase);
  Node* source_loop_entry = _source_loop_head->skip_strip_mined()->in(LoopNode::EntryControl);
  PredicatesForLoop predicates_for_loop(source_loop_entry, &move_and_init_assertion_predicates);
  predicates_for_loop.for_each();
}

// Creates a single new Template Assertion Predicates at the source loop with its init and stride value together with an
// Initialized Assertion Predicate for the init and last value.
void AssertionPredicates::create(const int if_opcode, const int scale, Node* offset, Node* range) {
  PredicateChain predicate_chain(_source_loop_head, _phase);
  TemplateAssertionPredicate template_assertion_predicate = create_new_template(if_opcode, scale, offset, range,
                                                                                predicate_chain);
  template_assertion_predicate.initialize(_phase, predicate_chain);
}

TemplateAssertionPredicate AssertionPredicates::create_new_template(const int if_opcode, const int scale, Node* offset,
                                                                    Node* range, PredicateChain& predicate_chain) {
  LoopNode* loop_head = _source_loop_head->skip_strip_mined();
  NewTemplateAssertionPredicate new_template_assertion_predicate(_source_loop_head, _phase);
  TemplateAssertionPredicate template_assertion_predicate(
      new_template_assertion_predicate.create(if_opcode, loop_head->in(LoopNode::EntryControl), scale, offset,
                                              range));
  predicate_chain.insert_new_predicate(template_assertion_predicate);
  return template_assertion_predicate;
}

// This visitor updates all visited Template Assertion Predicates by setting a new input for the OpaqueLoopStrideNodes.
// Afterward, we initialize the updated template by creating an InitializedAssertionPredicate for the init and last value.
// The old InitializedAssertionPredicates are killed.
class UpdateAndInitAssertionPredicates : public PredicateVisitor {
  Node* _new_stride;
  PhaseIdealLoop* _phase;
  uint _index_before_visit;
  PredicateChain _predicate_chain;

 public:
  using PredicateVisitor::visit;

  UpdateAndInitAssertionPredicates(Node* new_stride, CountedLoopNode* loop_head, PhaseIdealLoop* phase)
      : _new_stride(new_stride),
        _phase(phase),
        _index_before_visit(phase->C->unique()),
        _predicate_chain(loop_head, phase) {}

  void visit(TemplateAssertionPredicate& template_assertion_predicate) override {
    template_assertion_predicate.update_opaque_stride(_new_stride, &_phase->igvn());
    _predicate_chain.insert_existing_predicate(template_assertion_predicate);
    template_assertion_predicate.initialize(_phase, _predicate_chain);
  }

  void visit(InitializedAssertionPredicate& initialized_assertion_predicate) override {
    if (initialized_assertion_predicate.head()->_idx < _index_before_visit) {
      initialized_assertion_predicate.kill(&_phase->igvn());
    }
  }
};

// Update existing Assertion Predicates at the source loop with the provided stride value. The templates are first
// updated and then new Initialized Assertion Predicates are created based on the updated templates. The previously
// existing Initialized Assertion are no longer needed and killed.
void AssertionPredicates::update(const int new_stride_con) {
  Node* new_stride = create_stride(new_stride_con);
  UpdateAndInitAssertionPredicates update_assertion_predicates(new_stride, _source_loop_head, _phase);
  Node* source_loop_entry = _source_loop_head->skip_strip_mined()->in(LoopNode::EntryControl);
  PredicatesForLoop predicates_for_loop(source_loop_entry, &update_assertion_predicates);
  predicates_for_loop.for_each();
}

Node* AssertionPredicates::create_stride(const int stride_con) {
  Node* new_stride = _phase->igvn().intcon(stride_con);
  _phase->set_ctrl(new_stride, _phase->C->root());
  return new_stride;
}

TemplateAssertionPredicateBool::TemplateAssertionPredicateBool(Node* source_bool) : _source_bool(source_bool->isa_Bool()) {
#ifdef ASSERT
  // We could have already folded the BoolNode to a constant.
  if (is_not_dead()) {
    // During IGVN, we could have multiple outputs of the _source_bool, for example, when the backedge of the loop is
    // this Template Assertion Predicate is about to die and the CastII on the last value bool already folded to a
    // constant (i.e. no OpaqueLoop* nodes anymore). Then IGVN could already have commoned up the bool with the bool of
    // one of the Hoisted Check Predicates. Just check that the Template Assertion Predicate is one of the outputs.
    bool has_template_output = false;
    for (DUIterator_Fast imax, i = source_bool->fast_outs(imax); i < imax; i++) {
      Node* out = source_bool->fast_out(i);
      if (out->is_TemplateAssertionPredicate()) {
        has_template_output = true;
        break;
      }
    }
    assert(has_template_output, "must find Template Assertion Predicate as output");
  }
#endif // ASSERT
}

// Visitor to visit an OpaqueLoopStride node of a Template Assertion Predicate bool.
class OpaqueLoopStrideVisitor : public StackObj {
 public:
  virtual void visit(OpaqueLoopStrideNode* opaque_stride) = 0;
};

// Stack used when performing DFS on Template Assertion Predicate bools. Each node in the stack maintains an input index
// to the next node to visit in the DFS traversal. When a node is visited again in the stack (i.e. being on top again
// after popping all other nodes above), we increment the index to visit the next unvisited input until all inputs are
// visited. In that case, the node is dropped.
class DFSStack : public StackObj {
  Node_Stack _stack;

 public:
  explicit DFSStack(BoolNode* template_bool)
      : _stack(2) {
    _stack.push(template_bool, 1);
  }

  // Push the next unvisited input of the current node on the top of the stack.
  bool push_next_unvisited_input() {
    Node* current = _stack.node();
    for (uint index = _stack.index(); index < current->req(); index++) {
      Node* input = current->in(index);
      if (AssertionPredicateBoolOpcodes::is_valid(input)) {
        // We only care about related inputs.
        _stack.set_index(index);
        _stack.push(input, 1);
        return true;
      }
    }
    return false;
  }

  Node* top() const {
    return _stack.node();
  }

  uint index_to_previously_visited_parent() const {
    return _stack.index();
  }

  bool is_not_empty() const {
    return _stack.size() > 0;
  }

  void pop() {
    _stack.pop();
  }

  void increment_input_index() {
    _stack.set_index(_stack.index() + 1);
  }

  void replace_top_with(Node* node) {
    _stack.set_node(node);
  }
};

// Interface to transform OpaqueLoop* nodes of Template Assertion Predicate bools. The transformations must return a
// new or different existing node.
class TransformOpaqueLoopNodes : public StackObj {
 public:
  virtual Node* transform_opaque_init(OpaqueLoopInitNode* opaque_init) = 0;
  virtual Node* transform_opaque_stride(OpaqueLoopStrideNode* opaque_stride) = 0;
};

// Class to clone an Assertion Predicate bool. The BoolNode and all the nodes up to but excluding the OpaqueLoop* nodes
// are cloned. The OpaqueLoop* nodes are transformed by the provided strategy (e.g. cloned or replaced).
class CloneAssertionPredicateBool : public StackObj {
  DFSStack _stack;
  PhaseIdealLoop* _phase;
  uint _idx_before_cloning;
  Node* _ctrl_for_clones;
  DEBUG_ONLY(bool _found_init;)

  // Transform opaque_loop_node with the provided strategy. The transformation must return a new or an existing node
  // other than the OpaqueLoop* node itself.
  Node* transform_opaque_loop_node(const Node* opaque_loop_node, TransformOpaqueLoopNodes* transform_opaque_nodes) {
    Node* transformed_node;
    if (opaque_loop_node->is_OpaqueLoopInit()) {
      DEBUG_ONLY(_found_init = true;)
      transformed_node = transform_opaque_nodes->transform_opaque_init(opaque_loop_node->as_OpaqueLoopInit());
    } else {
      transformed_node = transform_opaque_nodes->transform_opaque_stride(opaque_loop_node->as_OpaqueLoopStride());
    }
    assert(transformed_node != opaque_loop_node, "OpaqueLoopNode must have been transformed");
    return transformed_node;
  }

  void pop_opaque_loop_node(Node* transformed_opaque_loop_node) {
    _stack.pop();
    assert(_stack.is_not_empty(), "must not be empty when popping an OpaqueLoopNode");
    if (must_clone_top_node(transformed_opaque_loop_node)) {
      clone_top_node(transformed_opaque_loop_node);
    } else {
      set_req_of_clone_to_parent(transformed_opaque_loop_node);
    }
    _stack.increment_input_index();
  }

  // Must only clone top node (i.e. child of 'previously_visited_parent') if not yet cloned (could visit this node a
  // second time in DFS when coming back) and parent was already cloned or transformed (i.e. child node is on the chain
  // to an OpaqueLoop* node and therefore needs to be cloned).
  bool must_clone_top_node(Node* previously_visited_parent) {
    Node* child_of_last_visited_parent = _stack.top();
    const uint index_to_previously_visited_parent = _stack.index_to_previously_visited_parent();
    return child_of_last_visited_parent->_idx < _idx_before_cloning &&
           child_of_last_visited_parent->in(index_to_previously_visited_parent) != previously_visited_parent;
  }

  // Clone the node currently on top of the stack (i.e. a descendant of an OpaqueLoop* node) and set
  // 'previously_visited_parent' as new input at the input index stored with the top node. Replace the
  // current node on top with the cloned version.
  void clone_top_node(Node* previously_visited_parent) {
    Node* child_of_last_visited_parent = _stack.top();
    const uint index_to_previously_visited_parent = _stack.index_to_previously_visited_parent();
    Node* clone = _phase->clone_and_register(child_of_last_visited_parent, _ctrl_for_clones);
    clone->set_req(index_to_previously_visited_parent, previously_visited_parent);
    _stack.replace_top_with(clone);
  }

  void set_req_of_clone_to_parent(Node* parent) const {
    Node* child_of_parent = _stack.top();
    const uint index_to_previously_visited_parent = _stack.index_to_previously_visited_parent();
    child_of_parent->set_req(index_to_previously_visited_parent, parent);
  }

  // Pop a node from the stack and clone the new node on top if the parent node was cloned or transformed before (i.e.
  // a node on the chain to an OpaqueLoop* node). The clone is set as new node on top. If the new node on top was
  // already cloned, check if the previously visited parent was also cloned. If so, we do not need to clone the new
  // node on top but can just connect it to the previously visited cloned parent. Finally, increment the input index
  // to visit the next unvisited input of the current top node.
  void pop_node(Node* previously_visited_parent) {
    _stack.pop();
    if (_stack.is_not_empty()) {
      if (must_clone_top_node(previously_visited_parent)) {
        clone_top_node(previously_visited_parent);
      } else if (is_cloned_node(previously_visited_parent)) {
        rewire_top_node_to(previously_visited_parent);
      }
      _stack.increment_input_index();
    }
  }

  bool is_cloned_node(Node* node) const {
    return node->_idx >= _idx_before_cloning;
  }

  void rewire_top_node_to(Node* previously_visited_parent) {
    _stack.top()->set_req(_stack.index_to_previously_visited_parent(), previously_visited_parent);
  }

 public:
  CloneAssertionPredicateBool(BoolNode* template_bool, Node* ctrl_for_clones, PhaseIdealLoop* phase)
      : _stack(template_bool),
        _phase(phase),
        _idx_before_cloning(phase->C->unique()),
        _ctrl_for_clones(ctrl_for_clones)
        DEBUG_ONLY(COMMA _found_init(false)) {}

  // Look for the OpaqueLoop* nodes to transform them with the strategy defined with 'transform_opaque_loop_nodes'.
  // Clone all nodes in between.
  BoolNode* clone(TransformOpaqueLoopNodes* transform_opaque_loop_nodes) {
    Node* current;
    while (_stack.is_not_empty()) {
      current = _stack.top();
      if (current->is_Opaque1()) {
        Node* transformed_node = transform_opaque_loop_node(current, transform_opaque_loop_nodes);
        pop_opaque_loop_node(transformed_node);
      } else if (!_stack.push_next_unvisited_input()) {
        pop_node(current);
      }
    }
    assert(current->is_Bool() && current->_idx >= _idx_before_cloning, "new BoolNode expected");
    assert(_found_init, "OpaqueLoopInitNode must always be found");
    return current->as_Bool();
  }
};

// Class to clone OpaqueLoop* nodes without creating duplicated nodes.
class CachedOpaqueLoopNodes {
  OpaqueLoopInitNode* _cached_opaque_new_init;
  OpaqueLoopStrideNode* _cached_new_opaque_stride;
  PhaseIdealLoop* _phase;
  Node* _new_ctrl;

 public:
  CachedOpaqueLoopNodes(PhaseIdealLoop* phase, Node* new_ctrl)
      : _cached_opaque_new_init(nullptr),
        _cached_new_opaque_stride(nullptr),
        _phase(phase),
        _new_ctrl(new_ctrl) {}

  OpaqueLoopInitNode* clone_init(OpaqueLoopInitNode* opaque_init) {
    if (_cached_opaque_new_init == nullptr) {
      _cached_opaque_new_init = _phase->clone_and_register(opaque_init, _new_ctrl)->as_OpaqueLoopInit();
    }
    return _cached_opaque_new_init;
  }

  OpaqueLoopStrideNode* clone_stride(OpaqueLoopStrideNode* opaque_stride) {
    if (_cached_new_opaque_stride == nullptr) {
      _cached_new_opaque_stride = _phase->clone_and_register(opaque_stride, _new_ctrl)->as_OpaqueLoopStride();
    }
    return _cached_new_opaque_stride;
  }
};

// The transformations of this class clone the existing OpaqueLoop* nodes without any other update.
class CloneOpaqueLoopNodes : public TransformOpaqueLoopNodes {
  CachedOpaqueLoopNodes _cached_opaque_loop_nodes;

 public:
  CloneOpaqueLoopNodes(PhaseIdealLoop* phase, Node* new_ctrl)
      : _cached_opaque_loop_nodes(phase, new_ctrl) {}

  Node* transform_opaque_init(OpaqueLoopInitNode* opaque_init) override {
    return _cached_opaque_loop_nodes.clone_init(opaque_init);
  }

  Node* transform_opaque_stride(OpaqueLoopStrideNode* opaque_stride) override {
    return _cached_opaque_loop_nodes.clone_stride(opaque_stride);
  }
};

// Clones this Template Assertion Predicate bool. This includes all nodes from the BoolNode to the OpaqueLoop* nodes.
// The cloned nodes are not updated.
BoolNode* TemplateAssertionPredicateBool::clone(Node* new_ctrl, PhaseIdealLoop* phase) {
  assert(is_not_dead(), "must not be dead");
  CloneOpaqueLoopNodes clone_opaque_loop_nodes(phase, new_ctrl);
  CloneAssertionPredicateBool clone_assertion_predicate_bool(_source_bool, new_ctrl, phase);
  return clone_assertion_predicate_bool.clone(&clone_opaque_loop_nodes);
}

// The transformations of this class clone the existing OpaqueLoop* nodes. The newly cloned OpaqueLoopInitNode
// additionally gets a new input node.
class CloneWithNewOpaqueInitInput : public TransformOpaqueLoopNodes {
  PhaseIdealLoop* _phase;
  Node* _new_opaque_init_input;
  CachedOpaqueLoopNodes _cached_opaque_loop_nodes;

 public:
  CloneWithNewOpaqueInitInput(PhaseIdealLoop* phase, Node* new_ctrl, Node* new_opaque_init_input)
      : _phase(phase),
        _new_opaque_init_input(new_opaque_init_input),
        _cached_opaque_loop_nodes(phase, new_ctrl) {}

  Node* transform_opaque_init(OpaqueLoopInitNode* opaque_init) override {
    Node* new_opaque_init = _cached_opaque_loop_nodes.clone_init(opaque_init);
    _phase->igvn().replace_input_of(new_opaque_init, 1, _new_opaque_init_input);
    return new_opaque_init;
  }

  Node* transform_opaque_stride(OpaqueLoopStrideNode* opaque_stride) override {
    return _cached_opaque_loop_nodes.clone_stride(opaque_stride);
  }
};

// Clones this Template Assertion Predicate bool. This includes all nodes from the BoolNode to the OpaqueLoop* nodes.
// The newly cloned OpaqueLoopInitNodes additionally get 'new_opaque_init_input' as a new input. The other nodes are
// not updated.
BoolNode* TemplateAssertionPredicateBool::clone_update_opaque_init(Node* new_ctrl, Node* new_opaque_init_input,
                                                                   PhaseIdealLoop* phase) {
  assert(is_not_dead(), "must not be dead");
  CloneWithNewOpaqueInitInput clone_with_new_opaque_init_input(phase, new_ctrl, new_opaque_init_input);
  CloneAssertionPredicateBool clone_assertion_predicate_bool(_source_bool, new_ctrl, phase);
  return clone_assertion_predicate_bool.clone(&clone_with_new_opaque_init_input);
}

// The transformations of this class fold the OpaqueLoop* nodes by returning their inputs.
class RemoveOpaqueLoopNodes : public TransformOpaqueLoopNodes {
  PhaseIdealLoop* _phase;

 public:
  Node* transform_opaque_init(OpaqueLoopInitNode* opaque_init) override {
    return opaque_init->in(1);
  }

  Node* transform_opaque_stride(OpaqueLoopStrideNode* opaque_stride) override {
    return opaque_stride->in(1);
  }
};

// Clones this Template Assertion Predicate bool. This includes all nodes from the BoolNode to the OpaqueLoop* nodes.
// The OpaqueLoop* nodes are not cloned but replaced by their input nodes (i.e. folding the OpaqueLoop* nodes away).
BoolNode* TemplateAssertionPredicateBool::clone_remove_opaque_loop_nodes(Node* new_ctrl, PhaseIdealLoop* phase) {
  assert(is_not_dead(), "must not be dead");
  RemoveOpaqueLoopNodes remove_opaque_loop_nodes;
  CloneAssertionPredicateBool clone_assertion_predicate_bool(_source_bool, new_ctrl, phase);
  return clone_assertion_predicate_bool.clone(&remove_opaque_loop_nodes);
}

// This visitor updates the input of OpaqueLoopStride nodes in Template Assertion Predicate bools.
class UpdateOpaqueStrideInput : public OpaqueLoopStrideVisitor {
  PhaseIterGVN* _igvn;
  Node* _new_opaque_stride_input;

 public:
  UpdateOpaqueStrideInput(PhaseIterGVN* igvn, Node* new_opaque_stride_input)
      : _igvn(igvn),
        _new_opaque_stride_input(new_opaque_stride_input) {}

  void visit(OpaqueLoopStrideNode* opaque_stride) override {
    _igvn->replace_input_of(opaque_stride, 1, _new_opaque_stride_input);
  }
};

// This class looks for OpaqueLoopStride nodes in Template Assertion Predicate bools and visits them.
class OpaqueLoopStrideNodes : public StackObj {
  DFSStack _stack;

 public:
  OpaqueLoopStrideNodes(BoolNode* template_bool) : _stack(template_bool) {}

  void findAndVisit(OpaqueLoopStrideVisitor* action) {
    while (_stack.is_not_empty()) {
      Node* current = _stack.top();
      if (current->is_OpaqueLoopStride()) {
        action->visit(current->as_OpaqueLoopStride());
        pop_visited_node();
      } else if (!_stack.push_next_unvisited_input()) {
        pop_visited_node();
      }
    };
  }

  void pop_visited_node() {
    _stack.pop();
    if (_stack.is_not_empty()) {
      _stack.increment_input_index();
    }
  }
};

// Sets 'new_opaque_stride_input' as new input of the OpaqueLoopStride node of this Template Assertion Predicate bool.
void TemplateAssertionPredicateBool::update_opaque_stride(Node* new_opaque_stride_input, PhaseIterGVN* igvn) {
  assert(is_not_dead(), "must not be dead");
  UpdateOpaqueStrideInput update_opaque_stride_input(igvn, new_opaque_stride_input);
  OpaqueLoopStrideNodes opaque_loop_stride_nodes(_source_bool);
  opaque_loop_stride_nodes.findAndVisit(&update_opaque_stride_input);
}

#ifdef ASSERT
// This visitor asserts that there are no OpaqueLoopStride nodes in Template Assertion Predicate bools.
class VerifyNoOpaqueStride : public OpaqueLoopStrideVisitor {
 public:

  void visit(OpaqueLoopStrideNode* opaque_stride) override {
    assert(false, "should not find OpaqueLoopStrideNode");
  }
};

// Visit all nodes of this Template Assertion Predicate bool. Verifies that we do not visit any OpaqueLoopStrideNode.
void TemplateAssertionPredicateBool::verify_no_opaque_stride() {
  VerifyNoOpaqueStride verify_no_opaque_stride;
  OpaqueLoopStrideNodes opaque_loop_stride_nodes(_source_bool);
  opaque_loop_stride_nodes.findAndVisit(&verify_no_opaque_stride);
}
#endif // ASSERT

TemplateAssertionPredicate
TemplateAssertionPredicate::create_and_init(Node* new_ctrl, BoolNode* new_init_bool, Node* new_last_value,
                                            TemplateAssertionPredicateDataOutput* node_in_target_loop, PhaseIdealLoop* phase) {
  TemplateAssertionPredicateNode* cloned_template = _template_assertion_predicate->clone()->as_TemplateAssertionPredicate();
  update_data_dependencies_to_clone(cloned_template, node_in_target_loop, phase);
  init_new_template(cloned_template, new_ctrl, new_init_bool, new_last_value, phase);
  return { cloned_template->as_TemplateAssertionPredicate() };
}

void TemplateAssertionPredicate::init_new_template(TemplateAssertionPredicateNode* cloned_template, Node* new_ctrl,
                                                   BoolNode* new_init_bool, Node* new_last_value,
                                                   PhaseIdealLoop* phase) {
  phase->igvn().replace_input_of(cloned_template, TemplateAssertionPredicateNode::InitValue, new_init_bool);
  phase->igvn().replace_input_of(cloned_template, TemplateAssertionPredicateNode::LastValue, new_last_value);
  phase->igvn().replace_input_of(cloned_template, 0, new_ctrl);
  phase->register_control(cloned_template, phase->get_loop(new_ctrl), new_ctrl);
}

// Update any data dependencies from this template to the new template if it meets the requirements checked with
// 'node_in_target_loop'.
void TemplateAssertionPredicate::update_data_dependencies_to_clone(TemplateAssertionPredicateNode* cloned_template,
                                                                   TemplateAssertionPredicateDataOutput* node_in_target_loop,
                                                                   PhaseIdealLoop* phase) {
  for (DUIterator_Fast imax, i = _template_assertion_predicate->fast_outs(imax); i < imax; i++) {
    Node* node = _template_assertion_predicate->fast_out(i);
    if (!node->is_CFG() && node_in_target_loop->must_update(node)) {
      phase->igvn().replace_input_of(node, 0, cloned_template);
      --i;
      --imax;
    }
  }
}

// This class creates a new Initialized Assertion Predicate.
class CreateInitializedAssertionPredicate {
  PhaseIdealLoop* _phase;

  // Create a new If or RangeCheck node to represent an Initialized Assertion Predicate and return it.
  IfNode* create_if_node(TemplateAssertionPredicateNode* template_assertion_predicate, Node* new_ctrl, BoolNode* new_bool,
                         IdealLoopTree* loop, AssertionPredicateType assertion_predicate_type) const {
    OpaqueAssertionPredicateNode* opaque_assertion_predicate_node = new OpaqueAssertionPredicateNode(new_bool);
    _phase->register_new_node(opaque_assertion_predicate_node,new_ctrl);
    IfNode* if_node = template_assertion_predicate->create_initialized_assertion_predicate(
        new_ctrl, opaque_assertion_predicate_node, assertion_predicate_type);
    _phase->register_control(if_node, loop, new_ctrl);
    return if_node;
  }

  void create_halt_node(IfFalseNode* fail_proj, IdealLoopTree* loop) {
    StartNode* start_node = _phase->C->start();
    Node* frame = new ParmNode(start_node, TypeFunc::FramePtr);
    _phase->register_new_node(frame, start_node);
    Node* halt = new HaltNode(fail_proj, frame, "Assertion Predicate cannot fail");
    _phase->igvn().add_input_to(_phase->C->root(), halt);
    _phase->register_control(halt, loop, fail_proj);
  }

  // Create the out nodes of a newly created Initialized Assertion Predicate If node which includes the projections and
  // the dedicated Halt node.
  IfTrueNode* create_if_proj_nodes(IfNode* if_node, IdealLoopTree* loop) {
    IfTrueNode* succ_proj = new IfTrueNode(if_node);
    IfFalseNode* fail_proj = new IfFalseNode(if_node);
    _phase->register_control(succ_proj, loop, if_node);
    _phase->register_control(fail_proj, loop, if_node);
    create_halt_node(fail_proj, loop);
    return succ_proj;
  }
 public:
  explicit CreateInitializedAssertionPredicate(PhaseIdealLoop* phase) : _phase(phase) {}

  InitializedAssertionPredicate create(TemplateAssertionPredicateNode* template_assertion_predicate, Node* new_ctrl,
                                       BoolNode* new_bool, AssertionPredicateType assertion_predicate_type) {
    IdealLoopTree* loop = _phase->get_loop(new_ctrl);
    IfNode* if_node = create_if_node(template_assertion_predicate, new_ctrl, new_bool, loop, assertion_predicate_type);
    return { create_if_proj_nodes(if_node, loop) };
  }
};

void TemplateAssertionPredicate::create_initialized_predicate(Node* new_ctrl, PhaseIdealLoop* phase,
                                                              TemplateAssertionPredicateBool& template_bool,
                                                              AssertionPredicateType assertion_predicate_type,
                                                              PredicateChain& predicate_chain) {
  CreateInitializedAssertionPredicate create_initialized_assertion_predicate(phase);
  BoolNode* new_bool = template_bool.clone_remove_opaque_loop_nodes(new_ctrl, phase);
  InitializedAssertionPredicate initialized_assertion_predicate =
      create_initialized_assertion_predicate.create(_template_assertion_predicate, new_ctrl, new_bool,
                                                    assertion_predicate_type);
  predicate_chain.insert_new_predicate(initialized_assertion_predicate);
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

TemplateAssertionPredicateNode* NewTemplateAssertionPredicate::create(const int if_opcode, Node* new_ctrl, const int scale,
                                                                      Node* offset, Node* range) {
  OpaqueLoopInitNode* opaque_init = create_opaque_init(new_ctrl);

  TemplateAssertionPredicateBools template_assertion_predicate_bools(_loop_head, scale, offset, range, _phase);
  bool overflow_init_value;
  BoolNode* bool_init_value = template_assertion_predicate_bools.create_for_init_value(new_ctrl, opaque_init,
                                                                                       overflow_init_value);
  bool overflow_last_value;
  BoolNode* bool_last_value = template_assertion_predicate_bools.create_for_last_value(new_ctrl, opaque_init,
                                                                                       overflow_last_value);

  return create_template_assertion_predicate(if_opcode, new_ctrl, overflow_init_value, bool_init_value,
                                             overflow_last_value, bool_last_value);
}

OpaqueLoopInitNode* NewTemplateAssertionPredicate::create_opaque_init(Node* loop_entry) {
  OpaqueLoopInitNode* opaque_init = new OpaqueLoopInitNode(_phase->C, _loop_head->init_trip());
  _phase->register_new_node(opaque_init, loop_entry);
  return opaque_init;
}

TemplateAssertionPredicateNode*
NewTemplateAssertionPredicate::create_template_assertion_predicate(const int if_opcode, Node* new_ctrl,
                                                                   bool overflow_init_value, BoolNode* bool_init_value,
                                                                   bool overflow_last_value, BoolNode* bool_last_value) {
  TemplateAssertionPredicateNode* template_assertion_predicate_node
      = new TemplateAssertionPredicateNode(new_ctrl, bool_init_value, bool_last_value,
                                           overflow_init_value ? Op_If : if_opcode,
                                           overflow_last_value ? Op_If : if_opcode);
  _phase->register_control(template_assertion_predicate_node, _phase->get_loop(new_ctrl), new_ctrl);
  return template_assertion_predicate_node;
}

// We have an Initialized Assertion Predicate if the bool input of the IfNode is an OpaqueAssertionPredicate or a ConI
// node (could be found during IGVN when this node is being folded) and we find a HaltNode on the uncommon projection path.
// If an Initialized Assertion Predicate is being folded and has already lost its uncommon projection with the HaltNode,
// (i.e. the IfNode has only the success projection left), then we treat it as Runtime Predicate.
bool InitializedAssertionPredicate::is_success_proj(const Node* success_proj) {
  if (success_proj->is_IfTrue()) {
    Node* if_node = success_proj->in(0);
    if (if_node->is_If() && if_node->outcnt() == 2) {
      return has_opaque_or_con(if_node->as_If()) && has_halt(success_proj);
    }
  }
  return false;
}

// Check if the If node has an OpaqueAssertionPredicate or a ConI node as bool input. The latter case could happen when
// an Initialized Assertion Predicate is about to be folded during IGVN.
bool InitializedAssertionPredicate::has_opaque_or_con(const IfNode* if_node) {
  Node* bool_input = if_node->in(1);
  return bool_input->is_ConI() || bool_input->Opcode() == Op_OpaqueAssertionPredicate;
}

// Check if the other projection (UCT projection) of `success_proj` has a Halt node as output.
bool InitializedAssertionPredicate::has_halt(const Node* success_proj) {
  ProjNode* other_proj = success_proj->as_IfProj()->other_if_proj();
  return other_proj->outcnt() == 1 && other_proj->unique_out()->Opcode() == Op_Halt;
}

#ifdef ASSERT
// Check that the block has at most one Parse Predicate and that we only find Regular Predicate nodes (i.e. IfProj,
// If, RangeCheck, or TemplateAssertionPredicate nodes.
void PredicateBlock::verify_block() {
  Node* next = _parse_predicate.entry(); // Skip unique Parse Predicate of this block if present
  while (next != _entry) {
    assert(!next->is_ParsePredicate(), "can only have one Parse Predicate in a block");
    const int opcode = next->Opcode();
    assert(next->is_IfProj() || next->is_TemplateAssertionPredicate() || opcode == Op_If || opcode == Op_RangeCheck,
           "Regular Predicates consist of an IfProj and an If or RangeCheck or a TemplateAssertionPredicate node");
    assert(opcode != Op_If || !next->as_If()->is_zero_trip_guard(), "should not be zero trip guard");
    next = next->in(0);
  }
}
#endif // ASSERT

// Walk over all Regular Predicates of this block (if any) and return the first node not belonging to the block
// anymore (i.e. entry to the first Regular Predicate in this block if any or `regular_predicate_proj` otherwise).
Node* PredicateBlock::skip_regular_predicates(Node* regular_predicate_proj, Deoptimization::DeoptReason deopt_reason) {
  PredicateVisitor do_nothing_visitor;
  RegularPredicateInBlockIterator regular_predicate_in_block_iterator(regular_predicate_proj, deopt_reason,
                                                                      &do_nothing_visitor);
  return regular_predicate_in_block_iterator.for_each();
}

// Applies the PredicateVisitor to each Regular Predicate in this block.
Node* PredicateInBlockIterator::for_each() {
  Node* entry = _start_node;
  if (entry->is_IfTrue() && entry->in(0)->is_ParsePredicate()) {
    ParsePredicate parse_predicate(entry, _deopt_reason);
    if (parse_predicate.is_valid()) {
      _predicate_visitor->visit(parse_predicate);
      entry = parse_predicate.entry();
    } else {
      // Parse Predicate belonging to a different Predicate Block.
      return entry;
    }
  }

  RegularPredicateInBlockIterator regular_predicate_in_block_iterator(entry, _deopt_reason, _predicate_visitor);
  return regular_predicate_in_block_iterator.for_each();
}

// Applies the PredicateVisitor to each Regular Predicate in this block.
Node* RegularPredicateInBlockIterator::for_each() {
  Node* entry = _start_node;
  while (true) {
    if (entry->is_TemplateAssertionPredicate()) {
      TemplateAssertionPredicate template_assertion_predicate(entry->as_TemplateAssertionPredicate());
      _predicate_visitor->visit(template_assertion_predicate);
      entry = template_assertion_predicate.entry();
    } else if (RuntimePredicate::is_success_proj(entry, _deopt_reason)) {
      RuntimePredicate runtime_predicate(entry->as_IfProj());
      _predicate_visitor->visit(runtime_predicate);
      entry = runtime_predicate.entry();
    } else if (InitializedAssertionPredicate::is_success_proj(entry)) {
      InitializedAssertionPredicate initialized_assertion_predicate(entry->as_IfTrue());
      _predicate_visitor->visit(initialized_assertion_predicate);
      entry = initialized_assertion_predicate.entry();
    } else {
      // Either a Parse Predicate or not a Regular Predicate. In both cases, the node does not belong to this block.
      break;
    }
  }
  return entry;
}

// Applies the PredicateVisitor to each predicate for this loop.
void PredicatesForLoop::for_each() {
  Node* entry_to_block = for_each(_start_node, Deoptimization::Reason_loop_limit_check);

  if (UseLoopPredicate) {
    if (UseProfiledLoopPredicate) {
      entry_to_block = for_each(entry_to_block, Deoptimization::Reason_profile_predicate);
    }
    for_each(entry_to_block, Deoptimization::Reason_predicate);
  }
}

Node* PredicatesForLoop::for_each(Node* current, Deoptimization::DeoptReason deopt_reason) {
  PredicateInBlockIterator predicate_in_block_iterator(current, deopt_reason, _predicate_visitor);
  return predicate_in_block_iterator.for_each();
}

PredicateEntryIterator::PredicateEntryIterator(Node* start)
    : _current(start) {}

// Is current node pointed at by iterator a predicate tail?
bool PredicateEntryIterator::has_next() const {
  if (_current->is_TemplateAssertionPredicate()) {
    return true;
  } else if (_current->is_IfProj()) {
    IfNode* if_node = _current->in(0)->as_If();
    return (if_node->is_ParsePredicate() ||
            RuntimePredicate::is_success_proj(_current) ||
            InitializedAssertionPredicate::is_success_proj(_current));
  }
  return false;
}

// Skip the current predicate pointed at by iterator by returning the input into the predicate. This could possibly be
// a non-predicate node.
Node* PredicateEntryIterator::next_predicate_entry() {
  assert(has_next(), "current must be predicate");
  if (_current->is_TemplateAssertionPredicate()) {
    _current = _current->in(0);
  } else {
    _current = _current->in(0)->in(0);
  }
  return _current;
}
