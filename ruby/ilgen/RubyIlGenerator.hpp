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

#ifndef RUBYILGENERATOR_HPP
#define RUBYILGENERATOR_HPP

#include "ruby/ilgen/RubyByteCodeIteratorWithState.hpp"
#include "infra/Annotations.hpp"
#include "ilgen/IlGen.hpp"
#include "cs2/llistof.h"
#include "il/Node.hpp"

/* Some OMR headers define min/max macros. To support use of 
 * STL headers here, we undef them here. 
 */
#undef min
#undef max 

#include <map> 
#include <set>
#include <algorithm>

// To be used with the containers. 
#include "env/TypedAllocator.hpp"
#include "env/RawAllocator.hpp"



namespace TR { class IlGeneratorMethodDetails; }
namespace TR { class Block; }

// Set type: FIXME: Rename this to a better type.
typedef std::set<int32_t, std::less<int32_t>, 
        TR::typed_allocator<int32_t, TR::RawAllocator> >
        localset; 

// Map type 
typedef std::map<int32_t, int32_t, std::less<int32_t>,
         TR::typed_allocator<std::pair<int32_t, int32_t>, TR::RawAllocator>
        > intmap; 

//Value Map type 
typedef std::map<VALUE, VALUE, std::less<VALUE>,
         TR::typed_allocator<std::pair<VALUE, VALUE>, TR::RawAllocator>
        > valuemap; 



/**
 * The RubyIlGenerator consumes YARV bytecodes and produces Testarossa Trees.
 *
 * Stack Operations
 * ================
 *
 * In the majority of cases, stack operations are managed by the IlGenerator
 * simulating the YARV operand stack. In otherwords, a YARV 'push' is instead
 * modeled by an IL tree that would produce the pushed value, which is pushed
 * onto the ILGenerator's intermediate stack, and a 'popping' operation simply
 * pulls the value-generating tree out of the IlGenerator's intermdiate stack.
 *
 * One of the most important invariants maintained by the ILGenerator is that
 * the abstract stack height match what the stack height would be at any point.
 * Trees on the abstract stack are called pending, as they have yet to be
 * consumed.
 *
 * By maintaining this invariant, it is possible to always generate a reference
 * to a stack element by offseting from the a privatized stack pointer variable,
 * rather than offsetting from the real stack pointer (which must be accessed
 * indirectrly through the CFP). This makes generating the aliasing for a method
 * substantially easier, as there is a  single symref for each stack slot now.
 *
 * Stack Restorations
 * ------------------
 *
 * The above works fine in most cases. However, until JIT frames can be
 * correctly OSR'd by the interpreter, we must restore the YARV stack before a
 * call, as an exception throw in a callee currently returns to the interpreter, 
 * and so the YARV stack must be prepared, just in case control never returns
 * to a jitted frame.
 *
 * Pending Trees
 * -------------
 * 
 * One requirement Trees has as an intermediate language is that trees may not 
 * be commoned across basic-blocks. A side effect of this is pending trees may
 * not be carried across basic block boundaries. This means that pending trees
 * must be written to variables, then loaded back in the successor blocks. 
 *
 * To make multiple-method entry easier, we choose to write our trees back to
 * the YARV stack at the end of a block \see RubyIlGenerator::saveStack.  
 *
 *
 * Multiple Entry
 * ==============
 *
 * Ruby handles two features, exception handling and optional arguments, by
 * allowing the interpreter to jump to an offset in the bytecode for a method. 
 * To support the same features in the JIT, we must be able to jump to the 
 * compiled code corresponding to an offset in the bytecode. 
 *
 * This is accomplished in a couple of ways. First and foremost, before ILGen 
 * begins, a prepass is used to compute the possible targets for a method. 
 * 
 * \note Version one of this code only supports the offsets for optional 
 *       arguments, however, exceptions can be handled.
 *
 * Given the set of targets for a method, we ensure that these trees will end
 * up in their own blocks by calling genTarget for those indicies. The IL 
 * generator, in cooperation with saveStacks will ensure that any loads of 
 * trees which were 'pending' will come from the YARV stack -- though we 
 * do not see this behviour for optional arguments. 
 *
 * JIT bodies, when called, take two arguments. The first is the thread, and
 * the second is the computed displacement the interpreter would use to jump
 * to the correct bytecode to continue executing. To mimic this behaviour, 
 * a switch statement is generated at the begining of the method, and dispatches
 * to the correct block. 
 *
 * Exception targets sometimes need to be handled specially. 
 * 
 * Invalid Bytecode Sequences
 * ==========================
 *
 * In some cases, Ruby will emit bytecode sequences that are invalid. These
 * sequences are not reachable through execution however, and so are safe from
 * the perspective of the interpreter. One example is the following: 
 *
 *     a = loop do break end
 *
 * The block `do break end` compiles to the following bytecodes:
 *
 *     putnil
 *     throw 
 *     leave
 *
 * The problem is twofold here:
 * 
 * 1. The `leave` after the `throw` has nothing to return.
 * 2. There is an un-followable exception edge to the leave. 
 *
 * In normal execution a `leave` would return the top of the stack. 
 *
 * Now, in normal IlGen, this wouldn't be a problem, as leave wouldn't be
 * generated. However, we need to handle exception edges uniformly. 
 *
 * To avoid this problem we generate a return to the interpreter in cases where
 * we have an exception edge target to a block that was not generated in
 * straight line code. 
 *
 * Other Requirements of IlGen
 * ===========================
 *
 * * The optimizer assumes that the IlGenerator doesn't generate code for
 *   unreachable blocks. Unreachable blocks may otherwise be never cleaned up. 
 *
 */
