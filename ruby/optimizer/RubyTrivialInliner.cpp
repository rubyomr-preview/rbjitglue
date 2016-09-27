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


#include "ruby/optimizer/RubyTrivialInliner.hpp"

Ruby::TrivialInliner::TrivialInliner(TR::OptimizationManager *manager)
   : TR::Optimization(manager)
   {}

int32_t Ruby::TrivialInliner::perform()
   {
   //defaultInitialSize must be consistent with
   //TRIVIAL_INLINER_MAX_SIZE under ibm/control/Options.hpp
   const uint32_t defaultInitialSize = 25;

   uint32_t initialSize = comp()->getOptions()->getTrivialInlinerMaxSize();

   //Check if we're overriding the defaultInitialSize via an option.
   //If not increase initialSize as defaultInitialSize is too small.
   if(initialSize == defaultInitialSize)
      {
      //TODO: Get statistics on a good threshold for inlining Ruby methods.
      initialSize = 400;
      }

   comp()->generateAccurateNodeCount();

   TR::ResolvedMethodSymbol * sym = comp()->getMethodSymbol();
   if (sym->mayHaveInlineableCall() && !comp()->isDisabled(OMR::inlining))
      {
      TR_DumbInliner inliner(optimizer(), this, initialSize);
      inliner.performInlining(sym);
      }

   comp()->setSupressEarlyInlining(false);

   return 1; // cost??
   }
