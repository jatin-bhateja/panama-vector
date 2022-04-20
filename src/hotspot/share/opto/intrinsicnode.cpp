/*
 * Copyright (c) 2015, 2020, Oracle and/or its affiliates. All rights reserved.
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
#include "opto/intrinsicnode.hpp"
#include "opto/addnode.hpp"
#include "opto/mulnode.hpp"
#include "opto/memnode.hpp"
#include "opto/phaseX.hpp"

//=============================================================================
// Do not match memory edge.
uint StrIntrinsicNode::match_edge(uint idx) const {
  return idx == 2 || idx == 3;
}

//------------------------------Ideal------------------------------------------
// Return a node which is more "ideal" than the current node.  Strip out
// control copies
Node* StrIntrinsicNode::Ideal(PhaseGVN* phase, bool can_reshape) {
  if (remove_dead_region(phase, can_reshape)) return this;
  // Don't bother trying to transform a dead node
  if (in(0) && in(0)->is_top())  return NULL;

  if (can_reshape) {
    Node* mem = phase->transform(in(MemNode::Memory));
    // If transformed to a MergeMem, get the desired slice
    uint alias_idx = phase->C->get_alias_index(adr_type());
    mem = mem->is_MergeMem() ? mem->as_MergeMem()->memory_at(alias_idx) : mem;
    if (mem != in(MemNode::Memory)) {
      set_req_X(MemNode::Memory, mem, phase);
      return this;
    }
  }
  return NULL;
}

//------------------------------Value------------------------------------------
const Type* StrIntrinsicNode::Value(PhaseGVN* phase) const {
  if (in(0) && phase->type(in(0)) == Type::TOP) return Type::TOP;
  return bottom_type();
}

uint StrIntrinsicNode::size_of() const { return sizeof(*this); }

//=============================================================================
//------------------------------Ideal------------------------------------------
// Return a node which is more "ideal" than the current node.  Strip out
// control copies
Node* StrCompressedCopyNode::Ideal(PhaseGVN* phase, bool can_reshape) {
  return remove_dead_region(phase, can_reshape) ? this : NULL;
}

//=============================================================================
//------------------------------Ideal------------------------------------------
// Return a node which is more "ideal" than the current node.  Strip out
// control copies
Node* StrInflatedCopyNode::Ideal(PhaseGVN* phase, bool can_reshape) {
  return remove_dead_region(phase, can_reshape) ? this : NULL;
}

//=============================================================================
//------------------------------match_edge-------------------------------------
// Do not match memory edge
uint EncodeISOArrayNode::match_edge(uint idx) const {
  return idx == 2 || idx == 3; // EncodeISOArray src (Binary dst len)
}

//------------------------------Ideal------------------------------------------
// Return a node which is more "ideal" than the current node.  Strip out
// control copies
Node* EncodeISOArrayNode::Ideal(PhaseGVN* phase, bool can_reshape) {
  return remove_dead_region(phase, can_reshape) ? this : NULL;
}

//------------------------------Value------------------------------------------
const Type* EncodeISOArrayNode::Value(PhaseGVN* phase) const {
  if (in(0) && phase->type(in(0)) == Type::TOP) return Type::TOP;
  return bottom_type();
}

//------------------------------CopySign-----------------------------------------
CopySignDNode* CopySignDNode::make(PhaseGVN& gvn, Node* in1, Node* in2) {
  return new CopySignDNode(in1, in2, gvn.makecon(TypeD::ZERO));
}

//------------------------------Signum-------------------------------------------
SignumDNode* SignumDNode::make(PhaseGVN& gvn, Node* in) {
  return new SignumDNode(in, gvn.makecon(TypeD::ZERO), gvn.makecon(TypeD::ONE));
}

SignumFNode* SignumFNode::make(PhaseGVN& gvn, Node* in) {
  return new SignumFNode(in, gvn.makecon(TypeF::ZERO), gvn.makecon(TypeF::ONE));
}

Node* CompressBitsNode::Ideal(PhaseGVN* phase, bool can_reshape) {
  Node* src = in(1);
  Node* mask = in(2);
  if (bottom_type()->isa_int()) {
    if (mask->Opcode() == Op_LShiftI && phase->type(mask->in(1))->is_int()->is_con()) {
      // compress(x, 1 << n) == (x >> n & 1)
      if (phase->type(mask->in(1))->higher_equal(TypeInt::ONE)) {
        Node* rshift = phase->transform(new RShiftINode(in(1), mask->in(2)));
        return new AndINode(rshift, phase->makecon(TypeInt::ONE));
      // compress(x, -1 << n) == x >>> n
      } else if (phase->type(mask->in(1))->higher_equal(TypeInt::MINUS_1)) {
        return new URShiftINode(in(1), mask->in(2));
      }
    }
    // compress(expand(x, m), m) == x & compress(m, m)
    if (src->Opcode() == Op_ExpandBits &&
        src->in(2) == mask) {
      Node* compr = phase->transform(new CompressBitsNode(mask, mask, TypeInt::INT));
      return new AndINode(compr, src->in(1));
    }
  } else {
    assert(bottom_type()->isa_long(), "");
    if (mask->Opcode() == Op_LShiftL && phase->type(mask->in(1))->is_long()->is_con()) {
      // compress(x, 1 << n) == (x >> n & 1)
      if (phase->type(mask->in(1))->higher_equal(TypeLong::ONE)) {
        Node* rshift = phase->transform(new RShiftLNode(in(1), mask->in(2)));
        return new AndLNode(rshift, phase->makecon(TypeLong::ONE));
      // compress(x, -1 << n) == x >>> n
      } else if (phase->type(mask->in(1))->higher_equal(TypeLong::MINUS_1)) {
        return new URShiftLNode(in(1), mask->in(2));
      }
    }
    // compress(expand(x, m), m) == x & compress(m, m)
    if (src->Opcode() == Op_ExpandBits &&
        src->in(2) == mask) {
      Node* compr = phase->transform(new CompressBitsNode(mask, mask, TypeLong::LONG));
      return new AndLNode(compr, src->in(1));
    }
  }
  return NULL;
}

Node* compress_expand_identity(PhaseGVN* phase, Node* n) {
  if (n->bottom_type()->isa_int()) {
    // compress(x, 0) == 0
    if(phase->type(n->in(2))->higher_equal(TypeInt::ZERO)) return n->in(2);
    // compress(x, -1) == x
    if(phase->type(n->in(2))->higher_equal( TypeInt::MINUS_1)) return n->in(1);
  } else {
    assert(n->bottom_type()->isa_long(), "");
    // compress(x, 0) == 0
    if(phase->type(n->in(2))->higher_equal(TypeLong::ZERO)) return n->in(2);
    // compress(x, -1) == x
    if(phase->type(n->in(2))->higher_equal( TypeLong::MINUS_1)) return n->in(1);
  }
  return n;
}

Node* CompressBitsNode::Identity(PhaseGVN* phase) {
  return compress_expand_identity(phase, this);
}


Node* ExpandBitsNode::Ideal(PhaseGVN* phase, bool can_reshape) {
  Node* src = in(1);
  Node* mask = in(2);
  if (bottom_type()->isa_int()) {
    if (mask->Opcode() == Op_LShiftI && phase->type(mask->in(1))->is_int()->is_con()) {
      // expand(x, 1 << n) == (x & 1) << n
      if (phase->type(mask->in(1))->higher_equal(TypeInt::ONE)) {
        Node* andnode = phase->transform(new AndINode(in(1), phase->makecon(TypeInt::ONE)));
        return new LShiftINode(andnode, mask->in(2));
      // expand(x, -1 << n) == x << n
      } else if (phase->type(mask->in(1))->higher_equal(TypeInt::MINUS_1)) {
        return new LShiftINode(in(1), mask->in(2));
      }
    }
    // expand(compress(x, m), m) == x & m
    if (src->Opcode() == Op_CompressBits &&
        src->in(2) == mask) {
      return new AndINode(src->in(1), mask);
    }
  } else {
    assert(bottom_type()->isa_long(), "");
    if (mask->Opcode() == Op_LShiftL && phase->type(mask->in(1))->is_long()->is_con()) {
      // expand(x, 1 << n) == (x & 1) << n
      if (phase->type(mask->in(1))->higher_equal(TypeLong::ONE)) {
        Node* andnode = phase->transform(new AndLNode(in(1), phase->makecon(TypeLong::ONE)));
        return new LShiftLNode(andnode, mask->in(2));
      // expand(x, -1 << n) == x << n
      } else if (phase->type(mask->in(1))->higher_equal(TypeLong::MINUS_1)) {
        return new LShiftLNode(in(1), mask->in(2));
      }
    }
    // expand(compress(x, m), m) == x & m
    if (src->Opcode() == Op_CompressBits &&
        src->in(2) == mask) {
      return new AndLNode(src->in(1), mask);
    }
  }
  return NULL;
}

Node* ExpandBitsNode::Identity(PhaseGVN* phase) {
  return compress_expand_identity(phase, this);
}
