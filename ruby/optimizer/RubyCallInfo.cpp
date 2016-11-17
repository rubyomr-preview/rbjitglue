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


#include "ruby/optimizer/RubyCallInfo.hpp"

#include "compile/SymbolReferenceTable.hpp"
#include "il/SymbolReference.hpp"
#include "il/Node_inlines.hpp"
#include "optimizer/Inliner.hpp"
#include "ruby/env/RubyMethod.hpp"

#ifdef RUBY_PROJECT_SPECIFIC
#include "ruby/config.h" 
#endif

TR_CallSite* TR_CallSite::create(TR::TreeTop* tt,
      TR::Node *parent,
      TR::Node *node,
      TR_OpaqueClassBlock *receiverClass, TR::SymbolReference *symRef,
      TR_ResolvedMethod *resolvedMethod,
      TR::Compilation* comp,
      TR_Memory* trMemory,
      TR_AllocationKind kind,
      TR_ResolvedMethod* caller,
      int32_t depth,
      bool allConsts)
   {
   TR_ASSERT(false, "Inlining for 2.3 is intentionally disabled." ); 
   return NULL; 
   /* 
   TR_ResolvedMethod* lCaller = caller ? caller : symRef->getOwningMethod(comp);
   if(node->getSymbol()->castToMethodSymbol()->isHelper())
      {
      if (node->getSymbolReference() == comp->getSymRefTab()->getSymRef(RubyHelper_vm_send))
         {
         return new (trMemory, kind) TR_Ruby_Send_CallSite(lCaller,
               tt,
               parent,
               node,
               NULL,
               receiverClass,
               (int32_t)symRef->getOffset(),
               symRef->getCPIndex(),
               resolvedMethod,
               NULL,
               node->getOpCode().isCallIndirect(),
               false,
               node->getByteCodeInfo(),
               comp);
         }
      else if (node->getSymbolReference() == comp->getSymRefTab()->getSymRef(RubyHelper_vm_invokeblock))
         {
         return new (trMemory, kind) TR_Ruby_InvokeBlock_CallSite(lCaller,
               tt,
               parent,
               node,
               NULL,
               receiverClass,
               (int32_t)symRef->getOffset(),
               symRef->getCPIndex(),
               resolvedMethod,
               NULL,
               node->getOpCode().isCallIndirect(),
               false,
               node->getByteCodeInfo(),
               comp);
         }
      else if (node->getSymbolReference() == comp->getSymRefTab()->getSymRef(RubyHelper_vm_send_without_block))
         {
         TR_CallSite* callSite = new (trMemory, kind) TR_Ruby_SendSimple_CallSite(lCaller,
               tt,
               parent,
               node,
               NULL,
               receiverClass,
               (int32_t)symRef->getOffset(),
               symRef->getCPIndex(),
               resolvedMethod,
               NULL,
               node->getOpCode().isCallIndirect(),
               false,
               node->getByteCodeInfo(),
               comp);

         //Store the receiver in a temporary to be used by TR_InlinerBase::calleeTreeTopPreMergeActions.
         //VirtualGuard splits the block up so we cannot directly reference the receiver from the callNode
         //without BlockVerfier triggering an assertion on it.
         //DeadStoreElimination should get rid of this temp if nobody uses it.
         TR::Node *receiver = node->getThirdChild();
         TR::SymbolReference *receiverTemp = TR::Node::storeToTemp(receiver, tt);
         comp->getSymRefTab()->setRubyInlinedReceiverTempSymRef(callSite, receiverTemp);

         return callSite;
         }
      }

   //For everything else.
   return new (trMemory, kind) TR_RubyCallSite(lCaller,
         tt,
         parent,
         node,
         NULL,
         receiverClass,
         (int32_t)symRef->getOffset(),
         symRef->getCPIndex(),
         resolvedMethod,
         NULL,
         node->getOpCode().isCallIndirect(),
         false,
         node->getByteCodeInfo(),
         comp);
   
         */
   }

bool
TR_RubyCallSite::findCallSiteTarget (TR_CallStack* callStack, TR_InlinerBase* inliner)
   {
   //No targets to add, don't know which type of call this is.
   return false;
   }

bool
TR_Ruby_Send_CallSite::findCallSiteTarget (TR_CallStack* callStack, TR_InlinerBase* inliner)
   {
   return false;
   }

bool
TR_Ruby_InvokeBlock_CallSite::findCallSiteTarget (TR_CallStack* callStack, TR_InlinerBase* inliner)
   {
   return false;
   }

// bool
// TR_Ruby_SendSimple_CallSite::findCallSiteTarget (TR_CallStack* callStack, TR_InlinerBase* inliner)
//    {
//    //Landing here implies TR_RubyFE::checkInlineableWithoutInitialCalleeSymbol did all the checks and
//    //decided this callSite was a valid inlineable target.
// 
//    TR::Node* node = _callNode;
// 
//    //Ensure that ILGen hasn't changed the children's layout we expect.
//    TR_ASSERT(  ((node->getNumChildren() == 3) &&
//          node->getSecondChild() &&
//          node->getSecondChild()->getOpCodeValue() == TR::aconst), "Unexpected children in hierarchy of vm_send_without_block when creating callsite target.");
// 
//    rb_call_info_t *ci = (rb_call_info_t *) node->getSecondChild()->getAddress();
// #ifdef OMR_JIT_PROFILING
//    VALUE klass = ci->profiled_klass;
// #else
//    VALUE klass = Qundef; 
// #endif
//    VALUE actual_klass;
//    rb_method_entry_t *me = (rb_method_entry_t*)TR_RubyFE::instance()->getJitInterface()->callbacks.rb_method_entry_f(klass, ci->mid, &actual_klass);
//    rb_iseq_t *iseq_callee = me->def->body.iseq;
// 
//    int32_t len =
//          RSTRING_LEN(iseq_callee->location.path) +
//          (sizeof(size_t) * 3) +                // first_lineno: estimate three decimal digits per byte
//          RSTRING_LEN(iseq_callee->location.label) +
//          3;                                    // two colons and a null terminator
// 
//    char *name = (char*) comp()->trMemory()->allocateHeapMemory(len);
//    sprintf(name, "%s:%lld:%s",
//          (char*) RSTRING_PTR(iseq_callee->location.path),
//          FIX2LONG(iseq_callee->location.first_lineno),
//          (char*)RSTRING_PTR(iseq_callee->location.label));
// 
//    RubyMethodBlock* callee_mb = new (comp()->trPersistentMemory()) RubyMethodBlock(iseq_callee, name);
//    ResolvedRubyMethod* callee_method = new (comp()->trPersistentMemory()) ResolvedRubyMethod(*callee_mb);
//    TR::ResolvedMethodSymbol *calleeSymbol = TR::ResolvedMethodSymbol::createJittedMethodSymbol(comp()->trHeapMemory(), callee_method, comp());
// 
//    //Set ResolvedMethod and ResolvedMethodSymbol.
//    _initialCalleeMethod = callee_method;
//    _initialCalleeSymbol = calleeSymbol;
// 
//    //Add a target for inliner to process.
//    TR_VirtualGuardSelection *guard = new (comp()->trHeapMemory()) TR_VirtualGuardSelection(TR_ProfiledGuard, TR_RubyInlineTest, (TR_OpaqueClassBlock *) klass);
// 
//    //Send out a callTarget
//    addTarget(comp()->trMemory(),
//          inliner,
//          guard,
//          _initialCalleeMethod,
//          _initialCalleeMethod->classOfMethod(),
//          heapAlloc);
//    return true;
//    }

