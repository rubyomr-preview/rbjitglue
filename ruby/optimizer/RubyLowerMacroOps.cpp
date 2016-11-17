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


#include "optimizer/RubyLowerMacroOps.hpp"

#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Block.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "optimizer/Optimization_inlines.hpp"

#define OPT_DETAILS "O^O RUBYLOWERMACROOPS: "

Ruby::LowerMacroOps::LowerMacroOps(TR::OptimizationManager *manager)
   : TR::Optimization(manager) 
   {}


/** 
 * Extract some of the semantics of ruby calls into trees, providing a
 * fast-path version. 
 *
 * Executes on a treetop basis. 
 * 
 */ 
int32_t
Ruby::LowerMacroOps::perform() 
   {
   if (trace())
      traceMsg(comp(), OPT_DETAILS "Processing method: %s\n", comp()->signature());

   auto lastTT = cfg()->findLastTreeTop();
   for (auto tt = comp()->getMethodSymbol()->getFirstTreeTop();
        tt != lastTT;
        tt = tt->getNextTreeTop())
      {
      lowerTreeTop(tt); 
      }

   //Disable myself for future calls: 
   manager()->setRequested(false); 
   return 0;
   } 

/**
 * Transform a tree top. 
 */
void
Ruby::LowerMacroOps::lowerTreeTop(TR::TreeTop* tt) 
   {
   auto *node = tt->getNode(); 
   if (node->getOpCodeValue() == TR::treetop) 
      node = node->getFirstChild(); 
   
   switch (node->getOpCodeValue()) 
      {
      case TR::asynccheck: 
         lowerAsyncCheck(node, tt); 
      default: 
         break; 
      }
            
   }


TR::Node *
Ruby::LowerMacroOps::pendingInterruptsNode() 
   {
   static auto * irritate_asynccheck = feGetEnv("TR_RUBY_IRRITATE_ASYNCHECK"); 
   if (irritate_asynccheck == NULL) 
      {
      TR::Node *interrupt_flag =
         TR::Node::xloadi(TR::comp()->getSymRefTab()->findOrCreateRubyInterruptFlagSymRef(),
                          TR::Node::loadThread(comp()->getMethodSymbol()),
                          fe());

      TR::Node *not_interrupt_mask =
         TR::Node::xnot(
                        TR::Node::xloadi(TR::comp()->getSymRefTab()->findOrCreateRubyInterruptMaskSymRef(),
                                         TR::Node::loadThread(comp()->getMethodSymbol()),
                                        fe())
                       );

      return TR::Node::xand(
                            interrupt_flag,
                            not_interrupt_mask);
      }
   else
      {
      // asynccheck irritator: This path can be enabled to make the
      // asyncchek behave deterministically in case that helps in isolating
      // a bug.
      return TR::Node::xconst(1);
      }
   }


/**
 * Lower an asynccheck call into a branch and call. 
 *
 * \note We do this here rather than in the lowerTrees call from
 * CodeGenPrep.cpp because we would like this done before we run tactical GRA,
 * as block splitting after tacticalGRA is difficult. There is precedent in 
 * doing so, but it involves a bit of non-local analysis.
 *
 */
void 
Ruby::LowerMacroOps::lowerAsyncCheck(TR::Node *asynccheckNode, TR::TreeTop *asynccheckTree)
   {
   if (!performTransformation(comp(), "%s Lowering asynccheck (%p)\n", OPT_DETAILS, asynccheckNode))
      {
      TR_ASSERT(false, "Disabled asynccheck lowering. This will break in code generation"); 
      return; 
      }

   TR::Compilation *comp = TR::comp();
   TR::CFG *cfg = comp->getFlowGraph();

   cfg->setStructure(0);

   TR::Block *asynccheckBlock = asynccheckTree->getEnclosingBlock();
   TR::Block *remainderBlock = asynccheckBlock->split(asynccheckTree->getNextTreeTop(), cfg, true);

   // Create a call to the asynccheck handler
   TR::SymbolReference *callSymRef = asynccheckNode->getSymbolReference();

   auto threadLoadNode              = TR::Node::loadThread(comp->getMethodSymbol());
   auto callNode                    = TR::Node::create(TR::call, 2, threadLoadNode, TR::Node::iconst(0));
   callNode->setSymbolReference(callSymRef);
   auto callTree                    = TR::TreeTop::create(comp, TR::Node::create(TR::treetop, 1, callNode));

   TR::Node       *valueNode   =   pendingInterruptsNode();
   TR::DataType  valueType   =   valueNode->getDataType();
   TR::Node       *constNode   =   TR::Node::create(asynccheckNode, TR::ILOpCode::constOpCode(valueType), 0);
   TR::ILOpCodes   op          =   TR::ILOpCode::ifcmpneOpCode(valueType);
   TR::Node       *ifNode      =   TR::Node::createif(op, valueNode, constNode);
   TR::TreeTop    *ifTree      =   TR::TreeTop::create(comp, ifNode);

   asynccheckBlock->append(ifTree);
   asynccheckNode->removeAllChildren();
   asynccheckTree->getPrevTreeTop()->join(asynccheckTree->getNextTreeTop()); // remove the ac tree

   TR::Block *callBlock = TR::Block::createEmptyBlock(comp, 0);
   cfg->addNode(callBlock);
   cfg->findLastTreeTop()->join(callBlock->getEntry());
   callBlock->append(callTree);

   TR::Node *gotoNode = TR::Node::create(TR::Goto, 0, remainderBlock->getEntry());
   TR::TreeTop *gotoTree = TR::TreeTop::create(comp, gotoNode);
   callBlock->append(gotoTree);

   cfg->addEdge(asynccheckBlock, callBlock);
   cfg->addEdge(callBlock, remainderBlock);

   ifNode->setBranchDestination(callBlock->getEntry());
   
   return;
   }

