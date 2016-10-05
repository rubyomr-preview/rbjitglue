/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/


#ifndef RUBY_DUMBINLINER_INCL
#define RUBY_DUMBINLINER_INCL

#include "optimizer/Inliner.hpp"

namespace Ruby
{
class TrivialInliner : public TR::Optimization
   {
   public:
      TrivialInliner(TR::OptimizationManager *manager);
      static TR::Optimization *create(TR::OptimizationManager *manager)
         {
         return new (manager->allocator()) TrivialInliner(manager);
         }

      virtual int32_t perform();

      virtual bool         shouldPerform() { 
         static auto * enableInliner = feGetEnv("RUBY_ENABLE_INLINER"); 
         return enableInliner; 
      } 


   };
}

#endif
