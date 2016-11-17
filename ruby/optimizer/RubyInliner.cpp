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


#include <algorithm>
#include "env/VMHeaders.hpp" 
#include "env/FEBase.hpp"
#include "il/Block.hpp"
#include "il/ILOpCodes.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/symbol/ParameterSymbol.hpp"
#include "ilgen/IlGenRequest.hpp"
#include "ilgen/IlGeneratorMethodDetails_inlines.hpp"
#include "optimizer/Inliner.hpp"
#include "optimizer/RubyInliner.hpp"
#include "ruby/env/RubyFE.hpp"
#include "ruby/env/RubyMethod.hpp"
#include "ruby/optimizer/RubyCallInfo.hpp"
#include "ruby/ilgen/IlGeneratorMethodDetails.hpp"
#include "ras/DebugCounter.hpp"


#define OPT_DETAILS "O^O DUMB INLINER CALLSITE: "

//Quick helper function.
static TR::TreeTop*
genCall(TR::Compilation* comp, TR_RuntimeHelper helper, TR::ILOpCodes opcode, int32_t num, ...)
   {
   va_list args;
   va_start(args, num);
   TR::Node *callNode = TR::Node::create(opcode, num);
   for (int i = 0; i < num; i++)
      {
      callNode->setAndIncChild(i, va_arg(args, TR::Node *));
      }
   va_end(args);

   TR::SymbolReference * helperSymRef = comp->getSymRefTab()->findOrCreateRuntimeHelper(helper, true, true, false);
   helperSymRef->setEmptyUseDefAliases(comp->getSymRefTab());
   callNode->setSymbolReference(helperSymRef);

   TR::TreeTop* returnTT = TR::TreeTop::create(comp, TR::Node::create(TR::treetop, 1, callNode));
   return returnTT;
   }

bool TR_InlinerBase::tryToGenerateILForMethod (TR::ResolvedMethodSymbol* calleeSymbol, TR::ResolvedMethodSymbol* callerSymbol, TR_CallTarget* calltarget)
   {
   bool ilGenSuccess = false;
   TR::IlGeneratorMethodDetails callee_ilgen_details(calleeSymbol->getResolvedMethod());
   TR::CompileIlGenRequest request(callee_ilgen_details);
   ilGenSuccess  = calleeSymbol->genIL(fe(), comp(), comp()->getSymRefTab(), request);
   return ilGenSuccess;
   }

bool TR_InlinerBase::inlineCallTarget(TR_CallStack *callStack, TR_CallTarget *calltarget, bool inlinefromgraph, TR_PrexArgInfo *argInfo, TR::TreeTop** cursorTreeTop)
   {
   TR_InlinerDelimiter delimiter(tracer(),"TR_InlinerBase::inlineCallTarget");

   bool successful = inlineCallTarget2(callStack, calltarget, cursorTreeTop, inlinefromgraph, 99);
#if 0
   // if inlining fails, we need to tell decInlineDepth to remove elements that
   // we added during inlineCallTarget2
   comp()->decInlineDepth(!successful);
#endif
   return successful;
   }

void TR_InlinerBase::getBorderFrequencies(int32_t &hotBorderFrequency, int32_t &coldBorderFrequency, TR_ResolvedMethod * calleeResolvedMethod, TR::Node *callNode)
   {
   hotBorderFrequency = 2500;
   coldBorderFrequency = 0;
   return;
   }

int32_t TR_InlinerBase::scaleSizeBasedOnBlockFrequency(int32_t bytecodeSize, int32_t frequency, int32_t borderFrequency, TR_ResolvedMethod * calleeResolvedMethod, TR::Node *callNode, int32_t coldBorderFrequency)
   {
   int32_t maxFrequency = MAX_BLOCK_COUNT + MAX_COLD_BLOCK_COUNT;
   bytecodeSize = (int)((float)bytecodeSize * (float)(maxFrequency-borderFrequency)/(float)maxFrequency);
              if (bytecodeSize < 10) bytecodeSize = 10;

   return bytecodeSize;

   }

