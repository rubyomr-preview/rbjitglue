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


#include "optimizer/RubyIlFastpather.hpp"

#include "env/RubyFE.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Block.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Annotations.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "ruby/version.h"

#define OPT_DETAILS "O^O RUBYILFASTPATHER: "

static
char* getBOPName(int32_t bop)
   {

   //These functions need to be in-sync with respect to the BOP's supported:
   // Ruby::IlFastpather::IlFastpather
   // Ruby::IlFastpather::loadBOPRedefined
   // getBOPName (this)
   // Ensure all three are updated when adding BOPs.

   switch(bop)
      {
      case BOP_PLUS:
         return "redefined_flag[BOP_PLUS]";
      case BOP_MINUS:
         return "redefined_flag[BOP_MINUS]";
      case BOP_GE:
         return "redefined_flag[BOP_GE]";
      default:
         return "ERROR: UNSUPPORTED BOP FLAG";
      }
   }

Ruby::IlFastpather::IlFastpather(TR::OptimizationManager *manager)
   : TR::Optimization(manager) 
   {

   //Initialize VMEventFlag SymRef.
   TR::DataType dt = TR::NoType;
   switch (sizeof(rb_event_flag_t))
      {
      case 4:
         dt = TR::Int32; break;
      case 8:
         dt = TR::Int64; break;
      default:
         TR_ASSERT(0, "Unknown rb_event_flag_t size");
         break;
      };
   comp()->getSymRefTab()->findOrCreateRubyVMEventFlagsSymbolRef("ruby_vm_event_flag",
                                                                        dt,
                                                                        static_cast<TR_RubyFE*>(fe())->getJitInterface()->globals.ruby_vm_event_flags_ptr,
                                                                        0,
                                                                        false);

   //Initialize SymRefs for all BOPs
   comp()->getSymRefTab()->initializeRubyRedefinedFlagSymbolRefs(BOP_LAST_);

   //These functions need to be in-sync with respect to the BOP's supported:
   // Ruby::IlFastpather::IlFastpather (this)
   // Ruby::IlFastpather::loadBOPRedefined
   // getBOPName
   // Ensure all three are updated when adding BOPs.

   for(int32_t bop=BOP_PLUS; bop < BOP_LAST_; bop++)
      {
      //Only create SymRefs for BOPs we support.
      switch(bop)
         {
         case BOP_PLUS:
         case BOP_MINUS:
         case BOP_GE:
            comp()->getSymRefTab()->findOrCreateRubyRedefinedFlagSymbolRef(bop,
                                                                        getBOPName(bop),
                                                                        TR::Int16,
                                                                        &(static_cast<TR_RubyFE*>(fe())->getJitInterface()->globals.redefined_flag_ptr[bop]),
                                                                        0,
                                                                        true);
            break;
         default:
            /* Do nothing */
            break;
         }
      }
   }


/** 
 * Extract some of the semantics of ruby calls into trees, providing a
 * fast-path version. 
 *
 * Executes on a treetop basis. 
 * 
 */ 
int32_t
Ruby::IlFastpather::perform() 
   {
   if (trace())
      traceMsg(comp(), OPT_DETAILS "Processing method: %s\n", comp()->signature());

   // Currently a block is marked cold by this pass when created 
   // as a way of avoiding reprocessing generated call blocks. 
   //
   // FIXME: given that we'd really like to be flexible about 
   // where/when this optimization runs, a side table tracking 
   // block status should be preferred. 
   auto lastTT = cfg()->findLastTreeTop();
   for (auto tt = comp()->getMethodSymbol()->getFirstTreeTop();
        tt != lastTT;
        tt = tt->getNextTreeTop())
      {
      if (!tt->getEnclosingBlock()->isCold())
      performOnTreeTop(tt); 
      }

   return 0;
   } 

/**
 * Transform a tree top. 
 */
void
Ruby::IlFastpather::performOnTreeTop(TR::TreeTop* tt) 
   {
   auto *node = tt->getNode(); 
   if (node->getOpCodeValue() == TR::treetop) 
      node = node->getFirstChild(); 
   
   if (node->getOpCode().isCall() && 
          node->getSymbol()->castToMethodSymbol()->isHelper())
      {
      auto refNum = node->getSymbolReference()->getReferenceNumber();
      switch (refNum)
         {
         case RubyHelper_vm_trace: 
            fastpathTrace(tt, node); 
            break; 

         // case RubyHelper_vm_opt_plus:
         // case RubyHelper_vm_opt_minus:
         //    fastpathPlusMinus(tt, node, refNum == RubyHelper_vm_opt_plus ); 
            break;
         default:
            return; 
         }

      }

   }

