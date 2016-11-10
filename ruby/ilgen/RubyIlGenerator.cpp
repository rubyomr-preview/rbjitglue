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

#include "ruby/ilgen/RubyIlGenerator.hpp"

#include <algorithm>
#include <map>
#include <set>
#include "vm_insnhelper.h" // For BOP_MINUS and FIXNUM_REDEFINED_OP_FLAG etc.
#include "vm_core.h"       // For VM_SPECIAL_OBJECT_CBASE and VM_SPECIAL_OBJECT_CONST_BASE, etc.
#include "iseq.h"          // For catch table defs.
#include "env/StackMemoryRegion.hpp"
#include "env/jittypes.h"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "il/symbol/ParameterSymbol.hpp"
#include "ilgen/IlGeneratorMethodDetails_inlines.hpp"
#include "infra/Annotations.hpp"
#include "ruby/config.h"
#include "runtime/Runtime.hpp"
#include "ras/DebugCounter.hpp"

/* 
 * Some OMR headers define min/max macros. To support use of
 * STL headers here, we undef them here.
 */
#undef min
#undef max

#if OPT_CALL_THREADED_CODE
#error "JIT does not support OPT_CALL_THREADED_CODE interpreter"
#endif

RubyIlGenerator::RubyIlGenerator(TR::IlGeneratorMethodDetails &details,
                                 TR::ResolvedMethodSymbol *methodSymbol,
                                 TR_RubyFE *fe,
                                 TR::SymbolReferenceTable &symRefTab)
   : TR_IlGenerator(),
     TR_RubyByteCodeIteratorWithState(methodSymbol, TR_RubyFE::instance()),
     _methodDetails(details),
     _symRefTab(symRefTab),
     _epSymRef(0),
     _cfpSymRef(0),
     _selfSymRef(0),
     _spSymRef(0),
     _flagSymRef(0),
     _pcSymRef(0),
     _gcsSymRef(0),
     _privateSPSymRef(0),
     _icSerialSymRef(0),
     _icValueSymRef(0),
     _rb_iseq_struct_selfSymRef(0),
     _iseqSymRef(0),
     _mRubyVMFrozenCoreSymRef(0),
     _localSymRefs(TR::comp()->allocator()),
     _stackSymRefs(TR::comp()->allocator()),
     trace_enabled(false),
     _pendingTreesOnEntry(std::less<int32_t>(),
                          TR::typed_allocator<std::pair<int32_t,int32_t>,
                                             TR::RawAllocator>(TR::RawAllocator())),
     _vm_exec_coreBlock(0)
   {
   trace_enabled = feGetEnv("TR_TRACE_RUBYILGEN");
   //Create Ruby Helpers.

   //We need to create all the helper symrefs upfront.
   //Most Ruby locals are aliased with helpers so we cannot lazily create helpers
   //during IlGen.
   symRefTab.initializeRubyHelperSymbolRefs(TR_FSRH);
   for (int i = RubyHelper_rb_funcallv; i < TR_FSRH; ++i)
      {
      TR_RuntimeHelper helper = static_cast<TR_RuntimeHelper>(i);
      auto symRef = symRefTab.findOrCreateRubyHelperSymbolRef(helper, true, true, false);
      }

   _epSymRef         = symRefTab.createRubyNamedShadowSymRef("ep",        TR::Address,            TR_RubyFE::SLOTSIZE, offsetof(rb_control_frame_t, ep),   true);
   _cfpSymRef        = symRefTab.createRubyNamedShadowSymRef("cfp",       TR::Address,            TR_RubyFE::SLOTSIZE, offsetof(rb_thread_t, cfp),         false);
   _spSymRef         = symRefTab.createRubyNamedShadowSymRef("sp",        TR::Address,            TR_RubyFE::SLOTSIZE, offsetof(rb_control_frame_t, sp),   true);
   _flagSymRef       = symRefTab.createRubyNamedShadowSymRef("flag",      TR::Address,            TR_RubyFE::SLOTSIZE, offsetof(rb_control_frame_t, flag), true);


   // PC is being rematerialized before calls that may read/modify its value, so kill it across helper calls.
   _pcSymRef         = symRefTab.createRubyNamedShadowSymRef("pc",        TR::Address,             TR_RubyFE::SLOTSIZE, offsetof(rb_control_frame_t, pc),   true);
   _selfSymRef       = symRefTab.createRubyNamedShadowSymRef("self",      TR_RubyFE::slotType(),    TR_RubyFE::SLOTSIZE, offsetof(rb_control_frame_t, self), false);
   _icSerialSymRef   = symRefTab.createRubyNamedShadowSymRef("ic_serial", TR_RubyFE::slotType(),    TR_RubyFE::SLOTSIZE, offsetof(struct iseq_inline_cache_entry, ic_serial),        false);
   _icValueSymRef    = symRefTab.createRubyNamedShadowSymRef("ic_value",  TR_RubyFE::slotType(),    TR_RubyFE::SLOTSIZE, offsetof(struct iseq_inline_cache_entry, ic_value.value),   false);

   _rb_iseq_struct_selfSymRef =
                       symRefTab.createRubyNamedShadowSymRef("rb_iseq_struct->self",      TR_RubyFE::slotType(),  TR_RubyFE::SLOTSIZE, offsetof(rb_iseq_struct, self),     false);  //self in rb_iseq_struct does not change across calls.
   _iseqSymRef       = symRefTab.createRubyNamedShadowSymRef("iseq",                      TR::Address,           TR_RubyFE::SLOTSIZE, offsetof(rb_control_frame_t, iseq), true);

   static_assert(sizeof(*fe->getJitInterface()->globals.ruby_vm_global_constant_state_ptr) == TR_RubyFE::SLOTSIZE,
                 "vm_global_constant_state wrong size");
   _gcsSymRef        =  symRefTab.createRubyNamedStaticSymRef("ruby_vm_global_constant_state",     TR_RubyFE::slotType(),  fe->getJitInterface()->globals.ruby_vm_global_constant_state_ptr, 0, true);

   static_assert(sizeof(*fe->getJitInterface()->globals.ruby_rb_mRubyVMFrozenCore_ptr) == TR_RubyFE::SLOTSIZE,
                 "frozen_core wrong size");
   _mRubyVMFrozenCoreSymRef =
                        symRefTab.createRubyNamedStaticSymRef("ruby_mRubyVMFrozenCore",            TR_RubyFE::slotType(),  fe->getJitInterface()->globals.ruby_rb_mRubyVMFrozenCore_ptr,     0, true);

   _privateSPSymRef = symRefTab.createTemporary(methodSymbol, TR::Address);
   _privateSPSymRef->setEmptyUseDefAliases(&symRefTab);
   _privateSPSymRef->setAliasedTo(_spSymRef, true /*symmetric*/);
   }

/**
 * Put output to the trace log, verbose log, and increment any associated
 * static counters, then abort the compilation.
 *
 * Static counter is `compilation_abort/counterReason`
 */
void
RubyIlGenerator::logAbort(const char * logMessage,
                          const char * counterReason)
   {
   TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "compilation_abort/%s",
                                                          counterReason));
   fe()->outOfMemory(comp(), logMessage);
   TR_ASSERT(0, "Compilation abort failed! %s", logMessage);
   }

/**
 * Put output to the trace log, verbose log, and increment any associated
 * static counters, then abort the compilation.
 *
 * Static counter is `compilation_abort/counterReason/counterSubreason`
 */
void
RubyIlGenerator::logAbort(const char * logMessage,
                          const char * counterReason,
                          const char * counterSubreason)
   {
   TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "compilation_abort/%s/%s",
                                                          counterReason,
                                                          counterSubreason));
   fe()->outOfMemory(comp(), logMessage);
   TR_ASSERT(0, "Compilation abort failed! %s", logMessage);
   }



bool
RubyIlGenerator::genIL()
   {
   if (comp()->isOutermostMethod())
      comp()->reportILGeneratorPhase();

   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   comp()->setCurrentIlGenerator(this);

   _stack = new (trStackMemory()) TR_Stack<TR::Node *>(trMemory(), 20, false, stackAlloc);

   bool success = genILInternal();

   if (success)
      prependSPPrivatization();

   comp()->setCurrentIlGenerator(0);

   return success;
   }

/**
 * Returns the vm_exec block.
 *
 * This must be connected to the CFG, however,
 * the block will already have been added to the CFG.
 */
TR::Block *
RubyIlGenerator::getVMExec(TR::Block * insertBlock)
   {
   if (_vm_exec_coreBlock)
     return _vm_exec_coreBlock;

   auto * nextBlock = insertBlock->getNextBlock();

   _vm_exec_coreBlock = TR::Block::createEmptyBlock(comp());

   createVMExec(_vm_exec_coreBlock);

   insertBlock->getExit()->join(_vm_exec_coreBlock->getEntry());
   _vm_exec_coreBlock->getExit()->join(nextBlock->getEntry());

   cfg()->addNode(_vm_exec_coreBlock);
   cfg()->addSuccessorEdges(_vm_exec_coreBlock);

   return _vm_exec_coreBlock;
   }

/**
 * Generate a call to vm_exec_core, and return of its return.
 *
 * Unsets the VM_FRAME_FLAG_JITTED flag on the CFP before returning to the
 * interpreter, for RAS purposes.
 *
 * @returns the passed in, modified block.
 *
 * \TODO: Create a variant that can restore VM state for a fuller
 *        Voluntary-OSR story.
 */
TR::Block *
RubyIlGenerator::createVMExec(TR::Block * block)
   {
   auto flag_xor = storeCFPFlag(TR::Node::xxor(
                                   TR::Node::xconst(VM_FRAME_FLAG_JITTED),
                                   loadCFPFlag()));
   genTreeTop(flag_xor,block);

   // Call vm_exec_core and return returned value.
   auto * symRef = getHelperSymRef(RubyHelper_vm_exec_core);
   auto * cnode = TR::Node::create(TR::acall, 2, loadThread(), TR::Node::aconst(0));
   cnode->setSymbolReference(symRef);
   genTreeTop(cnode,block);
   genTreeTop(TR::Node::create(TR::areturn, 1, cnode), block);

   return block;
   }



/**
 * Generate a new block at the end of the program that contains
 * a call to rb_bug, which has as its argument an abort message,
 * which can have formatting args.
 *
 * This block must be inserted into the cfg.
 */
TR::Block *
RubyIlGenerator::createBug(TR::Block * block, const char * abort_message, int32_t formats, ...)
   {
   va_list args;
   va_start(args, formats);
   auto * symRef = getHelperSymRef(RubyHelper_rb_bug);
   TR_ASSERT(symRef, "Must have symRef");
   auto * cnode = TR::Node::create(TR::call, formats + 1);
   cnode->setSymbolReference(symRef);

   cnode->setAndIncChild(0,
       TR::Node::aconst(reinterpret_cast<uintptrj_t>(abort_message)));

   for (int i = 0; i < formats ; i++)
      {
      TR::Node* child = va_arg(args, TR::Node *);
      TR_ASSERT(child, "Null child passed in");
      cnode->setAndIncChild(i + 1, child);
      }

   traceMsg(comp(), "Generating rb_bug call with message %s and %d args\n",
            abort_message,
            formats);

   auto * callTT = genTreeTop(cnode, block);

   // Generate return to satisfy CFG requirements.
   // Feeding 0xdeadbeef as a poison value.
   genTreeTop(TR::Node::create(TR::areturn, 1,
                             TR::Node::aconst((uintptrj_t)0xdeadbeef)),
              block);

   return block;
   }