int
TR_InlinerBase::checkInlineableWithoutInitialCalleeSymbol (TR_CallSite* callSite, TR::Compilation* comp)
   {
   return Ruby_unsupported_calltype;
   // Needs modification for 2.3 
//   TR::Node* node = callSite->_callNode;
//
//   if(node->getSymbolReference() != comp->getSymRefTab()->getSymRef(RubyHelper_vm_send_without_block))
//       return Ruby_unsupported_calltype;
//
//   //Ensure that ILGen hasn't changed the children's layout we expect.
//   TR_ASSERT(  ((node->getNumChildren() == 3) &&
//        node->getSecondChild() &&
//        node->getSecondChild()->getOpCodeValue() == TR::aconst), "Unexpected children in hierarchy of vm_send_without_block when creating callsite target.");
// /*
//  * Preconditions:
//  *     true == (ci->me->def->type == VM_METHOD_TYPE_ISEQ)
//  *             If its not a proper Ruby method, all bets are off.
//  *     true == (iseq->arg_simple & 0x01)
//  *             Have simple args, otherwise in vm_callee_setup_arg we have to handle (ci->aux.opt_pc != 0)
//  *             And that requires multiple method entry.
//  *     false == (ci->flag & VM_CALL_TAILCALL)
//  *             Here we're handling the case of normal calls via vm_call_iseq_setup_normal.
//  *             For TailCalls we will need to work with vm_call_iseq_setup_tailcall.
//  *             The JIT currently doesn't compile methods of type VM_CALL_TAILCALL.
//  */
// 
//    rb_call_info_t *ci = (rb_call_info_t *) node->getSecondChild()->getAddress();
// 
// 
// #ifdef OMR_JIT_PROFILE
//    VALUE klass = ci->profiled_klass;
// #else
//    VALUE klass = Qundef;
// #endif
//    if(klass == Qnil || klass == Qundef)
//      {
//      if(klass == Qnil)
//         TR::DebugCounter::incStaticDebugCounter(comp, TR::DebugCounter::debugCounterName(comp, "ruby.callSites/send_without_block/notInlineable/ambiguious_profiled_klass/Qnil"));
//      else
//         TR::DebugCounter::incStaticDebugCounter(comp, TR::DebugCounter::debugCounterName(comp, "ruby.callSites/send_without_block/notInlineable/ambiguious_profiled_klass/Qundef"));
//      return Ruby_ambiguious_profiled_klass;
//      }
// 
// #ifdef OMR_RUBY_VALID_CLASS
//    if(!TR_RubyFE::instance()->getJitInterface()->callbacks.ruby_omr_is_valid_object_f(klass))
// #endif
//       {
//       TR::DebugCounter::incStaticDebugCounter(comp, TR::DebugCounter::debugCounterName(comp, "ruby.callSites/send_without_block/notInlineable/invalid_klass"));
//       return Ruby_invalid_klass;
//       }
// 
//    VALUE actual_klass;
//    rb_method_entry_t *me = (rb_method_entry_t*)TR_RubyFE::instance()->getJitInterface()->callbacks.rb_method_entry_f(klass, ci->mid, &actual_klass);
// 
//    if(!me)
//      {
//      TR::DebugCounter::incStaticDebugCounter(comp, TR::DebugCounter::debugCounterName(comp, "ruby.callSites/send_without_block/notInlineable/missing_method_entry"));
//      return Ruby_missing_method_entry;
//      }
// 
//    //Ruby dispatch favours me->flag values, 0x0(NOEX_PUBLIC) and 0x8(NOEX_BASIC)
//    //TODO: investigate the other values.
//    if(me->flag != 0 && me->flag != 0x8)
//      {
//      char flag[15];
//      snprintf(flag, 15, "0x%x", me->flag);
//      TR::DebugCounter::incStaticDebugCounter(comp, TR::DebugCounter::debugCounterName(comp, "ruby.callSites/send_without_block/notInlineable/unsupported_method_entry_flag/%s", flag));
//      return Ruby_unsupported_method_entry_flag;
//      }
// 
//    char methodName[64];
//    snprintf(methodName, 64, "%s", TR_RubyFE::instance()->getJitInterface()->callbacks.rb_id2name_f(ci->mid));
//    char klassName[64];
//    snprintf(klassName, 64, "%s", TR_RubyFE::instance()->getJitInterface()->callbacks.rb_class2name_f(klass));
// 
//    //If this isn't a proper Ruby method, don't inline.
//    switch(me->def->type)
//       {
//       case VM_METHOD_TYPE_ISEQ:
//          break;
//       case VM_METHOD_TYPE_CFUNC:
//          TR::DebugCounter::incStaticDebugCounter(comp, TR::DebugCounter::debugCounterName(comp, "ruby.callSites/send_without_block/notInlineable/inlining_cfunc/%s/%s", klassName,methodName));
//          return Ruby_inlining_cfunc;
//       default:
//          TR::DebugCounter::incStaticDebugCounter(comp, TR::DebugCounter::debugCounterName(comp, "ruby.callSites/send_without_block/notInlineable/inlining_non_iseq_method/%s/%s", klassName,methodName));
//          return Ruby_non_iseq_method;
//       }
// 
//    rb_iseq_t *iseq_callee = me->def->body.iseq;
// 
// 
//    //Check if the callee has optional arguments.
//    //We do not support inlining such callees yet.
//    bool check_arg_opts     = (iseq_callee->param.flags.has_opt    != 0);
//    bool check_arg_rest     = (iseq_callee->param.flags.has_rest   != 0);
//    bool check_arg_post_len = (iseq_callee->param.flags.has_post   != 0);
//    bool check_arg_block    = (iseq_callee->param.flags.has_block  != 0);
//    bool check_arg_keywords = (iseq_callee->param.flags.has_kw     != 0);
//    bool check_arg_kwrest   = (iseq_callee->param.flags.has_kwrest != 0);
// 
//    if ( check_arg_opts      ||
//        check_arg_rest      ||
//        check_arg_post_len  ||
//        check_arg_block     ||
//        check_arg_keywords ||
//        check_arg_kwrest
//        )
//      {
//      if (check_arg_kwrest)
//         TR::DebugCounter::incStaticDebugCounter(comp, TR::DebugCounter::debugCounterName(comp, "ruby.callSites/send_without_block/notInlineable/has_opt_args/arg_kwrest"));
//      if (check_arg_opts)
//         TR::DebugCounter::incStaticDebugCounter(comp, TR::DebugCounter::debugCounterName(comp, "ruby.callSites/send_without_block/notInlineable/has_opt_args/arg_opts"));
//      if (check_arg_rest)
//         TR::DebugCounter::incStaticDebugCounter(comp, TR::DebugCounter::debugCounterName(comp, "ruby.callSites/send_without_block/notInlineable/has_opt_args/arg_rest"));
//      if (check_arg_post_len)
//         TR::DebugCounter::incStaticDebugCounter(comp, TR::DebugCounter::debugCounterName(comp, "ruby.callSites/send_without_block/notInlineable/has_opt_args/arg_post_len"));
//      if (check_arg_block)
//         TR::DebugCounter::incStaticDebugCounter(comp, TR::DebugCounter::debugCounterName(comp, "ruby.callSites/send_without_block/notInlineable/has_opt_args/arg_block"));
//      if (check_arg_keywords)
//         TR::DebugCounter::incStaticDebugCounter(comp, TR::DebugCounter::debugCounterName(comp, "ruby.callSites/send_without_block/notInlineable/has_opt_args/arg_keywords"));
// 
//      return Ruby_has_opt_args;
//      }
// 
//    if(iseq_callee->catch_table != 0)
//      {
//      TR::DebugCounter::incStaticDebugCounter(comp, TR::DebugCounter::debugCounterName(comp, "ruby.callSites/send_without_block/notInlineable/non_zero_catchtable"));
//      return Ruby_non_zero_catchtable;
//      }
// 
//    //Ok everything looks good.
//    TR::DebugCounter::incStaticDebugCounter(comp, TR::DebugCounter::debugCounterName(comp, "ruby.callSites/send_without_block/inlineable"));
//   return InlineableTarget;
   }

