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

#include "opto/cfgnode.hpp"
#include "opto/loopnode.hpp"
#include "opto/opaquenode.hpp"

/*
 * There are different kinds of predicates throughout the code. We differentiate between the following predicates:
 *
 * - Regular Predicate: This term is used to refer to a Runtime Predicate or an Assertion Predicate and can be used to
 *                      distinguish from any Parse Predicate which is not a real predicate but rather a placeholder.
 * - Parse Predicate: Added during parsing to capture the current JVM state. This predicate represents a "placeholder"
 *                    above which Regular Predicates can be created later after parsing.
 *
 *                    There are initially three Parse Predicates for each loop:
 *                    - Loop Parse Predicate:             The Parse Predicate added for Loop Predicates.
 *                    - Profiled Loop Parse Predicate:    The Parse Predicate added for Profiled Loop Predicates.
 *                    - Loop Limit Check Parse Predicate: The Parse Predicate added for a Loop Limit Check Predicate.
 * - Runtime Predicate: This term is used to refer to a Hoisted Check Predicate (either a Loop Predicate or a Profiled
 *                      Loop Predicate) or a Loop Limit Check Predicate. These predicates will be checked at runtime while
 *                      the Parse and Assertion Predicates are always removed before code generation (except for
 *                      Initialized Assertion Predicates which are kept in debug builds while being removed in product
 *                      builds).
 *     - Hoisted Check Predicate: Either a Loop Predicate or a Profiled Loop Predicate that is created during Loop
 *                                Predication to hoist a check out of a loop.
 *         - Loop Predicate:     This predicate is created to hoist a loop-invariant check or a range check of the
 *                               form "a[i*scale + offset]", where scale and offset are loop-invariant, out of a
 *                               counted loop. The hoisted check must be executed in each loop iteration. This predicate
 *                               is created during Loop Predication and is inserted above the Loop Parse Predicate. Each
 *                               predicate for a range check is accompanied by additional Assertion Predicates (see below).
 *         - Profiled Loop:      This predicate is very similar to a Loop Predicate but the check to be hoisted does not
 *           Predicate           need to be executed in each loop iteration. By using profiling information, only checks
 *                               with a high execution frequency are chosen to be replaced by a Profiled Loop Predicate.
 *                               This predicate is created during Loop Predication and is inserted above the Profiled
 *                               Loop Parse Predicate.
 *     - Loop Limit Check:   This predicate is created when transforming a loop to a counted loop to protect against
 *       Predicate           the case when adding the stride to the induction variable would cause an overflow which
 *                           will not satisfy the loop limit exit condition. This overflow is unexpected for further
 *                           counted loop optimizations and could lead to wrong results. Therefore, when this predicate
 *                           fails at runtime, we must trap and recompile the method without turning the loop into a
 *                           counted loop to avoid these overflow problems.
 *                           The predicate does not replace an actual check inside the loop. This predicate can only
 *                           be added once above the Loop Limit Check Parse Predicate for a loop.
 * - Assertion Predicate: An always true predicate which will never fail (its range is already covered by an earlier
 *                        Hoisted Check Predicate or the main-loop entry guard) but is required in order to fold away a
 *                        dead sub loop in which some data could be proven to be dead (by the type system) and replaced
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
 *                                                           by an earlier check (a Hoisted Check Predicate or the
 *                                                           main-loop entry guard).
 *
 *                        Assertion Predicates are required when removing a range check from a loop. These are inserted
 *                        either at Loop Predication or at Range Check Elimination:
 *                        - Loop Predication:        A range check inside a loop is replaced by a Hoisted Check Predicate
 *                                                   before the loop. We add two additional Template Assertion Predicates
 *                                                   from which we can later create Initialized Assertion Predicates. One
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
 *                                                   their range is covered by a corresponding Hoisted Check Predicate.
 *                        - Range Check Elimination: A range check is removed from the main-loop by changing the pre
 *                                                   and main-loop iterations. We add two additional Template Assertion
 *                                                   Predicates (see explanation in section above) and one Initialized
 *                                                   Assertion Predicate for the just removed range check. When later
 *                                                   unrolling the main-loop, we create two Initialized Assertion
 *                                                   Predicates from the Template Assertion Predicates by replacing the
 *                                                   OpaqueLoop* nodes by actual values for the unrolled loop.
 *                                                   The Initialized Assertion Predicates are always true: They are true
 *                                                   when entering the main-loop (because we adjusted the pre-loop exit
 *                                                   condition), when executing the last iteration of the main-loop
 *                                                   (because we adjusted the main-loop exit condition), and during all
 *                                                   other iterations of the main-loop in-between by implication.
 *                                                   Note that Range Check Elimination could remove additional range
 *                                                   checks which were not possible to remove with Loop Predication
 *                                                   before (for example, because no Parse Predicates were available
 *                                                   before the loop to create Hoisted Check Predicates with).
 *
 *
 * In order to group predicates and refer to them throughout the code, we introduce the following additional term:
 * - Predicate Block: A block containing all Runtime Predicates, including the Assertion Predicates for Range Check
 *                    Predicates, and the associated Parse Predicate which all share the same uncommon trap. This block
 *                    could be empty if there were no Runtime Predicates created and the Parse Predicate was already
 *                    removed.
 *                    There are three different Predicate Blocks:
 *                    - Loop Predicate Block: Groups the Loop Predicates (if any), including the Assertion Predicates,
 *                                            and the Loop Parse Predicate (if not removed, yet) together.
 *                    - Profiled Loop         Groups the Profiled Loop Predicates (if any), including the Assertion
 *                      Predicate Block:      Predicates, and the Profiled Loop Parse Predicate (if not removed, yet)
 *                                            together.
 *                    - Loop Limit Check      Groups the Loop Limit Check Predicate (if created) and the Loop Limit
 *                      Predicate Block:      Check Parse Predicate (if not removed, yet) together.
 *
 *
 * Initially, before applying any loop-splitting optimizations, we find the following structure after Loop Predication
 * (predicates inside square brackets [] do not need to exist if there are no checks to hoist or if the hoisted check
 * is not a range check and does not need a Template Assertion Predicate):
 *
 *   [Loop Predicate 1 [+ Template Assertion Predicate 1]]            \
 *   [Loop Predicate 2 [+ Template Assertion Predicate 2]]            |
 *   ...                                                              | Loop Predicate Block
 *   [Loop Predicate n [+ Template Assertion Predicate n]]            |
 * Loop Parse Predicate                                               /
 *
 *   [Profiled Loop Predicate 1 [+ Template Assertion Predicate 1]]   \
 *   [Profiled Loop Predicate 2 [+ Template Assertion Predicate 2]]   | Profiled Loop
 *   ...                                                              | Predicate Block
 *   [Profiled Loop Predicate m [+ Template Assertion Predicate n]]   |
 * Profiled Loop Parse Predicate                                      /
 *
 *   [Loop Limit Check Predicate] (at most one)                       \ Loop Limit Check
 * Loop Limit Check Parse Predicate                                   / Predicate Block
 * Loop Head
 *
 * As an example, let's look at how the predicate structure looks for the main-loop after creating pre/main/post loops
 * and applying Range Check Elimination (the order is insignificant):
 *
 * Main Loop entry (zero-trip) guard
 *   [For Loop Predicate 1: Template + two Initialized Assertion Predicates]
 *   [For Loop Predicate 2: Template + two Initialized Assertion Predicates]
 *   ...
 *   [For Loop Predicate n: Template + two Initialized Assertion Predicates]
 *
 *   [For Profiled Loop Predicate 1: Template + two Initialized Assertion Predicates]
 *   [For Profiled Loop Predicate 2: Template + two Initialized Assertion Predicates]
 *   ...
 *   [For Profiled Loop Predicate m: Template + two Initialized Assertion Predicates]
 *
 *   [For Range Check Elimination Check 1: Template + two Initialized Assertion Predicate]
 *   [For Range Check Elimination Check 2: Template + two Initialized Assertion Predicate]
 *   ...
 *   [For Range Check Elimination Check k: Template + two Initialized Assertion Predicate]
 * Main Loop Head
 */