/**
 * Generate all targets of the entry switch
 *
 * This perhaps ought to be done as part of
 * `TR_ByteCodeIteratorWithState::markAnySpecialBranchTargets`
 *
 */
void
RubyIlGenerator::generateEntryTargets()
   {
   const localset& targets = computeEntryTargets();
   for (auto iter = targets.begin(); iter != targets.end(); ++iter)
      {
      if (trace_enabled)
         traceMsg(comp(), "generating target %d\n", *iter);

      genTarget(*iter, false);
      }
   }


/**
 * Compute targets
 */
localset
RubyIlGenerator::computeEntryTargets()
   {
   TR::RawAllocator rawAllocator;
   TR::typed_allocator<int32_t, TR::RawAllocator> ta(rawAllocator);
   localset targets(std::less<int32_t>(), ta);

   //Default target is target 0.
   targets.insert(0);


   //Analyze opt args
   const auto *iseq = mb().iseq();

   traceMsg(comp(), "arg_opts: %d\n", iseq->param.opt_num);
   for (int i = 0; i < iseq->param.opt_num; i++)
      {
      targets.insert(iseq->param.opt_table[i]);
      }

   // Analyze catch tables.
   static auto doExceptions = !feGetEnv("TR_RUBY_DISABLE_EXCEPTION_TARGETS");
   if (doExceptions)
      {
      addExceptionTargets(targets);
      }

   return targets;
   }



/**
 * To support multiple entries, each ruby method has as its first real block a
 * switch statement. The switch directs control flow by computing the offset
 * the VM expects to start executing.
 *
 * Switch cases are generated for all optional arguments and exception entries.
 *
 * The original design of the Entry switch assumed that all possible entries
 * into a particular body would be declared as part of the catch table.
 * Therefore the default entry of the switch would be a call to rb_bug, as we
 * didn't understand a particular offset.
 *
 * One case we missed is return-as-throw.
 *
 * This occurs when there's a return inside a block. For example:
 *
 *     def foo
 *          bar
 *          baz
 *     end
 *
 *     def bar
 *          Thing.each do |y|
 *                  if |y| == "exit"
 *                          return
 *                  end
 *          end
 *          puts "bar"
 *     end
 *
 * In this case, the return statement is actually inside a block. This gets
 * compiled to a `throw 1`. The exception is actually targeting the program
 * point just before the baz. However, there is no entry in the catch table for
 * this.
 *
 * One option would be to assume that any method called can throw-return. This
 * would have us break the blocks after ever call, and add an entry to the
 * switch for every bytecode following a throw.
 *
 * For now, in the absence of data showing this is a severe issue, we've chosen
 * instead to create a call to vm_exec_core, performing what is effectively an
 * OSR back to the interpreter.
 */
void
RubyIlGenerator::generateEntrySwitch()
   {

   auto enableBugBlock = feGetEnv("TR_ENABLE_BUG_BLOCK");

   traceMsg(comp(), "generating entry switch\n");

   auto* block          = TR::Block::createEmptyBlock(comp());

   auto* oldFirst       = _methodSymbol->getFirstTreeTop();
   auto* oldBlock       = oldFirst->getEnclosingBlock();

   auto* bugBlock       = enableBugBlock
                           ?
                              createBug(TR::Block::createEmptyBlock(comp()),
                                        "Invalid switch case %p passed to "
                                        " jit body for switch dispatch\n",
                                        1,
                                        loadDisplacement())
                           : createVMExec(TR::Block::createEmptyBlock(comp()));
   /*
    * We want to track how often we take the default branch exit, so, we add a debug
    * counter.
    */

   auto firstTreeOfBugBlock = bugBlock->getEntry()->getNextTreeTop();
   TR::DebugCounter::prependDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "(%s)/EntrySwitch/vm_exec_core-defaultBranch",comp()->signature()), firstTreeOfBugBlock);

   auto* selector       = loadDisplacement();

   bugBlock->getExit()->join(oldFirst);

   _methodSymbol->setFirstTreeTop(block->getEntry());
   block->getExit()->join(bugBlock->getEntry());

   cfg()->addNode(block);
   cfg()->addNode(bugBlock);

   cfg()->addEdge(bugBlock, cfg()->getEnd());
   oldBlock->movePredecessors(block);

   const localset& targets = computeEntryTargets();


   TR::Node  * defaultCase      = TR::Node::createCase(0, bugBlock->getEntry() );

   // We may be able to produce a better performing entry switch by
   // using a tableswitch (TR::table). However, we'd have to modify
   // the below code to handle the dense table where many targets
   // will be rb_bug.
   TR::Node  * switchNode       = TR::Node::create(TR::lookup, 2 +
                                                   targets.size(), selector,
                                                   defaultCase);

   int32_t child = 2;  //First two children are selector and default case.
   for (auto iter = targets.begin(); iter != targets.end(); ++iter, ++child)
      {
      traceMsg(comp(), "case (%d * %d = %d) -> %d\n", TR_RubyFE::SLOTSIZE, *iter, TR_RubyFE::SLOTSIZE * *iter,  *iter);
      auto * target = genSwitchTarget(*iter, block);
      switchNode->setAndIncChild(child,
                                 TR::Node::createCase(0, target, TR_RubyFE::SLOTSIZE * *iter));
      }

   auto * tt = genTreeTop(switchNode, block);
   TR::DebugCounter::prependDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "(%s)/invocations",comp()->signature()), tt);
   // Update CFG for switch.
   cfg()->addSuccessorEdges(block);
   return;
   }

/**
 * Return the correct block for the branch from the entry switch to block
 * `index`.
 *
 * We generate a new block to hold a debug counter increment in order to
 * diagnose this code. As well, In cases where there are pending trees carried
 * across the normal flow edge to block `index`, we use this block to adjust
 * the privatized stack pointer temp.
 */
TR::TreeTop*
RubyIlGenerator::genSwitchTarget(int32_t index, TR::Block* insertBlock)
   {
   const char*  debugCounterName = NULL;
   TR::TreeTop* goto_destination = NULL;

   // Disallow non-zero entries.
   static auto  disableMultipleOption = feGetEnv("TR_DISABLE_MULTIPLE_ENTRY");

   // Disallow entries which require a stack adjust.
   static auto  disableStackAdjust    = feGetEnv("TR_DISABLE_STACK_ADJUST_ENTRY");

   auto disableMultiple = disableMultipleOption && index > 0;

   //Generate new block, and link in.
   auto * block = TR::Block::createEmptyBlock(comp());


   if (_pendingTreesOnEntry[index] == 0
       && isGenerated(index)
       && !disableMultiple )
      {
      TR_ASSERT(at(index) != BIN(pop), "Targeted pop with 0 pending trees on entry... seems nonsensical!");
      traceMsg(comp(), "Generating normal target %d (has zero pending on entry)\n",index);
      goto_destination = genTarget(index);
      debugCounterName = "target";
      }
   else if (_pendingTreesOnEntry[index] > 0
            && isGenerated(index)
            && !disableMultiple
            && !disableStackAdjust)
      {
      // In this case there are N pending trees on entry to the method.
      //
      // A reminder: `saveStack` takes pending trees at the exit of one block,
      // stores them back to the YARV stack, and then loads them at the
      // beginning of the next block. So the pending trees for block[index]
      // were inserted as pending loads from the YARV stack when block[index]
      // was generated.
      //
      // However, the pending loads that were inserted were relative to
      // privateSP, based on a normal entry and flow of control. However, since
      // we are generating a switch target, this is not the normal flow of
      // control. privateSP points to the current stack pointer value, which is
      // not the zero-entry height.
      //
      // To correctly load the right values out of the YARV stack in the
      // future, we must adjust privateSP to point to where it *would have
      // pointed*, if we had taken a normal entry path. That height is
      // `privateSP - pendingTreesOnEntry.`

      traceMsg(comp(), "Generating stack adjust target %d (has %d pending trees(s)"
                       ", but target is generated, so stack height is understood.)\n",
                       index,
                       _pendingTreesOnEntry[index]);

      goto_destination = genTarget(index);

      auto *increment = TR::Node::createStore(_privateSPSymRef,
                                              TR::Node::create(TR::l2a, 1,
                                                               TR::Node::xsub(TR::Node::create(TR::a2l, 1, loadPrivateSP()),
                                                                              TR::Node::xconst(_pendingTreesOnEntry[index] *  TR_RubyFE::SLOTSIZE))
                                                               )
                                              );

      genTreeTop(increment, block);

      char stackAdjustName[1024];
      memset(stackAdjustName,0,1024);
      snprintf(stackAdjustName, 1024, "stackadjust-%ds", _pendingTreesOnEntry[index]);
      debugCounterName = stackAdjustName;
      }
   else
      {
      if (disableMultiple)
         traceMsg(comp(), "Generating return to VMExec for target %d,"
                  " because we disabled regular multiple entries.\n",
                  index);
      else if (!isGenerated(index))
         traceMsg(comp(), "Generating return to VMExec for target %d,"
                  " because it wasn't mainline generated\n",
                  index);
      else
         traceMsg(comp(), "Generating return to VMExec for target %d,"
                  " because other options were disabled.\n",
                  index);

      goto_destination = getVMExec(insertBlock)->getEntry();
      debugCounterName = "vm_exec_core";
      }

   //Link in block.
   auto * nextBlock = insertBlock->getNextBlock();
   insertBlock->getExit()->join(block->getEntry());
   block->getExit()->join(nextBlock->getEntry());

   auto *gotoNode = TR::Node::create(NULL, TR::Goto);
   gotoNode->setBranchDestination(goto_destination);
   genTreeTop(gotoNode, block);

   TR::DebugCounter::prependDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "(%s)/EntrySwitch/%s-%d",
                                                        comp()->signature(),
                                                        debugCounterName,
                                                        index),
                               block->getEntry()->getNextTreeTop() );
   cfg()->addNode(block);
   cfg()->addSuccessorEdges(block);

   return block->getEntry();
   }

/**
 * Add targets for exception entries
 */
void
RubyIlGenerator::addExceptionTargets(localset &targets)
   {
   const rb_iseq_t *iseq = mb().iseq();
   for (int i = 0;
        iseq->catch_table && i < iseq->catch_table->size;
        i++)
      {
      struct iseq_catch_table_entry &entry = iseq->catch_table->entries[i];
      if (!entry.iseq) // Catch entry is local
         {

         if (entry.sp != 0 ) // Not sure how we have to handle a non-zero sp in a catch table entry.
            {
            TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "compilation_abort/non_zero_sp_catch_table_entry"));
            fe()->outOfMemory(comp(), "Aborting compilation based on non-zero sp in local catch table entry\n" );
            }

         //FIXME: This might be excessive -- we probably can filter
         //       these on the entry type.
         switch (entry.type)
            {
            case iseq_catch_table_entry::CATCH_TYPE_RESCUE:
            case iseq_catch_table_entry::CATCH_TYPE_ENSURE:
            case iseq_catch_table_entry::CATCH_TYPE_RETRY:
            case iseq_catch_table_entry::CATCH_TYPE_BREAK:
            case iseq_catch_table_entry::CATCH_TYPE_REDO:
            case iseq_catch_table_entry::CATCH_TYPE_NEXT:
               traceMsg(comp(), "Exception cont: %s -> %d\n",
                        catch_type(entry.type), entry.cont);
               targets.insert(entry.cont);
            }
         }
      }
   }

