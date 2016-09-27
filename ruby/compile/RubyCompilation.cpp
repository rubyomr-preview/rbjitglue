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

#pragma csect(CODE,"RubyCompilation#C")
#pragma csect(STATIC,"RubyCompilation#S")
#pragma csect(TEST,"RubyCompilation#T")

#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/TRMemory.hpp"
#include "ilgen/IlGenRequest.hpp"
#include "control/OptimizationPlan.hpp"
#include "optimizer/Optimizer.hpp"
#include "infra/List.hpp"
#include "il/Node.hpp"
#include "optimizer/OptimizationManager.hpp"
#include <stdint.h>
#include "compile/ResolvedMethod.hpp"


Ruby::Compilation::Compilation(
      int32_t id,
      OMR_VMThread *omrVMThread,
      TR_FrontEnd *fe,
      TR_ResolvedMethod *compilee,
      TR::IlGenRequest &ilGenRequest,
      TR::Options &options,
      const TR::Region &dispatchRegion,
      TR_Memory *m,
      TR_OptimizationPlan *optimizationPlan)
   : OMR::CompilationConnector(
      id,
      omrVMThread,
      fe,
      compilee,
      ilGenRequest,
      options,
      dispatchRegion,
      m,
      optimizationPlan)
   {
   }
