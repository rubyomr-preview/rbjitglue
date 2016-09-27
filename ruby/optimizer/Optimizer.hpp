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


#ifndef TR_OPTIMIZER_INCL
#define TR_OPTIMIZER_INCL

#include "optimizer/RubyOptimizer.hpp"

namespace TR
{

class Optimizer : public ::Ruby::OptimizerConnector
   {
   public:

   Optimizer(TR::Compilation *comp, TR::ResolvedMethodSymbol *methodSymbol, bool isIlGen,
         const OptimizationStrategy *strategy = NULL, uint16_t VNType = 0) :
      ::Ruby::OptimizerConnector(comp, methodSymbol, isIlGen, strategy, VNType) {}
   };

}

#endif