/*
 * Modify the first block of the method to be saving SP
 */
void
RubyIlGenerator::prependSPPrivatization()
   {
   TR::Block* block = methodSymbol()->prependEmptyFirstBlock();
   TR::Node::genTreeTop(TR::Node::createStore(_privateSPSymRef, loadSP()),
                        block);
   return;
   }

bool
RubyIlGenerator::genILInternal()
   {
   TR_RubyByteCodeIteratorWithState::initialize();

   // This check is sufficient only while the VM is restricting dispatch
   // to the 0 offset case. When the VM is modified, we will have to
   // always support entry switches.
   static auto enableEntrySwitch = !feGetEnv("TR_DISABLE_ENTRY_SWITCH");
   traceMsg(comp(), "Entry switch %s\n",
            enableEntrySwitch ? "enabled"
            : "disabled. This requires the VM check incoming offset be zero.");

   if (enableEntrySwitch)
      generateEntryTargets();

   TR::Block *lastBlock = walker(NULL);

   methodSymbol()->setFirstTreeTop(blocks(0)->getEntry());

   if (trace_enabled)
      comp()->dumpMethodTrees("Before entry switch");


   // The entry switch *must not* introduce new pending push saves.
   // We accomplish this by dumping the _stack here.
   //
   // Philosophizing:
   //
   // Pending trees at this point are an interesting question: These
   // would be trees that were left pending from the last generated block in
   // the walker. I don't believe there are any legal ways that computations
   // here should be visible from the ruby side.
   //
   // Yet despite this, I'm mildly concerned by the possibility.  For
   // example, what if a side-effect producing call ends up on the stack and
   // is discarded here.... Should we be restoring the stack at return
   // points too?
   _stack->clear();

   if (enableEntrySwitch)
      generateEntrySwitch();

   return true;
   }

TR::Block *
RubyIlGenerator::walker(TR::Block *prevBlock)
   {
   int32_t lastIndex = 0, firstIndex = 0;
   TR_ASSERT(_bcIndex == 0, "first bcIndex isn't zero");

   indexedWalker(0, firstIndex, lastIndex);

   // join the basic blocks
   TR::Block * lastBlock, * nextBlock, * block = blocks(firstIndex);
   if (firstIndex == 0)
      cfg()->addEdge(cfg()->getStart(), block);
   else
      prevBlock->getExit()->join(block->getEntry());

   for (int32_t i = firstIndex; block; lastBlock = block, block = nextBlock)
      {
      while (block->getNextBlock())
         {
         TR_ASSERT( block->isAdded(), "Block should have already been added\n" );
         block = block->getNextBlock();
         }

      block->setIsAdded();

      for (nextBlock = 0; !nextBlock && ++i <= lastIndex; )
         if (isGenerated(i) && blocks(i) && !blocks(i)->isAdded())
            nextBlock = blocks(i);

      // If an exception range ends with an if and the fall through is
      // in the main-line code then we have to generate a fall through block
      // which contains a goto to jump back to the main-line code.
      //
      TR::Node * lastRealNode = block->getLastRealTreeTop()->getNode();
      if (!nextBlock && lastRealNode->getOpCode().isIf())
         {
         nextBlock = TR::Block::createEmptyBlock(comp());
         i = lastIndex;
         TR_ASSERT(blocks(i + 3), "can't find the fall thru block");
         nextBlock->append(TR::TreeTop::create(comp(),
                                               TR::Node::create(lastRealNode,
                                                                TR::Goto,
                                                                0,
                                                                blocks(i + 3)->getEntry())));
         }

      block->getExit()->getNode()->copyByteCodeInfo(lastRealNode);
      cfg()->insertBefore(block, nextBlock);
      }

   return lastBlock;

   }

/**
 * Restartable walker.
 * Generates all bytecode reachable from start_index.
 *
 * Sets firstIndex and lastIndex to lowest and highest indexes seen,
 * respectively.
 */
void
RubyIlGenerator::indexedWalker(int32_t startIndex, int32_t& firstIndex, int32_t& lastIndex)
   {
   traceMsg(comp(), "Starting indexedWalker at start_index %d\n", startIndex);
   _bcIndex = startIndex;
   while (_bcIndex < _maxByteCodeIndex)
      {
      if (blocks(_bcIndex) && blocks(_bcIndex) != _block)
         {
         if (isGenerated(_bcIndex))
            _bcIndex = genGoto(_bcIndex);
         else
            _bcIndex = genBBEndAndBBStart();
         if (_bcIndex >= _maxByteCodeIndex)
            break;
         }
      if (_bcIndex < firstIndex)
         firstIndex = _bcIndex;
      else if (_bcIndex > lastIndex)
         lastIndex = _bcIndex;

      TR_ASSERT(!isGenerated(_bcIndex), "Walker error");
      setIsGenerated(_bcIndex);

      VALUE   insn = current();
      int32_t len  = byteCodeLength(insn);


      if (trace_enabled)
         traceMsg(comp(), "RubyIlGenerator:: Generating block %d into %p\n", _bcIndex, _block);

      switch (insn)
         {
         case BIN(nop):                         /*nothing */ _bcIndex += len; break;

         case BIN(trace):                       if (!feGetEnv("OMR_DISABLE_TRACE_INSTRUCTIONS"))
                                                   trace((rb_num_t)getOperand(1));
                                                _bcIndex += len; break;

         case BIN(getlocal):                    push(getlocal(getOperand(1)/*idx*/, getOperand(2)/*level*/)); _bcIndex += len; break;
         case BIN(getlocal_OP__WC__0):          push(getlocal(getOperand(1)/*idx*/, 0/*level*/));             _bcIndex += len; break;
         case BIN(getlocal_OP__WC__1):          push(getlocal(getOperand(1)/*idx*/, 1/*level*/));             _bcIndex += len; break;

         case BIN(setlocal):                    setlocal(getOperand(1)/*idx*/, getOperand(2)/*level*/); _bcIndex += len; break;
         case BIN(setlocal_OP__WC__0):          setlocal(getOperand(1)/*idx*/,             0/*level*/); _bcIndex += len; break;
         case BIN(setlocal_OP__WC__1):          setlocal(getOperand(1)/*idx*/,             1/*level*/); _bcIndex += len; break;

         case BIN(getinstancevariable):         push(getinstancevariable(getOperand(1), getOperand(2)));  _bcIndex += len; break;
         case BIN(setinstancevariable):         setinstancevariable     (getOperand(1), getOperand(2)); _bcIndex += len; break;

         case BIN(getconstant):                 push(getconstant((ID)getOperand(1)));    _bcIndex += len; break;
         case BIN(setconstant):                 setconstant     ((ID)getOperand(1));     _bcIndex += len; break;
         case BIN(getglobal):                   push(getglobal((GENTRY)getOperand(1)));  _bcIndex += len; break;
         case BIN(setglobal):                   setglobal     ((GENTRY)getOperand(1));   _bcIndex += len; break;

         case BIN(putnil):                      push(TR::Node::xconst(Qnil));          _bcIndex += len; break;
         case BIN(putobject):                   push(TR::Node::xconst(getOperand(1))); _bcIndex += len; break;
         case BIN(putobject_OP_INT2FIX_O_0_C_): push(TR::Node::xconst(INT2FIX(0)));    _bcIndex += len; break;
         case BIN(putobject_OP_INT2FIX_O_1_C_): push(TR::Node::xconst(INT2FIX(1)));    _bcIndex += len; break;
         case BIN(answer):                      push(TR::Node::xconst(INT2FIX(42)));   _bcIndex += len; break;
         case BIN(putself):                     push(loadSelf());                       _bcIndex += len; break;

         case BIN(putstring):                   push(putstring(getOperand(1)));         _bcIndex += len; break;

         case BIN(pop):                         pop();               _bcIndex += len; break;
         case BIN(dup):                         push(top());         _bcIndex += len; break;
         case BIN(dupn):                        dupn(getOperand(1)); _bcIndex += len; break;
         case BIN(swap):                        swap();              _bcIndex += len; break;
         case BIN(reput):                       /*nothing */         _bcIndex += len; break;
         case BIN(topn):                        push(topn(getOperand(1))); _bcIndex += len; break;
         case BIN(setn):                        setn(getOperand(1), top()); _bcIndex += len; break;
         case BIN(adjuststack):                 popn(getOperand(1)); _bcIndex += len; break;

         case BIN(newarray):                    push(newarray((rb_num_t)getOperand(1))); _bcIndex += len; break;
         case BIN(duparray):                    push(duparray(getOperand(1))); _bcIndex += len; break;
         case BIN(concatarray):                 push(concatarray());  _bcIndex += len; break;
         case BIN(expandarray):                 expandarray((rb_num_t)getOperand(1), (rb_num_t)getOperand(2)); _bcIndex += len; break;
         case BIN(splatarray):                  push(splatarray((rb_num_t)getOperand(1)));  _bcIndex += len; break;
         case BIN(tostring):                    push(tostring());                       _bcIndex += len; break;
         case BIN(concatstrings):               push(concatstrings((rb_num_t)getOperand(1)));   _bcIndex += len; break;

         case BIN(getspecial):                  push(getspecial((rb_num_t)getOperand(1), (rb_num_t)getOperand(2)));  _bcIndex += len; break;
         case BIN(setspecial):                  setspecial((rb_num_t)getOperand(1));                                 _bcIndex += len; break;
         case BIN(getclassvariable):            push(getclassvariable((ID)getOperand(1)));                           _bcIndex += len; break;
         case BIN(setclassvariable):            setclassvariable((ID)getOperand(1));                                 _bcIndex += len; break;
         case BIN(putspecialobject):            push(putspecialobject((rb_num_t)getOperand(1)));                     _bcIndex += len; break;

         case BIN(checkmatch):                  push(checkmatch((rb_num_t)getOperand(1)));                           _bcIndex += len; break;

         case BIN(putiseq):                     push(putiseq((ISEQ)getOperand(1)));                                  _bcIndex += len; break;
         case BIN(newhash):                     push(newhash((rb_num_t)getOperand(1))); _bcIndex += len; break;

         case BIN(newrange):                    push(newrange((rb_num_t)getOperand(1))); _bcIndex += len; break;

         case BIN(toregexp):                    push(toregexp((rb_num_t)getOperand(1), (rb_num_t)getOperand(2))); _bcIndex += len; break;

         case BIN(leave):                       _bcIndex = genReturn(pop()); break;
         case BIN(throw):                       _bcIndex = genThrow((rb_num_t)getOperand(1), pop()); break;
         case BIN(jump):                        _bcIndex = jump(getOperand(1)); break;
         case BIN(branchif):                    _bcIndex = conditionalJump(true /* branchIfTrue*/, getOperand(1)); break;
         case BIN(branchunless):                _bcIndex = conditionalJump(false/*!branchIfTrue*/, getOperand(1)); break;
         case BIN(getinlinecache):              _bcIndex = getinlinecache((OFFSET)getOperand(1), (IC) getOperand(2)); break;

         case BIN(setinlinecache):              push(setinlinecache((IC)getOperand(1))); _bcIndex += len; break;

         case BIN(opt_regexpmatch1):            push(opt_regexpmatch1(getOperand(1))); _bcIndex += len; break;
         case BIN(opt_regexpmatch2):            push(opt_regexpmatch2((CALL_INFO)getOperand(1))); _bcIndex += len; break;

         case BIN(defined):                     push(defined((rb_num_t)getOperand(1), (VALUE)getOperand(2), (VALUE)getOperand(3))); _bcIndex += len; break;

         case BIN(opt_plus):                    push(opt_binary (RubyHelper_vm_opt_plus,    getOperand(1))); _bcIndex += len; break;
         case BIN(opt_minus):                   push(opt_binary (RubyHelper_vm_opt_minus,   getOperand(1))); _bcIndex += len; break;
         case BIN(opt_mult):                    push(opt_binary (RubyHelper_vm_opt_mult,    getOperand(1))); _bcIndex += len; break;
         case BIN(opt_div):                     push(opt_binary (RubyHelper_vm_opt_div,     getOperand(1))); _bcIndex += len; break;
         case BIN(opt_mod):                     push(opt_binary (RubyHelper_vm_opt_mod,     getOperand(1))); _bcIndex += len; break;
         case BIN(opt_eq):                      push(opt_binary (RubyHelper_vm_opt_eq,      getOperand(1))); _bcIndex += len; break;
         case BIN(opt_neq):                     push(opt_neq    (RubyHelper_vm_opt_neq,     getOperand(1), getOperand(2))); _bcIndex += len; break;
         case BIN(opt_lt):                      push(opt_binary (RubyHelper_vm_opt_lt,      getOperand(1))); _bcIndex += len; break;
         case BIN(opt_le):                      push(opt_binary (RubyHelper_vm_opt_le,      getOperand(1))); _bcIndex += len; break;
         case BIN(opt_gt):                      push(opt_binary (RubyHelper_vm_opt_gt,      getOperand(1))); _bcIndex += len; break;
         case BIN(opt_ge):                      push(opt_binary (RubyHelper_vm_opt_ge,      getOperand(1))); _bcIndex += len; break;
         case BIN(opt_ltlt):                    push(opt_binary (RubyHelper_vm_opt_ltlt,    getOperand(1))); _bcIndex += len; break;
         case BIN(opt_not):                     push(opt_unary  (RubyHelper_vm_opt_not,     getOperand(1))); _bcIndex += len; break;
         case BIN(opt_aref):                    push(opt_binary (RubyHelper_vm_opt_aref,    getOperand(1))); _bcIndex += len; break;
         case BIN(opt_aset):                    push(opt_ternary(RubyHelper_vm_opt_aset,    getOperand(1))); _bcIndex += len; break;
         case BIN(opt_length):                  push(opt_unary  (RubyHelper_vm_opt_length,  getOperand(1))); _bcIndex += len; break;
         case BIN(opt_size):                    push(opt_unary  (RubyHelper_vm_opt_size,    getOperand(1))); _bcIndex += len; break;
         case BIN(opt_empty_p):                 push(opt_unary  (RubyHelper_vm_opt_empty_p, getOperand(1))); _bcIndex += len; break;
         case BIN(opt_succ):                    push(opt_unary  (RubyHelper_vm_opt_succ,    getOperand(1))); _bcIndex += len; break;
         case BIN(opt_aset_with):               push(aset_with((CALL_INFO)getOperand(1), (VALUE)getOperand(2)));
                                                _bcIndex += len;
                                                break;
         case BIN(opt_aref_with):               push(aref_with((CALL_INFO)getOperand(1), (VALUE)getOperand(2)));
                                                _bcIndex += len;
                                                break;
         case BIN(opt_send_without_block):
            push(genCall_ruby_stack(getOperand(1), CallType_send_without_block)); _bcIndex += len; break;
         case BIN(send):
            push(genCall_ruby_stack(getOperand(1), CallType_send)); _bcIndex += len; break;
         case BIN(invokesuper):
            push(genCall_ruby_stack(getOperand(1), CallType_invokesuper)); _bcIndex += len; break;
         case BIN(invokeblock):
            push(genCall_ruby_stack(getOperand(1), CallType_invokeblock)); _bcIndex += len; break;

         // TODO: BIN(defineclass):
         // TODO: BIN(opt_str_freeze):
         // TODO: BIN(once):
         // TODO: BIN(opt_case_dispatch):
         // TODO: BIN(opt_call_c_function):
         // TODO: BIN(bitblt):
         // TODO: BIN(opt_send_without_block):

         default:
            traceMsg(comp(), "ABORTING COMPILE: Unsupported YARV instruction %d %s\n", (int)insn, byteCodeName(insn));

            char abort[200];
            memset(abort, 0, sizeof(abort));
            sprintf(abort, "unsupported YARV instruction %s (%d)", byteCodeName(insn), (int)insn);
            logAbort(abort, "unsupported_instruction", byteCodeName(insn));

            // UNREACHABLE
            _bcIndex += len; break;
         }

      auto * byteCodeLimitStr   = feGetEnv("OMR_RUBY_BYTECODE_LIMIT");
      auto   byteCodeMaximum    = 10000;  // Default maximum is 10,000 byte codes.

      if (byteCodeLimitStr)
         {
         byteCodeMaximum = atoi(byteCodeLimitStr);
         traceMsg(comp(), "Complexity limit set to %d\n", byteCodeMaximum);
         }

      if(_bcIndex > byteCodeMaximum)
         {
         logAbort("Excessive complexity in ILGen (_bcIndex > 10000 (or OMR_RUBY_BYTECODE_LIMIT) )",
                  "excessive_complexity");
         // Unreachable
         }
      }

   }

