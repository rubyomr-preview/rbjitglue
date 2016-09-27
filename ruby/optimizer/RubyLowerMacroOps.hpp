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


#ifndef RUBYLOWERMACROOPS_INCL
#define RUBYLOWERMACROOPS_INCL

#include "optimizer/Optimization.hpp"


namespace Ruby
{

/**
 * Lower Ruby macro ops. 
 */
class LowerMacroOps : public TR::Optimization
   {
   public:

   LowerMacroOps(TR::OptimizationManager *manager);

   /**
    * Optimization factory method
    */
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) LowerMacroOps(manager); 
      }

   TR::CFG * cfg() { return comp()->getFlowGraph(); } 

   virtual int32_t      perform(); 

   /**
    * This opt must happen at least once before codegen, but once it has
    * happened once, it needn't happen again.
    *
    * This opt is therefore set-requested at construction in Ruby::Optimizer,
    * but LowerMacroOps::perform will unset this once it has lowered. 
    *
    */
   virtual bool shouldPerform() { return manager()->requested(); } 

   private: 

   void         lowerTreeTop(TR::TreeTop *); 
   void         lowerAsyncCheck(TR::Node *, TR::TreeTop *);
   TR::Node*    pendingInterruptsNode(); 

   };

}


#endif