class InitializedAssertionPredicate;
class ParsePredicate;
class Predicates;
class PredicateVisitor;
class RuntimePredicate;
class TemplateAssertionPredicate;

enum class AssertionPredicateType {
  None,
  Init_value,
  Last_value
};

// Interface to represent a C2 predicate. A predicate is either represented by a single CFG node or an If/IfProjs pair.
class Predicate : public StackObj {
 public:
  // Return the unique entry CFG node into the predicate.
  virtual Node* entry() const = 0;

  // Return the head node of the predicate which is either:
  // - The single CFG node if the predicate has only a single CFG node (i.e. Template Assertion Predicate)
  // - The If node if the predicate is an If/IfProjs pair.
  virtual Node* head() const = 0;

  // Return the tail node of the predicate which is either:
  // - The single CFG node if the predicate has only a single CFG node (i.e. Template Assertion Predicate)
  // - The IfProj success node if the predicate is an If/IfProjs pair.
  virtual Node* tail() const = 0;
};

// Interface to create a new Parse Predicate (clone of an old Parse Predicate) for either the fast or slow loop.
// The fast loop needs to create some additional clones while the slow loop can reuse old nodes.
class NewParsePredicate : public StackObj {
 public:
  virtual ParsePredicateSuccessProj* create(PhaseIdealLoop* phase, Node* new_entry,
                                            ParsePredicateSuccessProj* old_parse_predicate_success_proj) = 0;
};

