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

#ifndef RUBYBYTECODEITERATOR_HPP
#define RUBYBYTECODEITERATOR_HPP

#include "ilgen/ByteCodeIterator.hpp"
#include "env/RubyFE.hpp"
#include "env/RubyMethod.hpp"
#include "env/VMHeaders.hpp" 
#include "il/Symbol.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "il/symbol/MethodSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/symbol/RegisterMappedSymbol.hpp"
#include "il/symbol/StaticSymbol.hpp"

typedef VALUE RubyByteCode;

class TR_RubyByteCodeIterator : public TR_ByteCodeIterator<RubyByteCode, ResolvedRubyMethod>
   {
   public:
   typedef TR_ByteCodeIterator<RubyByteCode, ResolvedRubyMethod> Base;

   TR_RubyByteCodeIterator(TR::ResolvedMethodSymbol *methodSymbol, TR::Compilation *comp)
      : Base(methodSymbol,
             static_cast<ResolvedRubyMethod *>(methodSymbol->getResolvedMethod()),
             comp),
        _fe(TR_RubyFE::instance())
      {}

   RubyByteCode first()         { _bcIndex = 0; return current(); }
   RubyByteCode next();
   RubyByteCode current() const { return (_bcIndex >= _maxByteCodeIndex) ? VM_INSTRUCTION_SIZE : at(_bcIndex); }

   protected:

   TR_RubyFE       *fe() const { return _fe; }
   RubyMethodBlock &mb() const { return method()->getRubyMethodBlock(); }

   RubyByteCode  at            (int32_t index)   const;
   int32_t       byteCodeLength(RubyByteCode bc) const;
   const char   *operandTypes  (RubyByteCode bc) const;
   const char   *byteCodeName  (RubyByteCode bc) const;

   VALUE getOperand(int32_t op) const { return at(_bcIndex + op); }

   bool isBranch() const
      { VALUE insn = current(); return
                                   insn == BIN(jump) ||
                                   insn == BIN(branchif) ||
                                   insn == BIN(branchunless) ||
                                   insn == BIN(getinlinecache); }
   int32_t branchDestination(int32_t base) const;

   void printByteCodePrologue();
   void printByteCode();
   void printByteCodeEpilogue();

   void printLocalTables() const;
   void printLocalTable(rb_iseq_t const *iseq, int32_t const level) const;
   void printCatchTable(rb_iseq_t const *iseq, int32_t const level) const;
   void printArgsTable(rb_iseq_t const *iseq) const;

   char const *getLocalName(lindex_t index, const rb_iseq_t *iseq) const;
   char const *getLocalName(lindex_t index, rb_num_t level) const
      { return getLocalName(index, getParentIseq(level)); }

   rb_iseq_t const *getParentIseq(rb_num_t level) const;

   private:
   RubyByteCode const * bytecodes() const { return mb().bytecodes(); }

   TR_RubyFE * const _fe;
   };

const char* catch_type(iseq_catch_table_entry::catch_type t); 
#endif
