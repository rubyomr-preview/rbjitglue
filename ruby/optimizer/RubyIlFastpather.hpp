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


#ifndef RUBYILFASTPATHER_INCL
#define RUBYILFASTPATHER_INCL

#include "optimizer/Optimization.hpp"

#include "ruby.h" //VALUE
#include "vm_core.h" // For BOP_MINUS and FIXNUM_REDEFINED_OP_FLAG etc.
#include "vm_insnhelper.h" // For BOP_MINUS and FIXNUM_REDEFINED_OP_FLAG etc.


namespace Ruby
{
/**
 * Optimization class that modifies IL to fastpath operations from
 * the ruby VM implementation. 
 *
 * This optimization must be kept up to date with the RubyVM. 
 */
class IlFastpather : public TR::Optimization
   {
   public:

   IlFastpather(TR::OptimizationManager * manager); 

   /**
    * Optimization factory method
    */
   static TR::Optimization *create(TR::OptimizationManager *manager) 
      {
      return new (manager->allocator()) IlFastpather(manager); 
      }

   TR::CFG * cfg() { return comp()->getFlowGraph(); } 

   virtual int32_t      perform(); 
   virtual bool         shouldPerform() { 
      static auto * disableFastPath= feGetEnv("OMR_DISABLE_FASTPATH"); 
      return !disableFastPath; 
   } 

   private: 

   void performOnTreeTop(TR::TreeTop *); 
   void fastpathTrace   (TR::TreeTop *, TR::Node *); 

   //Fast Pathing
   TR::Node *genFixNumTest(TR::Node *);
   TR::Node *genRedefinedTest(int32_t, int32_t, TR::TreeTop *);
   TR::Node *genTraceTest(TR::Node * flag);

   TR::TreeTop * genTreeTop(TR::Node*, TR::Block*); 

   void fastpathPlusMinus(TR::TreeTop *, TR::Node *,  bool);
   void fastpathGE       (TR::TreeTop *, TR::Node *);

   // There is also an implementation of this function inside the pythonFE
   // that uses STL containers. We ought to common with that version. 
   void createMultiDiamond(TR::TreeTop *, TR::Block *, uint32_t,
                           TR::Block *&, TR::Block *&, TR::Block *&,
                           CS2::ArrayOf<TR::Block *, TR::Allocator> &);

   TR::ILOpCodes getCompareOp(TR::Node *origif);
   TR::Node     *loadBOPRedefined(int32_t bop);


   TR::SymbolReference * storeToTempBefore(TR::Node *, TR::TreeTop *);

   };

}


#endif