// Generic predicate visitor that does nothing. Subclass this visitor to add customized actions for each predicate.
class PredicateVisitor : StackObj {
 public:
  virtual void visit(TemplateAssertionPredicate& template_assertion_predicate) {}
  virtual void visit(ParsePredicate& parse_predicate) {}
  virtual void visit(RuntimePredicate& runtime_predicate) {}
  virtual void visit(InitializedAssertionPredicate& initialized_assertion_predicate) {}
};

// Class to represent a Parse Predicate.
class ParsePredicate : public Predicate {
  ParsePredicateSuccessProj* _success_proj;
  ParsePredicateNode* _parse_predicate_node;
  Node* _entry;

  IfTrueNode* init_success_proj(const Node* parse_predicate_proj) const {
    assert(parse_predicate_proj != nullptr, "must not be null");
    return parse_predicate_proj->isa_IfTrue();
  }

  static ParsePredicateNode* init_parse_predicate(Node* parse_predicate_proj, Deoptimization::DeoptReason deopt_reason);

 public:
  ParsePredicate(Node* parse_predicate_proj, Deoptimization::DeoptReason deopt_reason)
      : _success_proj(init_success_proj(parse_predicate_proj)),
        _parse_predicate_node(init_parse_predicate(parse_predicate_proj, deopt_reason)),
        _entry(_parse_predicate_node != nullptr ? _parse_predicate_node->in(0) : parse_predicate_proj) {}

  // Returns the control input node into this Parse Predicate if it is valid. Otherwise, it returns the passed node
  // into the constructor of this class.
  Node* entry() const override {
    return _entry;
  }

  // This Parse Predicate is valid if the node passed to the constructor is a projection of a ParsePredicateNode and the
  // deopt_reason of the uncommon trap of the ParsePredicateNode matches the passed deopt_reason to the constructor.
  bool is_valid() const {
    return _parse_predicate_node != nullptr;
  }

  ParsePredicateNode* head() const override {
    assert(is_valid(), "must be valid");
    return _parse_predicate_node;
  }

  ParsePredicateSuccessProj* tail() const override {
    assert(is_valid(), "must be valid");
    return _success_proj;
  }

  ParsePredicate clone(Node* new_ctrl, NewParsePredicate* new_parse_predicate, PhaseIdealLoop* phase) {
    ParsePredicateSuccessProj* success_proj = new_parse_predicate->create(phase, new_ctrl, _success_proj);
    ParsePredicateNode* new_parse_predicate_node = success_proj->in(0)->as_ParsePredicate();
#ifdef ASSERT
    assert(_parse_predicate_node->uncommon_trap() == new_parse_predicate_node->uncommon_trap(), "same uncommon trap");
    assert(_parse_predicate_node->deopt_reason() == new_parse_predicate_node->deopt_reason(), "same deopt reason trap");
#endif // ASSERT
    return { success_proj, new_parse_predicate_node->deopt_reason() };
  }

  // Kill this Parse Predicate by marking it as useless. The Parse Predicate will be removed during the next round of IGVN.
  void kill(PhaseIterGVN* igvn) {
    _parse_predicate_node->mark_useless();
    igvn->_worklist.push(_parse_predicate_node);
  }
};

// Eliminate all useless Parse Predicates by marking them as useless and adding them to the IGVN worklist. These are
// then removed in the next IGVN round.
class EliminateUselessParsePredicates : public StackObj {
  Compile* C;
  PhaseIterGVN* _igvn;
  IdealLoopTree* _ltree_root;