/**
 * Generate a treetop in the current block.
 */
TR::TreeTop *
RubyIlGenerator::genTreeTop(TR::Node *n, TR::Block* block)
   {
   if (!block)
     block = getCurrentBlock();

   if (trace_enabled)
      traceMsg(comp(), "Generating Node %p into treetop in block %d (%p)\n", n, block->getNumber(), block);

   return TR::Node::genTreeTop(n, block);
   }

/**
 * Returns a SymbolReference for a stack slot.
 *
 * \note This symbol reference is relative to the privatized SP
 * captured on entry.
 */
TR::SymbolReference *
RubyIlGenerator::getStackSymRef(int32_t stackHeight)
   {
   CS2::HashIndex hashIndex = 0;
   if (_stackSymRefs.Locate(stackHeight, hashIndex))
      return _stackSymRefs.DataAt(hashIndex);

   int32_t offset = TR_RubyFE::SLOTSIZE * stackHeight;
   auto symRef = symRefTab()->createRubyNamedShadowSymRef("stackSlot", TR_RubyFE::slotType(), TR_RubyFE::SLOTSIZE, offset, true);
   _stackSymRefs.Add(stackHeight, symRef);

   traceMsg(comp(), "Created stack symRef with spOffset = %d ( stackHeight %d SLOTSIZE %d)\n", offset, stackHeight, TR_RubyFE::SLOTSIZE);

   return symRef;
   }

TR::SymbolReference *
RubyIlGenerator::getLocalSymRef(lindex_t idx, rb_num_t level)
   {
   Local key(idx, level);
   CS2::HashIndex hashIndex = 0;
   if (_localSymRefs.Locate(key, hashIndex))
      return _localSymRefs.DataAt(hashIndex);
   else
      {
      char *name = const_cast<char*>(getLocalName(idx, level));

      int32_t offset = - TR_RubyFE::SLOTSIZE * idx;
      auto symRef = symRefTab()->createRubyNamedShadowSymRef(name, TR_RubyFE::slotType(), TR_RubyFE::SLOTSIZE, offset, true);
      _localSymRefs.Add(key, symRef);

      return symRef;
      }
   }

TR::SymbolReference *
RubyIlGenerator::getHelperSymRef(TR_RuntimeHelper helper)
   {
   auto symRef = symRefTab()->findOrCreateRubyHelperSymbolRef(helper,
                                                               true,    /*canGCandReturn*/
                                                               true,    /*canGCandExcept*/
                                                               false);  /*preservesAllRegisters*/
   return symRef;
   }

TR::Node *
RubyIlGenerator::getlocal(lindex_t idx, rb_num_t level)
   {
   // val = *(ep - idx);
   TR::Node *load = xloadi(getLocalSymRef(idx, level),
                            loadEP(level));
   genTreeTop(load); // anchor - the value may go onto
                     // the stack and a store may be done
                     // to the local in the interim
   return load;
   }

TR::Node *
RubyIlGenerator::setlocal(lindex_t idx, rb_num_t level)
   {
   // *(ep - idx) = val
   auto value = pop();
   TR::Node *store = xstorei(getLocalSymRef(idx, level),
                              loadEP(level),
                              value);
   genTreeTop(store);
   return store;
   }

TR::Node *
RubyIlGenerator::getinstancevariable(VALUE id, VALUE ic)
   {
   return genCall(RubyHelper_vm_getivar, TR::Node::xcallOp(), 5,
                  loadSelf(),
                  TR::Node::xconst(id),
                  TR::Node::xconst(ic),
                  TR::Node::aconst(0), /* ci */
                  TR::Node::xconst(0)); /* is_attr */
   }

TR::Node *
RubyIlGenerator::aset_with(CALL_INFO ci, VALUE key)
   {
   // Pop order matters
   auto* val    = pop();
   auto* recv   = pop();

   //Shortcut by using xconst asuming sizes are correct
   static_assert(sizeof(ci) == sizeof(VALUE), "CI can't use xconst");
   auto * ci_n  = TR::Node::xconst((uintptr_t)ci);
   auto * key_n = TR::Node::xconst((uintptr_t)key);

   return genCall(RubyHelper_vm_opt_aset_with, TR::Node::xcallOp(), 5,
                  loadThread(),
                  ci_n,
                  key_n,
                  recv,
                  val);

   }

TR::Node *
RubyIlGenerator::aref_with(CALL_INFO ci, VALUE key)
   {
   auto* recv   = pop();

   //Shortcut by using xconst asuming sizes are correct
   static_assert(sizeof(ci) == sizeof(VALUE), "CI can't use xconst");
   auto * ci_n  = TR::Node::xconst((uintptr_t)ci);
   auto * key_n = TR::Node::xconst((uintptr_t)key);

   return genCall(RubyHelper_vm_opt_aref_with, TR::Node::xcallOp(), 4,
                  loadThread(),
                  ci_n,
                  key_n,
                  recv);

   }




TR::Node *
RubyIlGenerator::setinstancevariable(VALUE id, VALUE ic)
   {
   auto value = pop();
   return genCall(RubyHelper_vm_setivar, TR::Node::xcallOp(), 6,
                  loadSelf(),
                  TR::Node::xconst(id),
                  value,
                  TR::Node::xconst(ic),
                  TR::Node::aconst(0), /* ci */
                  TR::Node::xconst(0)); /* is_attr */
   }