/**
 * Create a treetop for node, store to a temp, and insert before the passed 
 * treetop.  
 * 
 * \return Symbol reference of the temp. 
 */
TR::SymbolReference * 
Ruby::IlFastpather::storeToTempBefore(TR::Node *node, TR::TreeTop *tt)
   {
   TR::SymbolReference *tempSymRef = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), node->getDataType());
   tt->insertBefore(TR::TreeTop::create(comp(), TR::Node::createStore(tempSymRef, node)));
   return tempSymRef; 
   }



/**
 * Fastpath calls to vm_trace. 
 *
 * We can fastpath calls to VM trace by checking first the ruby_vm_event_flag. 
 *
 * @TODO: It would be ideal if we were able to also move the PC store, and any
 * 'pending push' stores, that preceed this call beneath the branch as well. 
 */
void
Ruby::IlFastpather::fastpathTrace(TR::TreeTop *tt, TR::Node *node) 
   {
   TR_ASSERT(node && tt, "Must have node and treetop passed in"); 
   TR_ASSERT(node->getNumChildren() == 2, "Wrong arg count on vm_trace call");

   auto containing_block = tt->getEnclosingBlock(); 

   if (!performTransformation(comp(), "%s Fastpathing %s on TT %p (%p)\n", OPT_DETAILS, "vm_trace", tt, node))
      return; 

   //Anchor and store out children so we can reference across control flow
   auto thread  = node->getChild(0);
   auto flag    = node->getChild(1);
   TR_ASSERT(thread  && flag, "null child..." ); 

   TR::SymbolReference *tempThread = storeToTempBefore(thread,  tt);
   TR::SymbolReference *tempFlag   = storeToTempBefore(flag,    tt);

   //create test
   auto test             = genTraceTest(flag);
   auto test_tt          = TR::TreeTop::create(comp(), test); 

   //Create new call node for if branch, loading from temps. 
   auto new_call_node = TR::Node::create(node->getOpCodeValue(), node->getNumChildren());
   new_call_node->setSymbolReference(node->getSymbolReference()); 
   new_call_node->setAndIncChild(0, TR::Node::createLoad(tempThread)); 
   new_call_node->setAndIncChild(1, TR::Node::createLoad(tempFlag)); 

   //New call, treetopped. 
   auto new_call = TR::TreeTop::create(comp(), TR::Node::create(TR::treetop, 1, new_call_node) ) ; 

   containing_block->createConditionalBlocksBeforeTree(tt, test_tt, new_call, NULL, cfg(), true, true);
   
   }

/**
 * Fastpath calls to vm_opt_plus or vm_opt_minus
 */