  void mark_all_parse_predicates_useless();
  static void mark_parse_predicates_useful(IdealLoopTree* loop);
  void add_useless_predicates_to_igvn_worklist();

 public:
  EliminateUselessParsePredicates(PhaseIterGVN* igvn, IdealLoopTree* ltree_root)
      : C(igvn->C),
        _igvn(igvn),
        _ltree_root(ltree_root) {}

  void eliminate();
};

// Class to represent a Runtime Predicate.
class RuntimePredicate : public Predicate {
  IfProjNode* _success_proj;
  IfNode* _if_node;

  static Deoptimization::DeoptReason uncommon_trap_reason(IfProjNode* if_proj);
  static bool may_be_runtime_predicate_if(Node* node);
  static bool is_being_folded_without_uncommon_proj(const IfProjNode* success_proj);

 public:
  explicit RuntimePredicate(IfProjNode* success_proj)
      : _success_proj(success_proj),
        _if_node(success_proj->in(0)->as_If()) {
    assert(is_success_proj(success_proj), "must be valid");
  }

  Node* entry() const override {
    return _if_node->in(0);
  }

  IfNode* head() const override {
    return _if_node;
  }

  Node* tail() const override {
    return _success_proj;
  }

  static bool is_success_proj(Node* maybe_success_proj);
  static bool is_success_proj(Node* maybe_success_proj, Deoptimization::DeoptReason deopt_reason);
};

// This class represents a chain of predicates above a loop. We build the chain by inserting either existing predicates
// at the loop or by inserting new predicates which also update control.
class PredicateChain : public StackObj {
  Node* _tail; // The current tail of this predicate chain which is initially the loop node itself.
  PhaseIdealLoop* _phase;

 public:
  PredicateChain(LoopNode* loop_node, PhaseIdealLoop* phase) : _tail(loop_node->skip_strip_mined()), _phase(phase) {}

  void insert_new_predicate(Predicate& new_predicate);
  void insert_existing_predicate(Predicate& existing_predicate);
};

// Class to create Assertion Predicates at the target loop by moving the templates from the source to the target loop
// and creating initialized predicates from them.
class AssertionPredicates : public StackObj {
  CountedLoopNode* _source_loop_head;
  PhaseIdealLoop* _phase;

  TemplateAssertionPredicate create_new_template(int if_opcode, int scale, Node* offset, Node* range,
                                                 PredicateChain& predicate_chain);
  Node* create_stride(int stride_con);

 public:
  AssertionPredicates(CountedLoopNode* source_loop_head, PhaseIdealLoop* phase)
      : _source_loop_head(source_loop_head),
        _phase(phase) {}


  void clone_to_loop(CountedLoopNode* target_loop_head, TemplateAssertionPredicateDataOutput* node_in_target_loop);
  void move_to_loop(CountedLoopNode* target_loop_head, TemplateAssertionPredicateDataOutput* node_in_target_loop);
  void create(int if_opcode, int scale, Node* offset, Node* range);
  void update(int new_stride_con);
};

// Utility class for querying Assertion Predicate bool opcodes.
class AssertionPredicateBoolOpcodes : public StackObj {
 public:
  // Is 'n' a node that could be part of an Assertion Predicate bool (i.e. could be found on the input chain of an
  // Assertion Predicate bool up to and including the OpaqueLoop* nodes)?
  static bool is_valid(const Node* n) {
    const int opcode = n->Opcode();
    return (opcode == Op_OpaqueLoopInit ||
            opcode == Op_OpaqueLoopStride ||
            n->is_Bool() ||
            n->is_Cmp() ||
            opcode == Op_AndL ||
            opcode == Op_OrL ||
            opcode == Op_RShiftL ||
            opcode == Op_LShiftL ||
            opcode == Op_LShiftI ||
            opcode == Op_AddL ||
            opcode == Op_AddI ||
            opcode == Op_MulL ||
            opcode == Op_MulI ||
            opcode == Op_SubL ||
            opcode == Op_SubI ||
            opcode == Op_ConvI2L ||
            opcode == Op_CastII);
  }
};

// Class that represents either the BoolNode for the initial value or the last value of a Template Assertion Predicate.
class TemplateAssertionPredicateBool : public StackObj {
  BoolNode* _source_bool;

 public:
  explicit TemplateAssertionPredicateBool(Node* source_bool);

  bool is_not_dead() const {
    return _source_bool != nullptr;
  }

