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

#ifndef RUBY_NODE_INCL
#define RUBY_NODE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef RUBY_NODE_CONNECTOR
#define RUBY_NODE_CONNECTOR
namespace Ruby { class  Node; }
namespace Ruby { typedef Ruby::Node NodeConnector; }
#endif

#include "il/OMRNode.hpp"

#include <stddef.h>               // for NULL
#include <stdint.h>               // for uint16_t
#include "il/ILOpCodes.hpp"       // for ILOpCodes
#include "infra/Annotations.hpp"  // for OMR_EXTENSIBLE
#include "ruby/env/RubyFE.hpp"

namespace TR { class Node; }

namespace Ruby
{


class OMR_EXTENSIBLE Node : public OMR::NodeConnector
   {
public:
   //Forwarding constructors
   Node() : OMR::NodeConnector() {}

   Node(TR::Node *originatingByteCodeNode, TR::ILOpCodes op,
        uint16_t numChildren, OptAttributes * oa = NULL)
      : OMR::NodeConnector(originatingByteCodeNode, op, numChildren, oa)
      {}

   Node(TR::Node *from, uint16_t numChildren = 0)
      : OMR::NodeConnector(from, numChildren)
      {}


   // Convenience methods
   static TR::Node *axadd(TR::Node *addr, TR::Node *offset);

   static TR::Node *xload(TR::SymbolReference *symRef);

   static TR::ILOpCodes xcallOp()
      { return TR_RubyFE::SLOTSIZE == 8 ? TR::lcall : TR::icall; }

   static TR::Node *xloadi(TR::SymbolReference *symRef, TR::Node *base, TR_FrontEnd * fe);

   static TR::Node *xstorei(TR::SymbolReference *symRef, TR::Node *base, TR::Node *value, TR_RubyFE * fe);

   static TR::Node *xadd(TR::Node *a, TR::Node *b);
   static TR::Node *xsub(TR::Node *a, TR::Node *b);
   static TR::Node *xand(TR::Node *a, TR::Node *b);
   static TR::Node *xxor(TR::Node *a, TR::Node *b);
   static TR::Node *xnot(TR::Node *a);

   static TR::ILOpCodes ifxcmpneOp()
      { return TR_RubyFE::SLOTSIZE == 8 ? TR::iflcmpne : TR::ificmpne; }
   static TR::ILOpCodes ifxcmpeqOp()
      { return TR_RubyFE::SLOTSIZE == 8 ? TR::iflcmpeq : TR::ificmpeq; }

   static TR::Node *ifxcmpne(TR::Node *a, TR::Node *b);
   static TR::Node *ifxcmpeq(TR::Node *a, TR::Node *b);

   static TR::Node *xcmpge(TR::Node *a, TR::Node *b);
   static TR::Node *xternary(TR::Node *cmp, TR::Node *t, TR::Node *f);

   static TR::Node *  xconst(uintptr_t value);

   static TR::ILOpCodes xloadOp(TR_RubyFE *fe);

   static TR::Node *createCallNode(TR::ILOpCodes, TR::SymbolReference *, int32_t num, ...);

   static void anchorBefore(TR::Node *node, TR::TreeTop *tt);

   static TR::TreeTop *genTreeTop(TR::Node *node, TR::Block *inBlock);

   static TR::SymbolReference *storeToTemp(TR::Node *value, TR::Block *inBlock);
   static TR::SymbolReference *storeToTemp(TR::Node *value, TR::TreeTop *tt);

   static TR::Node *loadThread(TR::ResolvedMethodSymbol*);

   };

}

#endif

