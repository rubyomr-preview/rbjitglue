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

#include "codegen/FrontEnd.hpp"
#include "ilgen/IlGeneratorMethodDetails_inlines.hpp"
#include "ruby/env/RubyMethod.hpp"
#include "ruby/ilgen/RubyIlGenerator.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/InlineBlock.hpp"
#include "env/IO.hpp"


Ruby::IlGeneratorMethodDetails::IlGeneratorMethodDetails(TR_ResolvedMethod *method) :
   _method(static_cast<ResolvedRubyMethod *>(method))
   {
   }


TR_IlGenerator *
Ruby::IlGeneratorMethodDetails::getIlGenerator(
      TR::ResolvedMethodSymbol *methodSymbol,
      TR_FrontEnd * fe,
      TR::Compilation *comp,
      TR::SymbolReferenceTable *symRefTab,
      bool forceClassLookahead,
      TR_InlineBlocks *blocksToInline)
   {
   TR_ASSERT(forceClassLookahead == false, "RubyIlGenerator does not support class lookahead");
   TR_ASSERT(blocksToInline == 0, "RubyIlGenerator does not yet support partial inlining");

   return new (comp->trHeapMemory()) RubyIlGenerator(*(self()),
                                                     methodSymbol,
                                                     static_cast<TR_RubyFE *>(fe),
                                                     *symRefTab);
   }


void
Ruby::IlGeneratorMethodDetails::print(TR_FrontEnd *fe, TR::FILE *file)
   {
   if (file == NULL)
      return;

   trfprintf(file, "( %p )", self()->getMethod());
   }


bool
Ruby::IlGeneratorMethodDetails::sameAs(TR::IlGeneratorMethodDetails & other, TR_FrontEnd *fe)
   {
   return self()->getMethod() == other.getMethod();
   }