  BoolNode* clone(Node* new_ctrl, PhaseIdealLoop* phase);
  BoolNode* clone_update_opaque_init(Node* new_ctrl, Node* new_opaque_init_input, PhaseIdealLoop* phase);
  BoolNode* clone_remove_opaque_loop_nodes(Node* new_ctrl, PhaseIdealLoop* phase);
  void update_opaque_stride(Node* new_opaque_stride_input, PhaseIterGVN* igvn);
  DEBUG_ONLY(void verify_no_opaque_stride());
};

// Class to represent a Template Assertion Predicate.
class TemplateAssertionPredicate : public Predicate {
  TemplateAssertionPredicateNode* _template_assertion_predicate;
  TemplateAssertionPredicateBool _init_value_bool;
  TemplateAssertionPredicateBool _last_value_bool;

  TemplateAssertionPredicate create_and_init(Node* new_ctrl, BoolNode* new_init_bool, Node* new_last_value,
                                             TemplateAssertionPredicateDataOutput* node_in_target_loop, PhaseIdealLoop* phase);
  static void init_new_template(TemplateAssertionPredicateNode* cloned_template, Node* new_ctrl, BoolNode* new_init_bool,
                                Node* new_last_value, PhaseIdealLoop* phase);
  void update_data_dependencies_to_clone(TemplateAssertionPredicateNode* cloned_template_assertion_predicate,
                                         TemplateAssertionPredicateDataOutput* node_in_target_loop, PhaseIdealLoop* phase);
  void create_initialized_predicate(Node* new_ctrl, PhaseIdealLoop* phase, TemplateAssertionPredicateBool &template_bool,
                                    AssertionPredicateType assertion_predicate_type, PredicateChain& predicate_chain);

 public:
  TemplateAssertionPredicate(TemplateAssertionPredicateNode* template_assertion_predicate)
      : _template_assertion_predicate(template_assertion_predicate),
        _init_value_bool(TemplateAssertionPredicateBool(template_assertion_predicate->in(TemplateAssertionPredicateNode::InitValue))),
        _last_value_bool(TemplateAssertionPredicateBool(template_assertion_predicate->in(TemplateAssertionPredicateNode::LastValue)))
  {}

  Node* entry() const override {
    return _template_assertion_predicate->in(0);
  }

  TemplateAssertionPredicateNode* head() const override {
    return _template_assertion_predicate;
  }

  TemplateAssertionPredicateNode* tail() const override {
    return _template_assertion_predicate;
  }

  // Clones this Template Assertion Predicate which will also clone its bools. The cloned nodes are not updated in any way.
  TemplateAssertionPredicate clone(Node* new_ctrl, TemplateAssertionPredicateDataOutput* node_in_target_loop, PhaseIdealLoop* phase) {
    BoolNode* new_init_bool = _init_value_bool.clone(new_ctrl, phase);
    Node* new_last_value;
    if (_last_value_bool.is_not_dead()) {
      new_last_value = _last_value_bool.clone(new_ctrl, phase);
    } else {
      new_last_value = phase->igvn().intcon(1);
    }
    return create_and_init(new_ctrl, new_init_bool, new_last_value, node_in_target_loop, phase);
  }

  // Same as clone() but the cloned OpaqueLoopInitNode nodes will get 'new_opaque_cloned nodes' as new input.
  TemplateAssertionPredicate clone_update_opaque_init(Node* new_ctrl, Node* new_opaque_init_input,
                                                      TemplateAssertionPredicateDataOutput* node_in_target_loop,
                                                      PhaseIdealLoop* phase) {
    BoolNode* new_init_bool = _init_value_bool.clone_update_opaque_init(new_ctrl, new_opaque_init_input, phase);
    Node* new_last_value;
    if (_last_value_bool.is_not_dead()) {
      new_last_value = _last_value_bool.clone_update_opaque_init(new_ctrl, new_opaque_init_input, phase);
    } else {
      new_last_value = phase->igvn().intcon(1);
    }
    return create_and_init(new_ctrl, new_init_bool, new_last_value, node_in_target_loop, phase);
  }

  // Update the input of the OpaqueLoopStrideNode of the last value bool of this Template Assertion Predicate to
  // the provided 'new_opaque_stride_input'.
  void update_opaque_stride(Node* new_opaque_stride_input, PhaseIterGVN* igvn) {
    // Only last value bool has OpaqueLoopStrideNode.
    DEBUG_ONLY(_init_value_bool.verify_no_opaque_stride());
    if (_last_value_bool.is_not_dead()) {
      _last_value_bool.update_opaque_stride(new_opaque_stride_input, igvn);
    }
  }