TR::Node *
RubyIlGenerator::getglobal(GENTRY entry)
   {
   return genCall(RubyHelper_rb_gvar_get, TR::Node::xcallOp(), 1,
                  TR::Node::aconst((uintptr_t)entry));
   }

TR::Node *
RubyIlGenerator::setglobal(GENTRY entry)
   {
   auto value = pop();
   return genCall(RubyHelper_rb_gvar_set, TR::Node::xcallOp(), 2,
                  TR::Node::aconst((uintptr_t)entry),
                  value);
   }

int32_t
RubyIlGenerator::genGoto(int32_t target)
   {
   // if (_blocks[target]->getEntry()->getNode()->getByteCodeIndex()
   //      <=       _block->getEntry()->getNode()->getByteCodeIndex())
   //     genAsyncCheck();

   genTreeTop(TR::Node::create(TR::Goto, 0, genTarget(target)));
   return findNextByteCodeToGen();
   }

TR::Node *
RubyIlGenerator::opt_unary(TR_RuntimeHelper helper, VALUE ci)
   {
   TR::Node *recv = pop();
   return genCall(helper, TR::Node::xcallOp(), 3,
                  loadThread(),
                  TR::Node::aconst((uintptr_t)ci),
                  recv);
   }

TR::Node *
RubyIlGenerator::opt_binary(TR_RuntimeHelper helper, VALUE ci)
   {
   TR::Node *obj  = pop();
   TR::Node *recv = pop();
   return genCall(helper, TR::Node::xcallOp(), 4,
                  loadThread(),
                  TR::Node::aconst((uintptr_t)ci),
                  recv,
                  obj);
   }

TR::Node *
RubyIlGenerator::opt_ternary(TR_RuntimeHelper helper, VALUE ci)
   {
   TR::Node *third= pop();
   TR::Node *obj  = pop();
   TR::Node *recv = pop();
   return genCall(helper, TR::Node::xcallOp(), 5,
                  loadThread(),
                  TR::Node::aconst((uintptr_t)ci),
                  recv,
                  obj,
                  third);
   }

TR::Node *
RubyIlGenerator::opt_neq(TR_RuntimeHelper helper, VALUE ci, VALUE ci_eq)
   {
   TR::Node *obj  = pop();
   TR::Node *recv = pop();
   return genCall(helper, TR::Node::xcallOp(), 5,
                  loadThread(),
                  TR::Node::aconst((uintptr_t)ci),
                  TR::Node::aconst((uintptr_t)ci_eq),
                  recv,
                  obj);
   }

/*
 * Generate ruby stack adjustment.
 */
void
RubyIlGenerator::genRubyStackAdjust(int32_t slots)
   {
   auto spStore =
      storeSP(
         TR::Node::axadd(
            loadSP(),
            TR::Node::xconst(slots * TR_RubyFE::SLOTSIZE)));
   genTreeTop(spStore);
   }

/**
 * Store back current real stack height.
 *
 * \note  It seems that we should be able to elide SP rematerializations where the
 *        abstract stack height is zero, (and therefore should already be
 *        equal to privateSP). However, this is not the case.
 *
 *        It is relatively simple for the abstract stack pointer and real
 *        stack pointer to get out of sync when trying to elide remats.
 *
 *        Consider the following case:
 *
 *            expandarray [1,2,3]
 *            setlocal l1
 *            setlocal l2
 *            setlocal l3
 *            expandarray [4,5,6]
 *
 *        We ilgen expandaray into a VM helper call, rematerializing SP first.
 *
 *        The helper pushes the real stack pointer up, however, when we ilgen
 *        it, we immediately consume the expanded array values back onto the
 *        abstract stack. We then flush the abstract stack back to a set of
 *        locals, and call the expandarray helper again.
 *
 *        Note that the first expand array will have added 3 to the SP, but
 *        when we call rematerializeSP the second time, the abstract stack height
 *        is zero again. If we didn't emit the remat, at this point, the
 *        expandarray helper would write three slots **above** where we think
 *        the stack pointer is.  We then will reload the values from the *original*
 *        expandarray.
 *
 */
void
RubyIlGenerator::rematerializeSP()
   {
   traceMsg(comp(), "Rematerializing SP, adding %d\n", _stack->size());
   auto spStore =
      storeSP( TR::Node::axadd(
                               loadPrivateSP(),
                               TR::Node::xconst(_stack->size() * TR_RubyFE::SLOTSIZE)));
      genTreeTop(spStore);
   }

/**
 * Generate a load off the ruby stack.
 *
 * \note The node passed in is the privatized stack pointer.
 */
TR::Node *
RubyIlGenerator::loadFromRubyStack(TR::Node *privateSP, int32_t slot)
   {
   auto *shadow = getStackSymRef(slot);
   auto load = xloadi(shadow,
                      privateSP);
   return load;
   }

/**
 * Generate a call into a ruby helper.
 *
 * In order to do this safely we must restore the state expected by the machine,
 * which means storing out an up to date value for the interpreter's PC
 */
TR::Node *
RubyIlGenerator::genCall(TR_RuntimeHelper helper, TR::ILOpCodes opcode, int32_t num, ...)
   {
   va_list args;
   va_start(args, num);
   TR::Node *callNode = TR::Node::create(opcode, num);
   for (int i = 0; i < num; i++)
      {
      callNode->setAndIncChild(i, va_arg(args, TR::Node *));
      }
   va_end(args);

   auto *symRef = getHelperSymRef(helper);
   callNode->setSymbolReference(symRef);

   auto byteCodePC = TR::Node::aconst((uintptrj_t)& (mb().bytecodesEncoded()[byteCodeLength(current()) + currentByteCodeIndex()]));

   handleSideEffect(callNode);

   genTreeTop(storePC(byteCodePC)); //Store PC. This comes after handleSideEffect to enable the simple fastpathing code
   genTreeTop(callNode);

   return callNode;
   }


void
RubyIlGenerator::dumpCallInfo(rb_call_info_t* ci)
   {


   traceMsg(comp(), "rb_call_info_t %p\n\t"
            "{mid = %d,\n\t"
            "flag = %d, \n\t"
            "orig_argc = %d, \n\t"
            "blockiseq = %p, \n\t"
            "method_state = %d, \n\t"
            "class_serial = %d, \n\t"
            "klass = %d, \n\t"
            "me = %p, \n\t"
            "defined_class = %p, \n\t"
            "argc = %d, \n\t"
            "blockptr = %p, \n\t"
            "recv = %d,\n\t"
            "\taux ={opt_pc = %d, index = %d, missing_reason = %d, inc_sp = %d},\n\t"
            " call = %p}\n",
            ci,
            ci->mid,
            ci->flag,
            ci->orig_argc,
            ci->blockiseq,
            ci->method_state,
            ci->class_serial,
            ci->klass,
            ci->me,
            ci->defined_class,
            ci->argc,
            ci->blockptr,
            ci->recv,
            ci->aux.opt_pc, ci->aux.index, ci->aux.missing_reason, ci->aux.inc_sp,
            ci->call);

   traceMsg(comp(), "Flag Word: 0 | ");
   if (ci->flag & VM_CALL_FCALL           ) traceMsg(comp(),"VM_CALL_FCALL           |");
   if (ci->flag & VM_CALL_VCALL           ) traceMsg(comp(),"VM_CALL_VCALL           |");
   if (ci->flag & VM_CALL_TAILCALL        ) traceMsg(comp(),"VM_CALL_TAILCALL        |");
   if (ci->flag & VM_CALL_SUPER           ) traceMsg(comp(),"VM_CALL_SUPER           |");
   if (ci->flag & VM_CALL_OPT_SEND        ) traceMsg(comp(),"VM_CALL_OPT_SEND        |");
   if (ci->flag & VM_CALL_ARGS_SPLAT      ) traceMsg(comp(),"VM_CALL_ARGS_SPLAT      |");
   if (ci->flag & VM_CALL_ARGS_BLOCKARG   ) traceMsg(comp(),"VM_CALL_ARGS_BLOCKARG   |");
   traceMsg(comp()," | 0\n");
   }

/**
 * Determine the appropriate number of arguments for a call based on a
 * `rb_call_info_t` and calltype.
 */
uint32_t
RubyIlGenerator::computeNumArgs(rb_call_info_t* ci, CallType type)
   {
   bool blockarg = ci->flag & VM_CALL_ARGS_BLOCKARG;
   auto numArgs  = 0;
   switch (type)
      {
      case CallType_send:
         numArgs = 1 /* receiver */ + ci->orig_argc;
         if (blockarg) numArgs++;
         break;

      case CallType_send_without_block:
         numArgs = 1 /* receiver */ + ci->orig_argc;
         if (blockarg)
            {
            logAbort("send_without_block: Oddly... has unsupported block arg","send_without_has_blockarg");
            //Unreachable.
            }
         break;

      case CallType_invokesuper:
         numArgs = 1 /* receiver */ + ci->orig_argc ;
         if (blockarg) numArgs++;
         break;

      case CallType_invokeblock:
         numArgs = ci->orig_argc; //No receiver for invoke block.
         if (blockarg)
            {
            logAbort("invokeblock: Unsupported block arg","invokeblock_blockarg");
            }
         break;

      default:
         TR_ASSERT(false, "Unknown calltype\n");
      }

   return numArgs;
   }

/**
 * Generate a call to a ruby function that gets its arguments via the ruby stack.
 */
