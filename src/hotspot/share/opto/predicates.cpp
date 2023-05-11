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
#include "opto/predicates.hpp"

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

TemplateAssertionPredicateBlock::TemplateAssertionPredicateBlock(Node* loop_entry) : _entry(loop_entry), _last(nullptr) {
  if (loop_entry->is_TemplateAssertionPredicate()) {
    _last = loop_entry->as_TemplateAssertionPredicate();
    TemplateAssertionPredicateIterator iterator(loop_entry);
    Node* next = loop_entry;
    while (iterator.has_next()) {
      next = iterator.next();
    }
    _entry = next->in(0);
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