void
Ruby::IlFastpather::fastpathPlusMinus(TR::TreeTop *tt, TR::Node *node, bool isPlus)
   {
#if 0

   auto* block = tt->getEnclosingBlock();

   if (!performTransformation(comp(), "%s Fastpathing %s on TT %p\n", OPT_DETAILS, isPlus ? "plus" : "minus", tt))
      return; 

   TR::Block *Bfast, *Bslow, *Btail;
   CS2::ArrayOf<TR::Block *, TR::Allocator> intermediateBlocks(comp()->allocator());

   //comp()->dumpMethodTrees("before fastPathPlusMinus");

   // We need to anchor these before the tree so that we can use
   // them across the control flow
   auto ciConst = node->getChild(1);
   auto a       = node->getChild(2);
   auto b       = node->getChild(3);
   TR::Node::anchorBefore(ciConst, tt);
   TR::Node::anchorBefore(a,       tt);
   TR::Node::anchorBefore(b,       tt);
   
   createMultiDiamond(tt, block, 3, Bfast, Bslow, Btail, intermediateBlocks);

   TR::Block *B1 = intermediateBlocks[0];
   TR::Block *B2 = intermediateBlocks[1];
   TR::Block *B3 = intermediateBlocks[2];

   // We need to store a, b to temps so that we can load them back in Bslow
   TR::SymbolReference *tempA = TR::Node::storeToTemp(a, block);
   TR::SymbolReference *tempB = TR::Node::storeToTemp(b, block);

   auto ifRedefined = genRedefinedTest(isPlus ? BOP_PLUS : BOP_MINUS,
                                       FIXNUM_REDEFINED_OP_FLAG,
                                       Bslow->getEntry());
   TR::Node::genTreeTop(ifRedefined, block);

   auto ifAFixnum = genFixNumTest(a);
   TR::Node::genTreeTop(ifAFixnum, B1);
   ifAFixnum->setBranchDestination(Bslow->getEntry());

   auto ifBFixnum = genFixNumTest(b);
   TR::Node::genTreeTop(ifBFixnum, B2);
   ifBFixnum->setBranchDestination(Bslow->getEntry());

   TR::Node *ifOverflow;
   if (isPlus)
      {
      // if (a + (b-1) ) overflows ->
      ifOverflow = TR::Node::createif(TR::iflcmno,
                                       a,
                                       TR::Node::xsub(
                                          b,
                                          TR::Node::xconst(1)));
      }
   else
      {
      // if (a - b) overflows ->
      ifOverflow = TR::Node::createif(TR::iflcmpo,
                                       a,
                                       b);
      }
   TR::Node::genTreeTop(ifOverflow, B3);
   ifOverflow->setBranchDestination(Bslow->getEntry());

   // FIXME: it would be nice to be able to combine the overflow test with this ad//subtract
   // we will need a new fused opcode for that modeled over the condCode opcode
   TR::Node *result;
   if (isPlus)
      {
      result =
         TR::Node::xadd(
            a,
            TR::Node::xsub(
               b,
               TR::Node::xconst(1)));
      }
   else
      {
      // Note that we want to do: (a - b) | 1, however since we know that the bottom bit
      // of (a - b) is necessarily 0, we can do (a - b) + 1 instead. This is more
      // optimizable than the or/sub combo
      result =
         TR::Node::xadd(
            TR::Node::xsub(
               a,
               b),
            TR::Node::xconst(1));
      }
   TR::SymbolReference *tempResult = TR::Node::storeToTemp(result, Bfast);

   // TODO: Generate a call to vm_call_simple in Bslow
   // That is cumbersome, it is easier to keep the call going to vm_opt_minus
   TR::Node *newCall = TR::Node::createCallNode(TR::Node::xcallOp(),
                                                        node->getSymbolReference(),
                                                        4,
                                                        TR::Node::loadThread(optimizer()->getMethodSymbol()),
                                                        TR::Node::aconst(ciConst->getAddress()),
                                                        TR::Node::createLoad(tempA),
                                                        TR::Node::createLoad(tempB));
   TR::Node::genTreeTop(TR::Node::createStore(tempResult, newCall),
              Bslow);
   TR::Node *gotoNode = TR::Node::create(TR::Goto, 0);
   TR::Node::genTreeTop(gotoNode, Bslow);
   gotoNode->setBranchDestination(Btail->getEntry());

   // Now change the original call node in Btail to be a load
   node = TR::Node::recreate(node,  
      TR::Node::xloadOp(static_cast<TR_RubyFE*>(TR::comp()->fe())));

   node->setSymbolReference(tempResult);
   node->removeAllChildren();

   //comp()->dumpMethodTrees("after fastPathPlusMinus");
   // comp()->verifyTrees();
   // comp()->verifyBlocks();
   // comp()->verifyCFG();
#else
   TR_ASSERT_FATAL(false, "Fastpathing is disabled because of 2.2-2.4 changes"); 
#endif
   }

/**
 * Originally:
 *
 *     B:
 *       .. head ..
 *       splitTT
 *       .. tail ..
 * 
 * Is converted to:
 * 
 *     B:
 *       .. head ..
 *     intermediateBlock1:   -> Bslow
 *     intermediateBlock2:   -> Bslow
 *     ..
 *     intermediateBlockN:   -> Bslow
 *     Bfast:
 *     Btail:
 *       splitTT
 *       .. tail ..
 *     -----------------------
 *     <remainder of code>     
 *     -----------------------
 *     Bslow:
 *       goto Btail
 * 
 * 
 * Additionally:
 * * we add CFG edges from B & intermediateBlocks to Bslow
 * * we add CFG edges for fall through from B through Btail
 * * we mark the intermidateBlocks and Bfast as extenensions of the previous block
 * * we add a CFG edge from Bslow back to Btail
 * 
 * It is up to the caller to
 * * add the iftrees in each of the B and intermediate blocks
 * * to change 'splitTT' to a load of temp
 * * add a goto in Bslow back to Btail
 *
 */