TR::Node *
RubyIlGenerator::genCall_ruby_stack(VALUE civ, CallType type)
   {
   // Basic operation:
   // - pop the arguments from our stack
   // - push them onto the ruby stack
   // - call appropriate handler.
   // - push the return value
   rb_call_info_t *ci = reinterpret_cast<rb_call_info_t*>(civ);

   if (trace_enabled) dumpCallInfo(ci);

   char * callTypes[] = {
      "CallType_send",
      "CallType_send_without_block",
      "CallType_invokesuper",
      "CallType_invokeblock"
   };

   if (ci->flag & VM_CALL_TAILCALL)
      {
      logAbort("jit doesn't support tailcall optimized iseqs","vm_call_tailcall");
      //unreachable.
      }

   int32_t pending   = _stack->size();
   auto    numArgs   = computeNumArgs(ci, type);
   int32_t restores  = pending - numArgs; // Stack elements not consumed by call.

   TR_ASSERT(restores >= 0,  "More args required than the stack has... %d = %d - %d (%s)\n", restores, pending, numArgs,  callTypes[type]);

   if (restores > 0 && feGetEnv("DISABLE_YARV_STACK_RESTORATION"))
      {
      logAbort("YARV stack restoration is disabled","yarv_stack_restoration_disabled");
      // Unreachable.
      }


   if (trace_enabled)
      traceMsg(comp(), "Creating call at bci=%d\n", _bcIndex);

   TR::Node *recv = 0;

   if (pending > 0)
      {
      rematerializeSP();
      //genRubyStackAdjust(pending);  //Bump stack pointer for callee.
      auto *privateSP = loadPrivateSP(); //

      //Write out arguments and pending slots
      for (int32_t i = 0; i < pending; i++)
         {
         TR::Node * val;
         if (i < numArgs)
            {
            val   = pop(); // Pop off args
            recv  = val;   // Save the receiver for those sends that need it.
            traceMsg(comp(), "\t[%d] writing argument to stack slot %d (N = %p)\n",
                     _bcIndex, pending - i - 1, val);
            }
         else
            {
            // We also write out non-args back to the stack in order to make OSR
            // workk.
            //
            // We peek at the pending trees however, to avoid having to reload
            // these values in a block.

            val   = topn(i - numArgs);
            traceMsg(comp(), "\t[%d] writing pending to stack slot %d (N = %p)\n",
                     _bcIndex, pending - i - 1, val);
            }

         auto *shadow = getStackSymRef(pending - i - 1);
         auto store = xstorei(shadow,
                              privateSP,
                              val);
         genTreeTop(store);

         }
      }

   TR::Node *callNode = 0;
   switch (type)
      {
      case CallType_send:
         callNode = genCall(RubyHelper_vm_send, TR::Node::xcallOp(), 3,
                            loadThread(),
                            TR::Node::aconst((uintptr_t)civ),
                            loadCFP());
         break;
      case CallType_send_without_block:
         TR_ASSERT(recv, "Reciever is null, despite sending-without-block\n");
         callNode = genCall(RubyHelper_vm_send_without_block, TR::Node::xcallOp(), 3,
                            loadThread(),
                            TR::Node::aconst((uintptr_t)civ),
                            recv);
         //Let the inliner take a pass at this.
         methodSymbol()->setMayHaveInlineableCall(true);
         break;
      case CallType_invokesuper:
         {
/*
         TR::Node *blockArg = !(ci->flag & VM_CALL_ARGS_BLOCKARG) ?
            genCall(RubyHelper_vm_get_block_ptr, TR::Node::xcallOp(), 1, loadEP()) :
            TR::Node::aconst(0);
*/
         callNode = genCall(RubyHelper_vm_invokesuper, TR::Node::xcallOp(), 3,
                            loadThread(),
                            TR::Node::aconst((uintptr_t)civ),
                            loadCFP());
         }
         break;

      case CallType_invokeblock:
         callNode = genCall(RubyHelper_vm_invokeblock, TR::Node::xcallOp(), 2,
                            loadThread(),
                            TR::Node::aconst((uintptr_t)civ));
         break;

      default:
         TR_ASSERT(0,                       "invalid call type");
         logAbort("invalid call type", "invalid_call_type");
         //unreachable.
      }

   if (restores > 0)
      {
      TR_ASSERT(pending > 0, "Reducing stack height where no buy occured!");
      //Return stack pointer to where it was before the buy for this call.
      // genRubyStackAdjust(-1 * restores );
      rematerializeSP();
      }

   return callNode;
   }

TR::Node *
RubyIlGenerator::createLocalArray(int32_t num)
   {
   // We create a localobject array (on-stack) to pass the arguments on
   TR::SymbolReference *localArray = comp()->getSymRefTab()->
      createLocalPrimArray(TR_RubyFE::SLOTSIZE * num, comp()->getMethodSymbol(), 8 /*FIXME: JVM-specific - byte*/);
   localArray->setStackAllocatedArrayAccess();

   TR::Node *argv = TR::Node::createWithSymRef(TR::loadaddr, 0, localArray);

   // The top of the stack becomes the last element
   // the stack better have atleast num elements
   for (int32_t i = 1; i <= num; ++i)
      {
      int32_t arrayIndex = num - i;
      TR::SymbolReference *shadow = comp()->getSymRefTab()->
         findOrCreateGenericIntShadowSymbolReference
         (arrayIndex * TR_RubyFE::SLOTSIZE, true /*allocUseDefBitVector*/);
      TR::Node *val = pop();
      genTreeTop(TR::Node::createWithSymRef(TR::astorei, 2, 2, argv, val, shadow));
      }

   return argv;
   }

TR::Node *
RubyIlGenerator::newrange(rb_num_t flag)
   {
   auto high = pop();
   auto low  = pop();

   return genCall(RubyHelper_rb_range_new, TR::Node::xcallOp(), 3,
                  low,
                  high,
                  TR::Node::xconst(flag));
   }

TR::Node *
RubyIlGenerator::putstring(VALUE str)
   {
   return genCall(RubyHelper_rb_str_resurrect, TR::Node::xcallOp(), 1,
                  TR::Node::xconst((uintptr_t)str));
   }

TR::Node *
RubyIlGenerator::newhash(rb_num_t num)
   {
   TR_ASSERT(num % 2 == 0, "newhash takes an even number of stack arguments");

   // TODO: RUBY_DTRACE_HASH_CREATE_ENABLED
   auto hash = genCall(RubyHelper_rb_hash_new, TR::Node::xcallOp(), 0);

   for (int i = num; i > 0; i -= 2)
      {
      auto value = topn(i-2);
      auto key   = topn(i-1);
      genCall(RubyHelper_rb_hash_aset, TR::Node::xcallOp(), 3, hash, key, value);
      }
   popn(num);
   return hash;
   }

void
RubyIlGenerator::trace(rb_num_t nf)
   {
   static_assert(sizeof(nf) == TR_RubyFE::SLOTSIZE, "xconst for trace isn't valid unless nf == sizeof slot");
   genCall(RubyHelper_vm_trace, TR::Node::xcallOp(), 2,
           loadThread(),
           TR::Node::xconst(nf));
   }

TR::Node *
RubyIlGenerator::newarray(rb_num_t num)
   {
   // put num elements into a local array called 'argv', then call:
   // VALUE rb_ary_new_from_values(rb_num_t num, VALUE *argv);
   static_assert(sizeof(num) == TR_RubyFE::SLOTSIZE, "xconst isn't valid unless size is slotsize");
   auto argv = createLocalArray(num);
   return genCall(RubyHelper_rb_ary_new_from_values, TR::Node::xcallOp(), 2,
                  TR::Node::xconst(num), argv);
   }

TR::Node *
RubyIlGenerator::duparray(VALUE ary)
   {
   auto node = TR::Node::xconst((uintptr_t)ary);
   return genCall(RubyHelper_rb_ary_resurrect, TR::Node::xcallOp(), 1, node);
   }

void
RubyIlGenerator::expandarray(rb_num_t numSlots, rb_num_t flag)
   {

   if (flag != 0)
      {
      logAbort("we do not support expandarray with flag other than 0 at the moment","expandarray_flag");
      //unreachable
      }

   // pops the array off the stack
   // push the contents of the array as values onto the ruby stack
   auto ary = pop();

   // This call writes to the stack relative to SP (as well as modifies the
   // value of SP), so we must rematerialize SP first to ensure we generate
   // loads afterwards from the right location.
   rematerializeSP();

   auto callNode = genCall(RubyHelper_vm_expandarray,
                           TR::call, 4, loadCFP(),
                           ary, TR::Node::xconst(numSlots),
                           TR::Node::xconst(flag));

   auto privateSP = loadPrivateSP();

   // TODO: The below is dead code, as we explicitly do not support a non-zero
   //       flag var, as checked above.
   //
   // if(flag & 0x1)
   //    numSlots += 1;

   auto startHeight = _stack->size();

   // We need to pop the arguments off the ruby stack and push them onto our stack
   // we do this from the deepest element to the top -- that way they end up on
   // our stack in the same order
   for (int i = 0; i < numSlots; i++)
      {
      traceMsg(comp(), "Extracting expanded array values, slot: %d = %d + %d + 1\n",
               i + startHeight, i, startHeight);
      auto val = loadFromRubyStack(privateSP, i + startHeight);
      genTreeTop(val); // Anchoring is necessary because SP will be adjusted below
                       // FIXME: Is anchoring still necessary when loading off private SP instead?
      push(val);
      }


   // traceMsg(comp(), "Returning %d slots to the stack\n", numSlots);
   // genRubyStackAdjust(-numSlots);
   }

TR::Node *
RubyIlGenerator::splatarray(rb_num_t flag)
   {
   auto ary = pop();
   return genCall(RubyHelper_vm_splatarray, TR::Node::xcallOp(), 2,
                  TR::Node::xconst(flag),
                  ary);
   }

TR::Node *
RubyIlGenerator::tostring()
   {
   auto obj = pop();
   return genCall(RubyHelper_rb_obj_as_string, TR::Node::xcallOp(), 1, obj);
   }

TR::Node *
RubyIlGenerator::concatstrings(rb_num_t num)
   {
   rb_num_t i = num - 1;
   auto val = genCall(RubyHelper_rb_str_resurrect, TR::Node::xcallOp(), 1, topn(i));

   while(i-- > 0)
      {
      auto v = topn(i);
      genCall(RubyHelper_rb_str_append, TR::Node::xcallOp(), 2, val, v);
      }
   popn(num);
   return val;
   }

TR::Node *
RubyIlGenerator::getspecial(rb_num_t key, rb_num_t value)
   {
   return genCall(RubyHelper_vm_getspecial, TR::Node::xcallOp(), 3,
         loadThread(),
         TR::Node::xconst(key),
         TR::Node::xconst(value));
   }

void
RubyIlGenerator::setspecial(rb_num_t key)
   {
   auto obj = pop();
   auto lep = genCall(RubyHelper_rb_vm_ep_local_ep, TR::Node::xcallOp(), 1, loadEP());
   genCall(RubyHelper_lep_svar_set, TR::Node::xcallOp(), 4,
         loadThread(),
         lep,
         TR::Node::xconst(key),
         obj);
   }

TR::Node *
RubyIlGenerator::putiseq(ISEQ iseq)
   {
   TR::Node *iseqNode = TR::Node::aconst((uintptr_t)iseq);
   return xloadi(_rb_iseq_struct_selfSymRef, iseqNode);
   }

TR::Node *
RubyIlGenerator::putspecialobject(rb_num_t value_type)
   {
   enum vm_special_object_type type = (enum vm_special_object_type) value_type;

   //Check if we can support this value_type.
   switch(type)
      {
      case VM_SPECIAL_OBJECT_CBASE:
      case VM_SPECIAL_OBJECT_CONST_BASE:
      case VM_SPECIAL_OBJECT_VMCORE:
         //We support these cases.
         break;
      default:
         logAbort("we do not support putspecialobject with this case","putspecialobject_value");
         break;
      }

   if(type != VM_SPECIAL_OBJECT_VMCORE)
      {
      TR_RuntimeHelper helper = (type == VM_SPECIAL_OBJECT_CBASE) ?
                                 RubyHelper_vm_get_cbase :
                                 RubyHelper_vm_get_const_base;

      return genCall(helper, TR::Node::xcallOp(), 2,
            load_CFP_ISeq(),
            loadEP());
      }
   else
      {
      TR_ASSERT(type == VM_SPECIAL_OBJECT_VMCORE, "putspecialobject in strange state.");

      // We can use RubyVMFrozenCore a compile time constant because:
      // 1. the object outlives all compiled methods
      // 2. objects are never moved by gc -- as long as that's true, 
      //    the following is safe.
      TR::Node *value = TR::Node::aconst(*(uintptrj_t*)fe()->getJitInterface()->globals.ruby_rb_mRubyVMFrozenCore_ptr);
      return value;
      }
   }


