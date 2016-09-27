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


#include "optimizer/OptimizationManager.hpp"
#include "optimizer/OptimizationManager_inlines.hpp"        // for OptimizationManager::self, etc

#include "env/CompilerEnv.hpp"
#include "codegen/CodeGenerator.hpp"
#include "il/Block.hpp"
#include "il/symbol/MethodSymbol.hpp"


Ruby::OptimizationManager::OptimizationManager(TR::Optimizer *o, OptimizationFactory factory, OMR::Optimizations optNum, const char *optDetailString, const OptimizationStrategy *groupOfOpts)
      : OMR::OptimizationManager(o, factory, optNum, optDetailString, groupOfOpts)
   {
   // set flags if necessary
   switch (self()->id())
      {
      case OMR::tacticalGlobalRegisterAllocator:
         _flags.set(requiresStructure);
         if (self()->comp()->getMethodHotness() >= hot && (TR::Compiler->target.is64Bit()))
            _flags.set(requiresLocalsUseDefInfo | doesNotRequireLoadsAsDefs);
         break;
      default:
         // do nothing
         break;
      }
   }