void
Ruby::IlFastpather::createMultiDiamond(TR::TreeTop *splitTT,
                                       TR::Block *B,
                                       uint32_t numIntermediateBlocks,
                                       TR::Block * & Bfast,
                                       TR::Block * & Bslow,
                                       TR::Block * & Btail,
                                       CS2::ArrayOf<TR::Block *, TR::Allocator> &intermediateBlocks)
   {
   Btail = B->split(splitTT, cfg(), true /*fixupCommoning*/);

   Bslow = TR::Block::createEmptyBlock(comp());
   Bslow->setIsCold();
   cfg()->addNode(Bslow);
   cfg()->addEdge(B, Bslow);
   // TR::Node *gotoNode = TR::Node::create(TR::Goto, 0);
   // TR::Node::genTreeTop(gotoNode, Bslow);
   // gotoNode->setBranchDestination(Btail->getEntry());
   cfg()->addEdge(Bslow, Btail);

   TR::Block *Blast = comp()->findLastTree()->getNode()->getBlock();
   Blast->getExit()->join(Bslow->getEntry());

   TR::Block *Bprev = B;
   for (int i = 1; i <= numIntermediateBlocks; ++i)
      {
      TR::Block *Bi = TR::Block::createEmptyBlock(comp());
      cfg()->addNode(Bi);
      cfg()->addEdge(Bprev, Bi);
      cfg()->addEdge(Bi, Bslow);
      Bprev->getExit()->join(Bi->getEntry());
      Bi->setIsExtensionOfPreviousBlock();

      intermediateBlocks[i-1] = Bi;
      Bprev = Bi;
      }

   Bfast = TR::Block::createEmptyBlock(comp());
   cfg()->addNode(Bfast);
   cfg()->addEdge(Bprev, Bfast);
   cfg()->addEdge(Bfast, Btail);
   cfg()->removeEdge(B, Btail);   // replaced
   Bprev->getExit()->join(Bfast->getEntry());
   Bfast->getExit()->join(Btail->getEntry());
   Bfast->setIsExtensionOfPreviousBlock();

   //comp()->dumpMethodTrees("after createMultiDiamond");
   }

// Not currently used. 
void
Ruby::IlFastpather::fastpathGE(TR::TreeTop *tt, TR::Node *origif)
   {
#if 0 
   auto* block = tt->getEnclosingBlock();

   if (!performTransformation(comp(), "%s Fastpathing %s on TT %p\n", OPT_DETAILS, "GE", tt))
      return; 

   auto call = origif->getFirstChild()->getFirstChild();
   auto a    = call->getChild(2);
   auto b    = call->getChild(3);
   TR::ILOpCodes compareOp = getCompareOp(origif);

   if (compareOp == TR::BadILOp) return; // unsupported

   //comp()->dumpMethodTrees("before fast path of conditional");

   TR::Block *Btarget = origif->getBranchDestination()->getNode()->getBlock();
   TR::Block *Bfallth = block->getNextBlock();

   // We need to anchor a and b before the if tree so that we can use
   // commoned references to a and b in extension blocks
   //
   TR::Node::anchorBefore(a, tt);
   TR::Node::anchorBefore(b, tt);

   // Now we split the block immediately before the origif tree
   TR::Block *Btail = block->split(tt, cfg(), true /*fixupCommoning*/);
   Btail->setIsCold(); // slow path
                                              /* HACK!!  */
   auto ifRedefined = genRedefinedTest(BOP_GE /* for now */, FIXNUM_REDEFINED_OP_FLAG,
                                       Btail->getEntry());
   TR::Node::genTreeTop(ifRedefined, block);
   // no need to add an edge from block->Btail as it already exists

   // first intermediate block
   TR::Block *B1 = TR::Block::createEmptyBlock(comp());
   cfg()->addNode(B1);
   cfg()->addEdge(block, B1);
   cfg()->addEdge(B1, Btail);
   block->getExit()->join(B1->getEntry());
   B1->setIsExtensionOfPreviousBlock();

   auto ifAFixnum = genFixNumTest(a);
   TR::Node::genTreeTop(ifAFixnum, B1);
   ifAFixnum->setBranchDestination(Btail->getEntry());

   // second intermediate block
   TR::Block *B2 = TR::Block::createEmptyBlock(comp());
   cfg()->addNode(B2);
   cfg()->addEdge(B1, B2);
   cfg()->addEdge(B2, Btail);
   B1->getExit()->join(B2->getEntry());
   B2->setIsExtensionOfPreviousBlock();

   auto ifBFixnum = genFixNumTest(b);
   TR::Node::genTreeTop(ifBFixnum, B2);
   ifBFixnum->setBranchDestination(Btail->getEntry());

   // fast path
   TR::Block *Bfast = TR::Block::createEmptyBlock(comp());
   cfg()->addNode(Bfast);
   cfg()->addEdge(B2, Bfast);
   cfg()->addEdge(Bfast, Btarget);
   B2->getExit()->join(Bfast->getEntry());
   Bfast->setIsExtensionOfPreviousBlock();

   auto newif = TR::Node::createif(compareOp,
                                    a, b,
                                    Btarget->getEntry());
   TR::Node::genTreeTop(newif, Bfast);

   // Goto block to the fall through
   TR::Block *Bgoto = TR::Block::createEmptyBlock(comp());
   cfg()->addNode(Bgoto);
   cfg()->addEdge(Bfast, Bgoto);
   cfg()->addEdge(Bgoto, Bfallth);
   Bfast->getExit()->join(Bgoto->getEntry());
   Bgoto->getExit()->join(Btail->getEntry());

   auto gotoNode = TR::Node::create(TR::Goto, 0);
   TR::Node::genTreeTop(gotoNode, Bgoto);
   gotoNode->setBranchDestination(Bfallth->getEntry());

   return; 
#else 
   TR_ASSERT_FATAL(false, "Fastpathing disabled for 2.2-2.4 changes"); 
   return;
#endif
   }