class RubyIlGenerator : public TR_IlGenerator, public TR_RubyByteCodeIteratorWithState
   {
   public:
   RubyIlGenerator(TR::IlGeneratorMethodDetails &details,
                   TR::ResolvedMethodSymbol *methodSymbol,
                   TR_RubyFE *fe,
                   TR::SymbolReferenceTable &symRefTab);

   TR_ALLOC(TR_Memory::IlGenerator);

   virtual bool genIL();

   virtual int32_t currentByteCodeIndex() { return bcIndex(); }
   virtual TR::Block *getCurrentBlock()    { return _block; }

   private:

   void logAbort(const char*, const char*);
   void logAbort(const char*, const char*, const char*);

   enum CallType
      {
      CallType_send,
      CallType_send_without_block,
      CallType_invokesuper,
      CallType_invokeblock
      };

   TR::SymbolReferenceTable  *symRefTab() { return &_symRefTab; }
   TR::CFG *cfg() { return methodSymbol()->getFlowGraph(); }
   TR_StackMemory        trStackMemory()      { return _trMemory; }
   TR_HeapMemory         trHeapMemory()       { return _trMemory; }

   bool trace()
      {
      return comp()->getOption(TR_TraceBC) || comp()->getOption(TR_TraceILGen);
      }

   bool genILInternal();
   void prependSPPrivatization();
   void walker();

   void indexedWalker(int32_t, int32_t&, int32_t&);
   TR::Block *genExceptionHandlers(TR::Block *prevBlock);

   void                 generateEntrySwitch(); 
   localset             computeEntryTargets(); 
   void                 generateEntryTargets();

   TR::TreeTop*         genEntrySwitchTarget(int32_t,TR::Block*);

   TR::SymbolReference *getStackSymRef(int32_t stackHeight);
   TR::SymbolReference *getLocalSymRef(lindex_t idx, rb_num_t level);
   TR::SymbolReference *getHelperSymRef(TR_RuntimeHelper helper);
   TR::SymbolReference *createShadowSymRef(char *name, TR::DataType dt, size_t size,
                                          int32_t offset, bool killedAcrossCalls);
   TR::SymbolReference *createStaticSymRef(char *name, TR::DataType dt,
                                          void *addr, bool killedAcrossCalls);

   TR::Block *createBug(TR::Block*,const char *, int32_t, ...); 
   TR::Block *createVMExec(TR::Block*); 
   TR::Block *getVMExec(TR::Block*); 
   // if inBlock is not passed, it the tree is generated in the CurrentBlock
   TR::TreeTop *genTreeTop(TR::Node *node, TR::Block* inBlock = NULL);

   TR::Node *loadThread();
   TR::Node *loadDisplacement();

   TR::ILOpCodes xloadOp()
      { return TR::Node::xloadOp(fe()); }

   TR::Node *    xloadi(TR::SymbolReference *symRef, TR::Node *base)
      { return TR::Node::xloadi(symRef, base, fe()); }

   TR::Node *xstorei(TR::SymbolReference *symRef, TR::Node *base, TR::Node *value)
      { return TR::Node::xstorei(symRef, base, value, fe()); }

   // Creates a local array object and populates it num elements from the
   // stack. The top of the stack becomes the last element of the array.
   TR::Node *createLocalArray(int32_t num);

   TR::Node *loadCFP();
   TR::Node *storeCFP(TR::Node *newCFP);
   TR::Node *loadSP();
   TR::Node *loadPC();
   TR::Node *loadCFPFlag();
   TR::Node *loadPrivateSP();
   TR::Node *load_CFP_ISeq();
   TR::Node *storeSP(TR::Node *val);
   TR::Node *storeCFPFlag(TR::Node *val);
   TR::Node *storePC(TR::Node *val); // for exception handling
   TR::Node *loadSelf();
   TR::Node *loadEP(rb_num_t level=0);
   TR::Node *loadPrevEP(TR::Node *ep);
   TR::Node *loadFromRubyStack(TR::Node *ep, int32_t slot);
   TR::Node *loadICSerial(TR::Node *ic)
      { return TR::Node::xloadi(_icSerialSymRef, ic, fe()); }
   TR::Node *loadICValue(TR::Node *ic)
      { return TR::Node::xloadi(_icValueSymRef, ic, fe()); }
   TR::Node *loadICCref(TR::Node *ic)
      { return TR::Node::xloadi(_icCrefSymRef, ic, fe()); }
   TR::Node *getGlobalConstantState()
      { return TR::Node::createLoad(_gcsSymRef); }

   TR::Node *getlocal      (lindex_t idx, rb_num_t level);
   TR::Node *setlocal      (lindex_t idx, rb_num_t level);
   TR::Node *getspecial    (rb_num_t key, rb_num_t value);
   void       setspecial    (rb_num_t key);
   TR::Node *getclassvariable    (ID id);
   void       setclassvariable    (ID id);
   TR::Node *putspecialobject    (rb_num_t value_type);
   TR::Node *getinstancevariable(VALUE id, VALUE ic);
   TR::Node *setinstancevariable(VALUE id, VALUE ic);
   TR::Node *getconstant   (ID id);
   void       setconstant   (ID id);
   TR::Node *getglobal     (GENTRY entry);
   TR::Node *setglobal     (GENTRY entry);
   TR::Node *opt_binary    (TR_RuntimeHelper helper, VALUE ci, VALUE cc);
   TR::Node *opt_ternary   (TR_RuntimeHelper helper, VALUE ci, VALUE cc);
   TR::Node *opt_unary     (TR_RuntimeHelper helper, VALUE ci, VALUE cc);
   TR::Node *opt_neq       (TR_RuntimeHelper helper, VALUE ci, VALUE cc, VALUE ci_eq, VALUE cc_eq);
   TR::Node *newhash       (rb_num_t num);
   TR::Node *newrange      (rb_num_t flag);
   TR::Node *newarray      (rb_num_t num);
   TR::Node *duparray      (VALUE ary);
   TR::Node *concatarray   ();
   void       expandarray   (rb_num_t num, rb_num_t flag);
   TR::Node *splatarray    (rb_num_t flag);
   TR::Node *tostring      ();
   TR::Node *concatstrings (rb_num_t num);
   TR::Node *setinlinecache(IC ic);
   TR::Node *putstring     (VALUE str);
   TR::Node *putiseq       (ISEQ iseq);
   TR::Node *freezestring  (VALUE debugInfo);
   TR::Node *checkmatch    (rb_num_t flag);
   TR::Node *toregexp      (rb_num_t opt, rb_num_t cnt);
   TR::Node *opt_regexpmatch1(VALUE r);
   TR::Node *opt_regexpmatch2(CALL_INFO ci, VALUE cc);
   TR::Node *defined       (rb_num_t op_type, VALUE obj, VALUE needstr);
   TR::Node *aref_with     (CALL_INFO, CALL_CACHE, VALUE);
   TR::Node *aset_with     (CALL_INFO, CALL_CACHE, VALUE);
   void       trace        (rb_num_t nf);

   // Control flow
   int32_t jump(int32_t offset);
   int32_t conditionalJump(bool branchIfTrue, int32_t offset);
   int32_t getinlinecache(OFFSET offset, IC ic);
   int32_t genLeave(TR::Node *retval);
   int32_t genThrow(rb_num_t throw_state, TR::Node *throwobj);
   int32_t genGoto(int32_t target);
   int32_t genOptCaseDispatch(CDHASH hash, OFFSET else_offset); 
   void    genAsyncCheck();
   void    genRubyStackAdjust(int32_t);
   void    rematerializeSP();
   TR::Node *generateCfpPop();


   TR::Node *genCall(TR_RuntimeHelper helper, TR::ILOpCodes opcode, int32_t num, ...);
   TR::Node *genCall_preparation(CALL_INFO ci, uint32_t numArgs, int32_t& restores, int32_t& pending);
   TR::Node *genCall_funcallv(VALUE ci);
   void cleanupStack(int32_t restores, int32_t pending);

   TR::Node *genSend            (CALL_INFO ci, CALL_CACHE cc, ISEQ blockiseq);
   TR::Node *genSendWithoutBlock(CALL_INFO ci, CALL_CACHE cc);
   TR::Node *genInvokeSuper     (CALL_INFO ci, CALL_CACHE cc, ISEQ blockiseq); 
   TR::Node *genInvokeBlock     (CALL_INFO ci); 

   // void     dumpCallInfo(CALL_INFO);

   virtual void saveStack(int32_t );
   bool valueMayBeModified(TR::Node *sideEffect, TR::Node *node);
   void handleSideEffect(TR::Node *sideEffect);
   void handlePendingPushSaveSideEffects(TR::Node *n, vcount_t visitCount);
   void handlePendingPushSaveSideEffects(TR::Node *n);

   void addExceptionTargets(localset&); 

   valuemap* hash_to_map(TR::StackMemoryRegion&, VALUE);

   bool trace_enabled; ///< IlGen Tracing enabled.

   TR::IlGeneratorMethodDetails   &_methodDetails;
   TR_RubyFE                            *_fe;
   TR::SymbolReferenceTable            &_symRefTab;


   TR::SymbolReference                 *_cfpSymRef;
   TR::SymbolReference                 *_epSymRef;
   TR::SymbolReference                 *_selfSymRef;
   TR::SymbolReference                 *_spSymRef;
   TR::SymbolReference                 *_pcSymRef;
   TR::SymbolReference                 *_flagSymRef;
   TR::SymbolReference                 *_gcsSymRef;
   TR::SymbolReference                 *_privateSPSymRef;
   TR::SymbolReference                 *_icSerialSymRef;
   TR::SymbolReference                 *_icValueSymRef;
   TR::SymbolReference                 *_icCrefSymRef;
   //TR::SymbolReference                 *_rb_iseq_struct_selfSymRef;
   TR::SymbolReference                 *_iseqSymRef;
   TR::SymbolReference                 *_mRubyVMFrozenCoreSymRef;

   class Local
      {
      public:
      Local(lindex_t index, rb_num_t level) : _index(index), _level(level) {}

      bool operator==(const Local &other) const { return other._index == _index && other._level == _level; }

      private:
      const lindex_t _index;
      const rb_num_t _level;
      };

   CS2::HashTable<Local,TR::SymbolReference*,TR::Allocator>             _localSymRefs;
   //Map a stack height to a symbol reference.
   CS2::HashTable<int32_t,TR::SymbolReference*,TR::Allocator>           _stackSymRefs;
 
   /**
    * To support exception handling, an entry switch 
    * must fixup the privatized SP before making a side entry 
    * to a handler block. 
    *
    * The offset is computed based on the number of pending trees
    * on entry to a block. 
    */
   intmap _pendingTreesOnEntry; 

   /**
    * Block containing a vm_exec_core call, which can be branched to
    * in order to reinvoke the interpreter. 
    *
    * All execution state must be restored before branching to
    * this block.
    */
   TR::Block* _vm_exec_coreBlock; 

   };

#endif