  // Create an Initialized Assertion Predicate from this Template Assertion Predicate for the init and the last value.
  // This is done by cloning the Template Assertion Predicate bools and removing the OpaqueLoop* nodes (i.e. folding
  // them away and using their inputs instead).
  void initialize(PhaseIdealLoop* phase, PredicateChain& predicate_chain) {
    Node* new_ctrl = entry();
    if (_last_value_bool.is_not_dead()) {
      create_initialized_predicate(new_ctrl, phase, _last_value_bool, AssertionPredicateType::Last_value,
                                   predicate_chain);
    }
    create_initialized_predicate(new_ctrl, phase, _init_value_bool, AssertionPredicateType::Init_value, predicate_chain);
  }

  // Kill this Template Assertion Predicate by marking it as useless. The Template Assertion Predicate will be removed
  // during the next round of IGVN.
  void kill(PhaseIterGVN* igvn) {
    _template_assertion_predicate->mark_useless();
    igvn->_worklist.push(_template_assertion_predicate);
  }
};

// Class to create bool nodes for a new Template Assertion Predicate.
class TemplateAssertionPredicateBools : public StackObj {
  PhaseIdealLoop* _phase;
  CountedLoopNode* _loop_head;
  jint _stride;
  int _scale;
  Node* _offset;
  Node* _range;
  bool _upper;

  Node* create_last_value(Node* new_ctrl, OpaqueLoopInitNode* opaque_init);

 public:
  TemplateAssertionPredicateBools(CountedLoopNode* loop_head, int scale, Node* offset, Node* range,
                                  PhaseIdealLoop* phase)
      : _phase(phase),
        _loop_head(loop_head),
        _stride(_loop_head->stride()->get_int()),
        _scale(scale),
        _offset(offset),
        _range(range),
        _upper((_stride > 0) != (_scale > 0))  // Make sure rc_predicate() chooses "scale*init + offset" case.
        {}

  BoolNode* create_for_init_value(Node* new_ctrl, OpaqueLoopInitNode* opaque_init, bool& overflow) {
    return _phase->rc_predicate(new_ctrl, _scale, _offset, opaque_init, nullptr, _stride, _range, _upper,
                                overflow);
  }

  BoolNode* create_for_last_value(Node* new_ctrl, OpaqueLoopInitNode* opaque_init, bool& overflow) {
    Node* last_value = create_last_value(new_ctrl, opaque_init);
    return _phase->rc_predicate(new_ctrl, _scale, _offset, last_value, nullptr, _stride, _range, _upper,
                                overflow);
  }
};

// Class to create a new Template Assertion Predicate and insert it into the graph.
class NewTemplateAssertionPredicate : public StackObj {
  CountedLoopNode* _loop_head;
  PhaseIdealLoop* _phase;

  OpaqueLoopInitNode* create_opaque_init(Node* loop_entry);
  TemplateAssertionPredicateNode* create_template_assertion_predicate(int if_opcode, Node* loop_entry,
                                                                      bool overflow_init_value, BoolNode* bool_init_value,
                                                                      bool overflow_last_value, BoolNode* bool_last_value);

 public:
  explicit NewTemplateAssertionPredicate(CountedLoopNode* loop_head, PhaseIdealLoop* phase)
      : _loop_head(loop_head),
        _phase(phase) {}

  TemplateAssertionPredicateNode* create(int if_opcode, Node* new_ctrl, int scale, Node* offset, Node* range);

};

// Interface to check if an output data node of a Template Assertion Predicate node must be updated to the newly cloned
// Template Assertion Predicate node. This decision is done based on whether the output node belongs to the original,
// not yet cloned loop body or not. This can be achieved by comparing node indices and/or old->new mappings.
class TemplateAssertionPredicateDataOutput : public StackObj {
 public:
  virtual bool must_update(Node* output_data_node) = 0;
};

// This class returns true if the output data node is part of the cloned loop body.
class NodeInClonedLoop : public TemplateAssertionPredicateDataOutput {
  uint _first_node_index_in_cloned_loop;

 public:
  explicit NodeInClonedLoop(uint first_node_index_in_cloned_loop)
      : _first_node_index_in_cloned_loop(first_node_index_in_cloned_loop) {}