/**
 * Generate branch that checks flags
 *
 * if (UNLIKELY(ruby_vm_event_flags & (flag_))) 
 *
 * @TODO: Need to ensure the datatype lenth is correct 
 *        Currently, assuming TR::Int64, which is the slot size. 
 *        However, if the slot size != sizeof(unsigned long), this code 
 *        won't be correct.
 */
TR::Node * 
Ruby::IlFastpather::genTraceTest(TR::Node * flag) 
   {
   auto flagSymref   = comp()->getSymRefTab()->findRubyVMEventFlagsSymbolRef();
   auto loadNode     = TR::Node::createLoad(flagSymref); 
   return TR::Node::createif(TR::ificmpne,
            TR::Node::create(TR::iand, 2, loadNode, flag),
            TR::Node::iconst(0));
   }

TR::Node *
Ruby::IlFastpather::genFixNumTest(TR::Node *object)
   {
   return
      TR::Node::ifxcmpeq(
         TR::Node::xand(
            object,
            TR::Node::xconst(1)),
         TR::Node::xconst(0));
   }


TR::Node *
Ruby::IlFastpather::genRedefinedTest(int32_t op, int32_t mask, TR::TreeTop *dest)
   {
   return
      TR::Node::createif(TR::ifscmpne,
                          loadBOPRedefined(op),
                          TR::Node::sconst(0), // FIXME: check against mask
                          dest);
   }

//MG: need access to redefined symrefs, with aliasing correclty set. 
//    Hmm. 
TR::Node *
Ruby::IlFastpather::loadBOPRedefined(int32_t bop)
   {
   //These functions need to be in-sync with respect to the BOP's supported:
   // Ruby::IlFastpather::IlFastpather
   // Ruby::IlFastpather::loadBOPRedefined (this)
   // getBOPName
   // Ensure all three are updated when adding BOPs.

   switch (bop)
      {
      case BOP_MINUS:
      case BOP_PLUS:
      case BOP_GE:
         return TR::Node::createLoad(comp()->getSymRefTab()->findRubyRedefinedFlagSymbolRef(bop));
      default:
         TR_ASSERT(0, "we only support BOP_MINUS,PLUS,GE at the moment");
      }
   return NULL;
   }


TR::ILOpCodes
Ruby::IlFastpather::getCompareOp(TR::Node *origif)
   {
   auto call = origif->getFirstChild()->getFirstChild();
   auto sr = call->getSymbolReference()->getReferenceNumber();

   // ifxcmpeq means that it was was a branchunless
   bool reverseBranch = (origif->getOpCode().isCompareTrueIfEqual()); 

   TR::ILOpCodes op = TR::BadILOp;
   if (sr == RubyHelper_vm_opt_eq)  op = TR::iflcmpeq;
   if (sr == RubyHelper_vm_opt_ge)  op = TR::iflcmpge;
   // if (sr == RubyHelper_vm_opt_gt)  op = TR::iflcmpgt;
   // if (sr == RubyHelper_vm_opt_le)  op = TR::iflcmple;
   // if (sr == RubyHelper_vm_opt_neq) op = TR::iflcmpne;
   // if (sr == RubyHelper_vm_opt_lt)  op = TR::iflcmplt;
   if (reverseBranch) op = TR::ILOpCode(op).getOpCodeForReverseBranch();
   return op;

   }