Ruby::InlinerUtil::InlinerUtil(TR::Compilation *comp)
   : OMR_InlinerUtil(comp)
   {}
   
void
Ruby::InlinerUtil::calleeTreeTopPreMergeActions(TR::ResolvedMethodSymbol *calleeResolvedMethodSymbol, TR_CallSite* callSite)
  {
//   TR::Node* send_without_block_call_Node = callSite->_callNode;
//
//  //Ensure that ILGen hasn't changed the children's layout we expect.
//  TR_ASSERT(  ((send_without_block_call_Node->getNumChildren() == 3) &&
//       send_without_block_call_Node->getSecondChild() &&
//       send_without_block_call_Node->getSecondChild()->getOpCodeValue() == TR::aconst), "Unexpected children in hierarchy of vm_send_without_block.");
//
//  rb_call_info_t *ci = (rb_call_info_t *) send_without_block_call_Node->getSecondChild()->getAddress();
//
//   TR::TreeTop * startOfInlinedCall = calleeResolvedMethodSymbol->getFirstTreeTop()->getNextTreeTop();
//
//   //Need Ruby Thread:
//   TR::ParameterSymbol *parmSymbol = comp()->getMethodSymbol()->getParameterList().getListHead()->getData();
//   TR::SymbolReference *threadSymRef = comp()->getSymRefTab()->findOrCreateAutoSymbol(comp()->getMethodSymbol(), parmSymbol->getSlot(), parmSymbol->getDataType(), true, false, true, false);
//
//   TR::ILOpCodes callOpCode = TR_RubyFE::SLOTSIZE == 8 ? TR::lcall : TR::icall;
//
//   TR::SymbolReference* receiverTempSymRef = comp()->getSymRefTab()->getRubyInlinedReceiverTempSymRef(callSite);
//   TR_ASSERT(receiverTempSymRef != NULL, "NULL receiverTempSymRef, findCallSiteTarget didn't store receiver to a temporary.");
//
//   TR::TreeTop* vm_frame_setup_call_TT = genCall(comp(), RubyHelper_vm_send_woblock_jit_inline_frame, callOpCode, 3,
//                                                   TR::Node::createLoad(threadSymRef),
//                                                   TR::Node::aconst((uintptr_t)ci),
//                                                   TR::Node::createLoad(receiverTempSymRef)
//                                                   );
//   startOfInlinedCall->insertBefore(vm_frame_setup_call_TT);
  }