  bool must_update(Node* output_data_node) override {
    return output_data_node->_idx >= _first_node_index_in_cloned_loop;
  }
};

// This class returns true if the output data node belongs to the original loop body.
class NodeInOriginalLoop : public TemplateAssertionPredicateDataOutput {
  uint _first_node_index_in_cloned_loop;
  Node_List* _old_new;

 public:
  explicit NodeInOriginalLoop(uint first_node_index_in_cloned_loop, Node_List* old_new)
      : _first_node_index_in_cloned_loop(first_node_index_in_cloned_loop),
        _old_new(old_new) {}

  // Check if 'output_data_node' is not a cloned node (i.e. < _first_node_index_in_cloned_loop) and if we've created a
  // clone from it (with _old_new). If there is a clone, we know that 'output_data_node' belongs to the original loop.
  bool must_update(Node* output_data_node) override {
    if (output_data_node->_idx < _first_node_index_in_cloned_loop) {
      Node* cloned_node = (*_old_new)[output_data_node->_idx];
      return cloned_node != nullptr && cloned_node->_idx >= _first_node_index_in_cloned_loop;
    } else {
      return false;
    }
  }
};

// Class to represent an Initialized Assertion Predicate.
class InitializedAssertionPredicate : public Predicate {
  IfTrueNode* _success_proj;
  IfNode* _if_node;

  static bool has_opaque_or_con(const IfNode* if_node);
  static bool has_halt(const Node* success_proj);

 public:
  InitializedAssertionPredicate(IfTrueNode* success_proj)
      : _success_proj(success_proj),
        _if_node(success_proj->in(0)->as_If()) {}

  Node* entry() const override {
    return _if_node->in(0);
  }

  IfNode* head() const override {
    return _if_node;
  }

  IfTrueNode* tail() const override {
    return _success_proj;
  };

  static bool is_success_proj(const Node* success_proj);

  // Kill this Initialized Assertion Predicate by setting the bool input of the If node representing it to true.
  // The Initialized Assertion Predicate will be removed during the next round of IGVN.
  void kill(PhaseIterGVN* igvn) {
    igvn->replace_input_of(_if_node, 1, igvn->intcon(1));
  }
};

// This class represents a Predicate Block (i.e. either a Loop Predicate Block, a Profiled Loop Predicate Block,
// or a Loop Limit Check Predicate Block). It contains zero or more Regular Predicates followed by a Parse Predicate
// which, however, does not need to exist (we could already have decided to remove Parse Predicates for this loop).
// Use this class for queries about the predicates in this block. For iteration inside a Predicate Block, use
// BlockPredicateIterator.
// For a loop that was split from another loop, we will only find Assertion Predicates. We group them together as a
// single Predicate Block without a Parse Predicate (Assertion Predicates cannot be mapped to an uncommon trap).
class PredicateBlock : public StackObj {
  ParsePredicate _parse_predicate; // Could be missing.
  Node* _entry;

  static Node* skip_regular_predicates(Node* regular_predicate_proj, Deoptimization::DeoptReason deopt_reason);
  DEBUG_ONLY(void verify_block();)

 public:
  PredicateBlock(Node* predicate_proj, Deoptimization::DeoptReason deopt_reason)
      : _parse_predicate(predicate_proj, deopt_reason),
        _entry(skip_regular_predicates(_parse_predicate.entry(), deopt_reason)) {
    DEBUG_ONLY(verify_block();)
  }

  // Returns the control input node into this Regular Predicate block. This is either:
  // - The control input to the first If node in the block representing a Regular Predicate if we've created at least one
  //   Runtime Predicate during Loop Predication (could be a Runtime Predicate or a Template Assertion Predicate if
  //   we've already folded the Runtime Predicate away).
  // - The control input node to the Parse Predicate if there is only a Parse Predicate and no Regular Predicate.
  // - The same node initially passed to the constructor if this Predicate block is empty (i.e. no Parse or Regular
  //   Predicate).
  Node* entry() const {
    return _entry;
  }

  bool is_non_empty() const {
    return has_parse_predicate() || has_runtime_predicates();
  }

  bool has_parse_predicate() const {
    return _parse_predicate.is_valid();
  }

  bool has_runtime_predicates() const {
    return _parse_predicate.entry() != _entry;
  }

  ParsePredicateSuccessProj* parse_predicate_success_proj() const {
    assert(has_parse_predicate(), "must be valid");
    return _parse_predicate.tail();
  }
};

