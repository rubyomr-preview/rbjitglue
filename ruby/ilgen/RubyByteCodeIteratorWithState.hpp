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

#ifndef RUBYBYTECODEITERATORWITHSTATE_HPP
#define RUBYBYTECODEITERATORWITHSTATE_HPP

#include "ilgen/RubyByteCodeIterator.hpp"
#include "ilgen/ByteCodeIteratorWithState.hpp"

class TR_RubyByteCodeIteratorWithState : 
   public TR_ByteCodeIteratorWithState<RubyByteCode, VM_INSTRUCTION_SIZE, 
                                       TR_RubyByteCodeIterator, TR::Node *>
   {
   public:
   typedef TR_ByteCodeIteratorWithState<RubyByteCode, VM_INSTRUCTION_SIZE, 
                                        TR_RubyByteCodeIterator, TR::Node *> Base;

   TR_RubyByteCodeIteratorWithState(TR::ResolvedMethodSymbol *methodSymbol, TR_RubyFE *fe)
      : Base(methodSymbol, TR::comp())
      { }

   protected:
   virtual void findAndMarkExceptionRanges();
   };

#endif