TR::Node *
RubyIlGenerator::getclassvariable(ID id)
   {
   auto cref = genCall(RubyHelper_rb_vm_get_cref, TR::Node::xcallOp(), 2,
                       load_CFP_ISeq(),
                       loadEP());
   auto klass = genCall(RubyHelper_vm_get_cvar_base, TR::Node::xcallOp(), 2,
                        cref,
                        loadCFP());
   return genCall(RubyHelper_rb_cvar_get, TR::Node::xcallOp(), 2,
                  klass,
                  TR::Node::xconst(id));
   }

void
RubyIlGenerator::setclassvariable(ID id)
   {
   auto val = pop();
   auto cref = genCall(RubyHelper_rb_vm_get_cref, TR::Node::xcallOp(), 2,
                       load_CFP_ISeq(),
                       loadEP());
   auto klass = genCall(RubyHelper_vm_get_cvar_base, TR::Node::xcallOp(), 2,
                        cref,
                        loadCFP());
   genCall(RubyHelper_rb_cvar_set, TR::Node::xcallOp(), 3,
           klass,
           TR::Node::xconst(id),
           val);
   }

TR::Node *
RubyIlGenerator::concatarray()
   {
   auto ary2st = pop();
   auto ary1   = pop();

   return genCall(RubyHelper_vm_concatarray, TR::Node::xcallOp(), 2,
                  ary1,
                  ary2st);
   }

TR::Node *
RubyIlGenerator::checkmatch(rb_num_t flag)
   {
   auto pattern = pop();
   auto target  = pop();

   return genCall(RubyHelper_vm_checkmatch, TR::Node::xcallOp(), 3,
                  TR::Node::xconst(flag),
                  target,
                  pattern);
   }

TR::Node *
RubyIlGenerator::toregexp(rb_num_t opt, rb_num_t cnt)
   {
   auto ary = genCall(RubyHelper_rb_ary_tmp_new, TR::Node::xcallOp(), 1, TR::Node::xconst(cnt));
   for(rb_num_t i=0; i < cnt; i++)
      {
      auto val = topn(i);
      genCall(RubyHelper_rb_ary_store, TR::Node::xcallOp(), 3,
            ary,
            TR::Node::xconst(cnt-i-1),
            val);
      }
   popn(cnt);
   return genCall(RubyHelper_rb_reg_new_ary, TR::Node::xcallOp(), 2,
         ary,
         TR::Node::iconst((int)opt));
   }

TR::Node*
RubyIlGenerator::opt_regexpmatch1(VALUE r)
   {
   auto obj = pop();
   return genCall(RubyHelper_vm_opt_regexpmatch1, TR::Node::xcallOp(), 2,
         TR::Node::xconst((uintptr_t)r),
         obj);
   }

TR::Node*
RubyIlGenerator::opt_regexpmatch2(CALL_INFO ci)
   {
   auto obj1 = pop();
   auto obj2 = pop();
   return genCall(RubyHelper_vm_opt_regexpmatch2, TR::Node::xcallOp(), 4,
                  loadThread(),
                  TR::Node::aconst((uintptr_t)ci),
                  obj2, //Autogenerated calback has obj2 before obj1.
                  obj1);
   }

TR::Node*
RubyIlGenerator::defined(rb_num_t op_type, VALUE obj, VALUE needstr)
   {
   auto v = pop();
  return genCall(RubyHelper_vm_defined, TR::Node::xcallOp(), 5,
                  loadThread(),
                  TR::Node::xconst(op_type),
                  TR::Node::xconst(obj),
                  TR::Node::xconst(needstr),
                  v);
   }

TR::Node *
RubyIlGenerator::getconstant(ID id)
   {
   auto klass = pop();

   return genCall(RubyHelper_vm_get_ev_const, TR::Node::xcallOp(), 5,
                  loadThread(),
                  // FIXME: make sure it is okay to use a constant for the iseq
                  // alternatively we can do th->cfp->iseq
                  TR::Node::aconst((uintptr_t)mb().iseq()),
                  klass,
                  TR::Node::xconst(id),
                  TR::Node::xconst(0));
   }

void
RubyIlGenerator::setconstant(ID id)
   {
   auto cbase = pop();
   auto val   = pop();
   genCall(RubyHelper_vm_check_if_namespace, TR::call, 1,
           cbase);
   genCall(RubyHelper_rb_const_set, TR::call, 3,
           cbase, TR::Node::xconst(id), val);
   }

TR::Node *
RubyIlGenerator::genCall_funcallv(VALUE civ)
   {
   // Basic Operation:
   // - put the arguments in a contiguous set of temps (argv)
   // - pop the receiver from our stack
   // - call rb_funcallv(recv, mid, argc, argv)
   // - push the return value
   rb_call_info_t *ci = reinterpret_cast<rb_call_info_t*>(civ);

   TR_ASSERT(0 == (ci->flag & VM_CALL_ARGS_BLOCKARG), "send: we don't support block arg just yet");

   TR::Node *argv     = createLocalArray(ci->orig_argc);
   TR::Node *receiver = pop();

   return genCall(RubyHelper_rb_funcallv, TR::Node::xcallOp(), 4,
                  receiver,
                  TR::Node::xconst(ci->mid),
                  TR::Node::xconst(ci->orig_argc),
                  argv);
   }

int32_t
RubyIlGenerator::jump(int32_t offset)
   {
   //TR::TreeTop *branchDestination = genTarget(_bcIndex + offset);
   // TR::TreeTop *branchDestination = _blocks[_bcIndex + offset]->getEntry();
   // TR::Node *gotoNode = TR::Node::create(NULL, TR::Goto);
   // gotoNode->setBranchDestination(branchDestination);
   // genTreeTop(gotoNode);

   uint32_t branchBCIndex = branchDestination(_bcIndex);

   if (branchBCIndex <= _bcIndex)
      genAsyncCheck();

   TR::TreeTop * branchDestination = genTarget(branchBCIndex);
   TR::Node *gotoNode = TR::Node::create(NULL, TR::Goto);
   gotoNode->setBranchDestination(branchDestination);
   genTreeTop(gotoNode);

   return findNextByteCodeToGen();
   }

int32_t
RubyIlGenerator::conditionalJump(bool branchIfTrue, int32_t offset)
   {
   auto value = pop();
   // (assuming branch if true)
   // if value is true { check interrupts ; jump to destination; }
   // test defined to be !((value & ~Qnil) == 0)

   // only Qnil and Qfalse are falsy values in ruby
   // Qnil is 8 (or something else) and Qfalse is 0

   static_assert(Qfalse == 0, "Qfalse must be 0 in Ruby");

   int32_t fallThruIndex = _bcIndex + byteCodeLength(current());
   genTarget(fallThruIndex);

   uint32_t branchBCIndex = branchDestination(_bcIndex);
   if (branchBCIndex <= _bcIndex)
      genAsyncCheck();

   TR::TreeTop *dest = genTarget(branchBCIndex);
   TR::Node *test = TR::Node::createif(branchIfTrue ? TR::Node::ifxcmpneOp() : TR::Node::ifxcmpeqOp(),
                                         TR::Node::xand(
                                            value,
                                            TR::Node::xconst(~Qnil)),
                                         TR::Node::xconst(0),
                                         dest);
   genTreeTop(test);

   return findNextByteCodeToGen();
   }

int32_t
RubyIlGenerator::getinlinecache(OFFSET offset, IC ic)
   {
   // The logic is as follows:
   // if (ic->ic_state == global_constant_state)  // cache hit
   //      { push(ic->ic_value.value); branch to offset; }
   // else { push(Qnil); }
   //
   // This is a little bit complicated, so we do the following instead:
   //
   // push ( cache-hit ? ic->value : Qnil )
   // ificmpne -> destination
   //    => cache-hit
   //    0

   // TODO: Q: How important is it for us to push Qnil? Usually that value
   // will be discarded on the miss path. Need to investigate.

   // This way the operand stack is properly managed by the byte code iterator
   // framework
   TR::Node *icNode = TR::Node::aconst((uintptr_t)ic);
   TR::Node *hit_p = TR::Node::create(TR::lcmpeq, 2,
                                        loadICSerial(icNode),
                                        getGlobalConstantState());
   TR::Node *ternary = TR::Node::xternary(hit_p,
                                 loadICValue(icNode),
                                 TR::Node::xconst(Qnil));
   // TODO: it would be nice to be able to put a bias hint on the ternary
   // node and have some later optimization pass expand it

   // Anchor and push
   genTreeTop(ternary);
   push(ternary);

   // Generate the fall through and destination targets
   // This needs to be done after the push so that the operand
   // gets carried across the edge in a temp
   //
   int32_t fallThruIndex = _bcIndex + byteCodeLength(current());
   genTarget(fallThruIndex);
   TR::TreeTop *dest = genTarget(branchDestination(_bcIndex));

   TR::Node *ifNode = TR::Node::createif(TR::ificmpne,
                                           hit_p,
                                           TR::Node::iconst(0),
                                           dest);
   genTreeTop(ifNode);
   return findNextByteCodeToGen();
   }

TR::Node *
RubyIlGenerator::setinlinecache(IC ic)
   {
   auto value = pop();
   TR::Node *icNode = TR::Node::aconst((uintptr_t)ic);
   genCall(RubyHelper_vm_setinlinecache, TR::call, 3,
           loadCFP(), icNode, value);
   return value;
   }

TR::Node*
RubyIlGenerator::generateCfpPop()
   {
   TR::Node *cfp = storeCFP(TR::Node::axadd(loadCFP(),
                                            TR::Node::xconst(sizeof(rb_control_frame_t))));
   return cfp;
   }

int32_t
RubyIlGenerator::genReturn(TR::Node *retval, bool popframe)
   {
   genAsyncCheck();

   // anchor retval before popping the frame
   genTreeTop(retval);

   if (popframe)
      {
      auto* cfp = generateCfpPop();
      // pop the frame
      // th->cfp++;
      genTreeTop(cfp);
      }

   genTreeTop(TR::Node::create(TR::areturn, 1, retval));
   return findNextByteCodeToGen();
   }

int32_t
RubyIlGenerator::genThrow(rb_num_t throw_state, TR::Node *throwobj)
   {
   TR::Node *val = genCall(RubyHelper_vm_throw, TR::Node::xcallOp(), 3,
                            loadThread(),
                            TR::Node::xconst(throw_state),
                            throwobj);
   return genReturn(val, false /*!popframe*/);
   }

void
RubyIlGenerator::genAsyncCheck()
   {
   TR::Node *check = TR::Node::createWithSymRef(TR::asynccheck,
                                                0,
                                                symRefTab()->findOrCreateAsyncCheckSymbolRef(_methodSymbol));
   genTreeTop(check);
   }

/**
 * Load ruby thread symref
 */
TR::Node *
RubyIlGenerator::loadThread()
   {
   return TR::Node::loadThread(_methodSymbol);
   }

/**
 * Load ruby displacement symref for a method.
 *
 * The displacement is the offset from the iseq base. In C code,
 * this works out to:
 *
 *     displacement = th->cfp->pc - th->cfp->iseq->iseq_encoded;
 *
 */