// This class represents all predicates before a loop. Use this class for queries about the predicates in this block.
// For iteration, either use the class PredicatesForLoop.
class Predicates : public StackObj {
  PredicateBlock _loop_limit_check_predicate_block;
  PredicateBlock _profiled_loop_predicate_block;
  PredicateBlock _loop_predicate_block;
  Node* _entry;

 public:
  explicit Predicates(Node* loop_entry)
      : _loop_limit_check_predicate_block(loop_entry, Deoptimization::Reason_loop_limit_check),
        _profiled_loop_predicate_block(_loop_limit_check_predicate_block.entry(),
                                       Deoptimization::Reason_profile_predicate),
        _loop_predicate_block(_profiled_loop_predicate_block.entry(),
                              Deoptimization::Reason_predicate),
        _entry(_loop_predicate_block.entry()) {
    assert(loop_entry != nullptr, "must not be null");
  }

  // Returns the control input the first predicate if there are any predicates. If there are no predicates, the same
  // node initially passed to the constructor is returned.
  Node* entry() const {
    return _entry;
  }

  const PredicateBlock* loop_predicate_block() const {
    return &_loop_predicate_block;
  }

  const PredicateBlock* profiled_loop_predicate_block() const {
    return &_profiled_loop_predicate_block;
  }

  const PredicateBlock* loop_limit_check_predicate_block() const {
    return &_loop_limit_check_predicate_block;
  }
};

// Iterator that applies a PredicateVisitor to each Regular Predicate in the block specified by the deopt_reason.
class RegularPredicateInBlockIterator : public StackObj {
  Deoptimization::DeoptReason _deopt_reason;
  Node* _start_node;
  PredicateVisitor* _predicate_visitor;

 public:
  RegularPredicateInBlockIterator(Node* start_node, Deoptimization::DeoptReason deopt_reason,
                                  PredicateVisitor* predicate_visitor)
      : _deopt_reason(deopt_reason),
        _start_node(start_node),
        _predicate_visitor(predicate_visitor) {}

  Node* for_each();
};

// Iterator that applies a PredicateVisitor to each predicate in the block specified by the deopt_reason.
class PredicateInBlockIterator : public StackObj {
  Node* _start_node;
  Deoptimization::DeoptReason _deopt_reason;
  PredicateVisitor* _predicate_visitor;

 public:
  PredicateInBlockIterator(Node* start_node, Deoptimization::DeoptReason deopt_reason, PredicateVisitor* predicate_visitor)
          : _start_node(start_node),
            _deopt_reason(deopt_reason),
            _predicate_visitor(predicate_visitor) {}

  Node* for_each();
};

// Predicate iterator that applies a PredicateVisitor to each predicate belonging to the same loop to which the passed
// node belongs to.
class PredicatesForLoop : public StackObj {
  Node* _start_node;
  PredicateVisitor* _predicate_visitor;

  Node* for_each(Node* next, Deoptimization::DeoptReason deopt_reason);

 public:
  PredicatesForLoop(Node* start_node, PredicateVisitor* predicate_visitor)
      : _start_node(start_node),
        _predicate_visitor(predicate_visitor) {}

  void for_each();
};

// Special predicate iterator that can be used to walk through predicates, regardless if they all belong to the same
// loop or not (i.e. leftovers from already folded nodes). The iterator always returns the next entry to a
// predicate.
class PredicateEntryIterator : public StackObj {
  Node* _current;

 public:
  PredicateEntryIterator(Node* start);

  bool has_next() const;
  Node* next_predicate_entry();
};

#ifdef ASSERT
// This class verifies that there are no Parse Predicates and no Runtime Predicates.
class VerifyOnlyAssertionPredicates : public PredicateVisitor {
 public:
  using PredicateVisitor::visit;

  void visit(ParsePredicate& parse_predicate) override {
    parse_predicate.head()->dump();
    assert(false, "should not find Parse Predicate");
  }

  void visit(RuntimePredicate& runtime_predicate) override {
    runtime_predicate.head()->dump();
    assert(false, "should not find Runtime Predicate");
  }

  static void verify(Node* loop_entry) {
    VerifyOnlyAssertionPredicates verify_only_assertion_predicates;
    PredicatesForLoop predicates_for_loop(loop_entry, &verify_only_assertion_predicates);
    predicates_for_loop.for_each();
  }
};
#endif // ASSERT

#endif // SHARE_OPTO_PREDICATES_HPP
