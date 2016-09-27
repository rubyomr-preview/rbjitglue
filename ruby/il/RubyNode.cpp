/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/

#include "il/RubyNode.hpp"

#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Block.hpp"
#include "il/TreeTop.hpp"

TR::Node *
Ruby::Node::axadd(TR::Node *addr, TR::Node *offset)
   {
   TR::ILOpCodes op = TR_RubyFE::SLOTSIZE == 8 ? TR::aladd : TR::aiadd;
   return TR::Node::create(op, 2, addr, offset);
   }

TR::Node *
Ruby::Node::xload(TR::SymbolReference *symRef)
   {
   return TR::Node::createLoad(symRef);
   }

TR::Node *
Ruby::Node::xadd(TR::Node *a, TR::Node *b)
   {
   return TR::Node::create(TR_RubyFE::SLOTSIZE == 8 ? TR::ladd : TR::iadd, 2, a, b);
   }

TR::Node *
Ruby::Node::xsub(TR::Node *a, TR::Node *b)
   {
   return TR::Node::create(TR_RubyFE::SLOTSIZE == 8 ? TR::lsub : TR::isub, 2, a, b);
   }

TR::Node *
Ruby::Node::xand(TR::Node *a, TR::Node *b)
   {
   return TR::Node::create(TR_RubyFE::SLOTSIZE == 8 ? TR::land : TR::iand, 2, a, b);
   }

TR::Node *
Ruby::Node::xxor(TR::Node *a, TR::Node *b)
   {
   return TR::Node::create(TR_RubyFE::SLOTSIZE == 8 ? TR::lxor : TR::ixor, 2, a, b);
   }

TR::Node *
Ruby::Node::xnot(TR::Node *a)
   {
   return TR::Node::xxor(a, TR::Node::xconst(-1));
   }

TR::Node *
Ruby::Node::ifxcmpne(TR::Node *a, TR::Node *b)
   {
   return TR::Node::createif(TR::Node::ifxcmpneOp(), a, b);
   }

TR::Node *
Ruby::Node::ifxcmpeq(TR::Node *a, TR::Node *b)
   {
   return TR::Node::createif(TR::Node::ifxcmpeqOp(), a, b);
   }

TR::Node *
Ruby::Node::xcmpge(TR::Node *a, TR::Node *b)
   {
   return TR::Node::create(TR_RubyFE::SLOTSIZE == 8 ? TR::lcmpge : TR::icmpge, 2, a, b);
   }

TR::Node *
Ruby::Node::xternary(TR::Node *cmp, TR::Node *t, TR::Node *f)
   {
   return TR::Node::create(TR_RubyFE::SLOTSIZE == 8 ? TR::lternary : TR::iternary, 3, cmp, t, f);
   }

TR::Node *
Ruby::Node::xconst(uintptr_t value)
   {
   return TR_RubyFE::SLOTSIZE == 8 ? TR::Node::lconst(value) : TR::Node::iconst(value);
   }

TR::ILOpCodes
Ruby::Node::xloadOp(TR_RubyFE *fe)
   {
   return TR::comp()->il.opCodeForDirectLoad( TR_RubyFE::slotType() );
   }

TR::Node *
Ruby::Node::xloadi(TR::SymbolReference *symRef, TR::Node *base, TR_FrontEnd * fe)
   {
   return TR::Node::createWithSymRef(TR::comp()->il.opCodeForIndirectLoad( TR_RubyFE::slotType() ), 1, 1, base, symRef);
   }

TR::Node *
Ruby::Node::xstorei(TR::SymbolReference *symRef, TR::Node *base, TR::Node *value, TR_RubyFE * fe)
   {
   return TR::Node::createWithSymRef(TR::comp()->il.opCodeForIndirectStore( TR_RubyFE::slotType() ), 2, 2, base, value, symRef);
   }

TR::Node *
Ruby::Node::createCallNode(TR::ILOpCodes opcode, TR::SymbolReference *symRef, int32_t num, ...)
   {
   // FIXME: var args loop duplicated from RubyIlGenerator::genCall 
   auto node = TR::Node::createWithSymRef(opcode, num, symRef);
   va_list args;
   va_start(args, num);
   for (int i = 0; i < num; ++i)
      {
      node->setAndIncChild(i, va_arg(args, TR::Node *));
      }
   va_end(args);
   return node;
   }

/**
 * Create a treetop for node, and insert it before the passed treetop.
 */
void
Ruby::Node::anchorBefore(TR::Node *node, TR::TreeTop *tt)
   {
   tt->insertBefore(TR::TreeTop::create(TR::comp(), TR::Node::create(TR::treetop, 1, node)));
   }

/**
 * Generate a tree top and link into the passed block. 
 */
TR::TreeTop *
Ruby::Node::genTreeTop(TR::Node *n, TR::Block *inBlock)
   {
   if (!n->getOpCode().isTreeTop())
      n = TR::Node::create(TR::treetop, 1, n);

   return inBlock->append(TR::TreeTop::create(TR::comp(), n));
   }


TR::SymbolReference *
Ruby::Node::storeToTemp(TR::Node *value, TR::Block *inBlock)
   {
   TR::SymbolReference *tempSymRef = TR::comp()->getSymRefTab()->createTemporary(TR::comp()->getMethodSymbol(), TR_RubyFE::slotType());
   TR::Node::genTreeTop(TR::Node::createStore(tempSymRef, value), inBlock);
   return tempSymRef;
   }

TR::SymbolReference *
Ruby::Node::storeToTemp(TR::Node *value, TR::TreeTop *tt)
   {
   TR::SymbolReference *tempSymRef = TR::comp()->getSymRefTab()->createTemporary(TR::comp()->getMethodSymbol(), TR_RubyFE::slotType());
   TR::Node* storeNode = TR::Node::createStore(tempSymRef, value);
   tt->insertBefore(TR::TreeTop::create(TR::comp(), TR::Node::create(TR::treetop, 1, storeNode)));
   return tempSymRef;
   }

/**
 * Load ruby thread symref for a method. 
 */
TR::Node *
Ruby::Node::loadThread(TR::ResolvedMethodSymbol * methodSymbol)
   {
   return TR::Node::createLoad(TR::comp()->getSymRefTab()->findOrCreateRubyThreadSymbolRef(methodSymbol));
   }
