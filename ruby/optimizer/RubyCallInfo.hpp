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


#ifndef RUBY_CALLINFO_INCL
#define RUBY_CALLINFO_INCL

#include "optimizer/CallInfo.hpp"

class TR_RubyCallSite : public  TR_CallSite
   {
   public:
      TR_CALLSITE_INHERIT_CONSTRUCTOR_AND_TR_ALLOC(TR_RubyCallSite, TR_CallSite)
      virtual bool findCallSiteTarget (TR_CallStack *callStack, TR_InlinerBase* inliner);
		virtual const char*  name () { return "TR_RubyCallSite"; }
   };

class TR_Ruby_Send_CallSite : public  TR_RubyCallSite
   {
   public:
      TR_CALLSITE_INHERIT_CONSTRUCTOR_AND_TR_ALLOC(TR_Ruby_Send_CallSite, TR_RubyCallSite)
      virtual bool findCallSiteTarget (TR_CallStack *callStack, TR_InlinerBase* inliner);
		virtual const char*  name () { return "TR_Ruby_VM_Send_CallSite"; }
   };

class TR_Ruby_SendSimple_CallSite : public  TR_RubyCallSite
   {
   public:
      TR_CALLSITE_INHERIT_CONSTRUCTOR_AND_TR_ALLOC(TR_Ruby_SendSimple_CallSite, TR_RubyCallSite)
      virtual bool findCallSiteTarget (TR_CallStack *callStack, TR_InlinerBase* inliner);
		virtual const char*  name () { return "TR_Ruby_VM_SendSimple_CallSite"; }
   };

class TR_Ruby_InvokeBlock_CallSite : public  TR_RubyCallSite
   {
   public:
      TR_CALLSITE_INHERIT_CONSTRUCTOR_AND_TR_ALLOC(TR_Ruby_InvokeBlock_CallSite, TR_RubyCallSite)
      virtual bool findCallSiteTarget (TR_CallStack *callStack, TR_InlinerBase* inliner);
		virtual const char*  name () { return "TR_Ruby_VM_InvokeBlock_CallSite"; }
   };

#endif /*RUBY_CALLINFO_INCL*/