TR::Node *
RubyIlGenerator::loadDisplacement()
   {
   // TODO: 32 bit targets will need this tree to be conditionally retyped.
   return TR::Node::create(TR::l2a, 1,
                           TR::Node::create(TR::lsub, 2,
                                            TR::Node::create(TR::a2l, 1, loadPC()),
                                            TR::Node::lconst(reinterpret_cast<uintptrj_t>(mb().bytecodesEncoded()))));
   }


TR::Node *
RubyIlGenerator::loadCFP()
   {
   // iaload thread->cfp
   //    aload thread
   return TR::Node::createWithSymRef(TR::aloadi, 1, 1,
                            loadThread(),
                            _cfpSymRef);
   }

TR::Node *
RubyIlGenerator::storeCFP(TR::Node *val)
   {
   return TR::Node::createWithSymRef(TR::astorei, 2, 2,
                            loadThread(),
                            val,
                            _cfpSymRef);
   }

TR::Node *
RubyIlGenerator::loadSP()
   {
   return TR::Node::createWithSymRef(TR::aloadi, 1, 1,
                            loadCFP(),
                            _spSymRef);
   }

TR::Node *
RubyIlGenerator::storeSP(TR::Node *val)
   {
   return TR::Node::createWithSymRef(TR::astorei, 2, 2,
                            loadCFP(),
                            val,
                            _spSymRef);
   }

TR::Node *
RubyIlGenerator::storePC(TR::Node *val)
   {
   return TR::Node::createWithSymRef(TR::astorei, 2, 2,
                            loadCFP(),
                            val,
                            _pcSymRef);
   }

TR::Node *
RubyIlGenerator::loadPC()
   {
   return TR::Node::createWithSymRef(TR::aloadi, 1, 1,
                            loadCFP(),
                            _pcSymRef);
   }

TR::Node *
RubyIlGenerator::storeCFPFlag(TR::Node *val)
   {
   return TR::Node::createWithSymRef(TR::astorei, 2, 2,
                            loadCFP(),
                            val,
                            _flagSymRef);
   }

TR::Node *
RubyIlGenerator::loadCFPFlag()
   {
   return TR::Node::createWithSymRef(TR::aloadi, 1, 1,
                            loadCFP(),
                            _flagSymRef);
   }


TR::Node *
RubyIlGenerator::load_CFP_ISeq()
   {
   return TR::Node::createWithSymRef(TR::aloadi, 1, 1,
                            loadCFP(),
                            _iseqSymRef);
   }

TR::Node *
RubyIlGenerator::loadSelf()
   {
   return xloadi(_selfSymRef,
                 loadCFP());
   }

TR::Node  *
RubyIlGenerator::loadPrivateSP()
   {
   return TR::Node::createLoad(_privateSPSymRef);
   }

TR::Node *
RubyIlGenerator::loadEP(rb_num_t level)
   {
   TR::Node *ep = TR::Node::createWithSymRef(TR::aloadi, 1, 1,
                                     loadCFP(),
                                     _epSymRef);

   for (int i = 0; i < (int) level; ++i)
      {
      ep = loadPrevEP(ep);
      }
   return ep;
   }

TR::Node *
RubyIlGenerator::loadPrevEP(TR::Node *ep)
   {
   // Note: we need to use a shadow instead of _epSymRef as ep->ep
   // is a different thing from cfp->ep
   // 
   // We can provide better aliasing if we  use a specific shadow symbols
   // instead of generic-int-shadow
   TR::SymbolReference *shadow = comp()->getSymRefTab()->
      findOrCreateGenericIntShadowSymbolReference(0, true /*allocUseDefBitVector*/);

   return TR::Node::xand(xloadi(shadow,
                  ep),
           TR::Node::xconst(~3));// c.f. GET_PREV_EP(ep) in vm_insnshelper.h
   }

/**
 * Ensure all pending trees are written back to the YARV stack, and
 * become pending in my target.
 *
 * \note This routine will have to be modified if we ever start saving or
 *       restoring non stack-slot-sized elements.
 */
void
RubyIlGenerator::saveStack(int32_t targetIndex)
   {
   // FIXME: duplicated from j9 walker.cpp
   if (_stack->isEmpty())
      return;

   auto stackSize = _stack->size();

   bool createTargetStack = (targetIndex >= 0 && !_stacks[targetIndex]);
   if (createTargetStack)
      {
      _stacks[targetIndex] = new (trStackMemory())
         ByteCodeStack(trMemory(), std::max<uint32_t>(20, _stack->size()));
      _pendingTreesOnEntry[targetIndex] = _stack->size();
      }
   else
      {
      traceMsg(comp(), "Target stack for targetIndex %d not created.\n",targetIndex);
      traceMsg(comp(), "-- %d == %d?\n",_stack->size(),_stacks[targetIndex]->size());
      TR_ASSERT(_stack->size() == _stacks[targetIndex]->size(), "Stack height mismatch!");
      }

   for (auto i = 0; i < stackSize; ++i)
      if (_stackTemps.topIndex() < i || _stackTemps[i] != _stack->element(i))
         handlePendingPushSaveSideEffects(_stack->element(i));

   // In the future, this can be made non-static in the cases where we
   // understand control flow must be passed along the JIT rather than
   // coming from the interpreter.
   static bool useTemps = feGetEnv("DEBUG_GET_TEMPS");

   if (!useTemps)
      {
      // When we save values to the stack, it must be ensured that we update
      // the stack pointer. Should the next block use a routine which can touch
      // the stack, values stored to the stack can be lost. Since seemingly
      // innocuous calls like vm_opt_plus may, in the general case, write to
      // the stack, it is safest to simply update the stack pointer here.
      rematerializeSP();
      }

   for (auto i = 0; i < _stack->size(); ++i)
      {
      // The node to be saved.
      TR::Node * n = _stack->element(i);

      TR::SymbolReference * symRef = useTemps ?
         symRefTab()->findOrCreatePendingPushTemporary(_methodSymbol, i, n->getDataType()) :
         getStackSymRef(i);

      if (_stackTemps.topIndex() < i || _stackTemps[i] != n)
         {
         traceMsg(comp(), "Saving node %p to %s slot %d in %d -> %d\n",
                  n,
                  useTemps ? "PPS" : "YARV stack",
                  i,
                  _bcIndex,
                  targetIndex);

         auto *store = useTemps ?
            TR::Node::createStore(symRef, n) :
            xstorei(symRef, loadPrivateSP(), n);

         genTreeTop(store);
         _stackTemps[i] = n;
         }

      // this arranges that the saved slot is reloaded on entry to the successor
      // basic block, which is the one starting at bytecode index targetIndex
      if (createTargetStack)
         {
         traceMsg(comp(), "creating load from %s slot %d in blockIndex %d -> %d \n",
                  useTemps ? "PPS" : "YARV stack",
                  i,
                  _bcIndex,
                  targetIndex);

         auto *load = useTemps ?
            TR::Node::createLoad(symRef) :
            xloadi(symRef, loadPrivateSP());

         (*_stacks[targetIndex])[i] = load;
         }

      }

   }

/**
 * `saveStack` assumes the execution mode of bytecode rather than the TR IL.
 * The saveStack method simply walks the simulated operand stack and generates
 * a store treetop for each Node it finds on the simulated stack. This, like
 * bytecode, assumes that operations are completed at the time values are
 * pushed on the simulated stack.
 *
 * Meanwhile, TR IL is being generated that refers to Nodes popped off the
 * stack. The assumption here is that the order of operations is unconstrained
 * other than by the data flow implied by the trees of references to nodes and
 * by the control dependancies imposed by the order of treetops. The latter are
 * implied by the semantics of each bytecode.
 *
 * The job of handlePendingPushSaveSideEffects is to add additional implicit
 * control dependancies to the TR IL caused by the decompilation points.  All
 * loads of the PPS save region must occur before decompilation points except
 * in the specfic case when the load would redundantly reload a value already
 * in the PPS.
 */
void
RubyIlGenerator::handlePendingPushSaveSideEffects(TR::Node * n)
   {
   if (_stack->size() == 0)
      return;

   handlePendingPushSaveSideEffects(n, comp()->incVisitCount());
   }

void
RubyIlGenerator::handlePendingPushSaveSideEffects(TR::Node * n, vcount_t visitCount)
   {
   if (n->getVisitCount() == visitCount)
      return;

   n->setVisitCount(visitCount);
   for (int32_t i = n->getNumChildren() - 1; i >= 0; --i)
      handlePendingPushSaveSideEffects(n->getChild(i), visitCount);

   //  \FIXME: This doesn't correctly handle stack saves (which are indirect var
   //          loads). Furthermore it assumes Java slot numbering.
   //
   //          It's not clear at all to me what cases this is catching, especially
   //          since it doesn't check for side-effects at all.
   if (n->getOpCode().isLoadVarDirect() && n->getSymbolReference()->getCPIndex() < 0)
      {
      int32_t stackSlot = -n->getSymbolReference()->getCPIndex() - 1;

      int32_t numSlots = 0;
      int32_t adjustedStackSlot = stackSlot;
      int32_t i;
      for (i = 0; i < _stack->size(); ++i)
         {
         TR::Node * stackNode = _stack->element(i);
         if (numSlots == stackSlot)
            adjustedStackSlot = i;
         numSlots += stackNode->getNumberOfSlots();
         }

      if ((_stack->topIndex() >= adjustedStackSlot) && (_stack->element(adjustedStackSlot) != n))
         genTreeTop(n);
      }
   }

/**
 * If any tree on the stack can be modified by the sideEffect then the node
 * on the stack must become a treetop before the sideEffect node is evaluated
 */
void
RubyIlGenerator::handleSideEffect(TR::Node * sideEffect)
   {

   for (int32_t i = 0; i < _stack->size(); ++i)
      {
      TR::Node * n = _stack->element(i);
      if (n->getReferenceCount() == 0 && valueMayBeModified(sideEffect, n))
         genTreeTop(n);
      }
   }

bool
RubyIlGenerator::valueMayBeModified(TR::Node * sideEffect, TR::Node * node)
   {
   if (node->getOpCode().hasSymbolReference() && sideEffect->mayModifyValue(node->getSymbolReference()))
      return true;

   int32_t numChilds = node->getNumChildren();
   for (int32_t i = 0; i < numChilds; ++i)
      if (valueMayBeModified(sideEffect, node->getChild(i)))
         return true;

   return false;
   }

TR::Block *
RubyIlGenerator::genExceptionHandlers(TR::Block *lastBlock)
   {
   for (auto iter = _tryCatchInfo.begin();
        iter != _tryCatchInfo.end();
        ++iter)
      {
      TryCatchInfo &handlerInfo = *iter;

      TR::Block *catchBlock = handlerInfo._catchBlock;
      for (auto j = handlerInfo._startIndex;
           j <= handlerInfo._endIndex;
           ++j)
         {
         if (blocks(j) && cfg()->getNodes().find(blocks(j)))
            {
            if (blocks(j) == catchBlock)
               {
               TR_ASSERT(0, "exception loop?");
               }

            cfg()->addExceptionEdge(blocks(j), catchBlock);
            }
         }
      }

   return lastBlock;
   }
