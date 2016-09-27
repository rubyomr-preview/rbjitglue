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


#ifndef RUBY_COMPILATION_INCL
#define RUBY_COMPILATION_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef RUBY_COMPILATION_CONNECTOR
#define RUBY_COMPILATION_CONNECTOR
namespace Ruby { class Compilation; }
namespace Ruby { typedef Ruby::Compilation CompilationConnector; }
#endif

#include "compile/OMRCompilation.hpp"
#include "env/OMRMemory.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"

class TR_FrontEnd;
class TR_ResolvedMethod;
class TR_OptimizationPlan;
namespace TR { class IlGenRequest; }
struct OMR_VMThread;

namespace Ruby
{

class Compilation : public OMR::CompilationConnector
   {
   public:

   TR_ALLOC(TR_Memory::Compilation)

   Compilation(
         int32_t compThreadId,
         OMR_VMThread *omrVMThread,
         TR_FrontEnd *,
         TR_ResolvedMethod *,
         TR::IlGenRequest &,
         TR::Options &,
         const TR::Region &dispatchRegion,
         TR_Memory *,
         TR_OptimizationPlan *optimizationPlan);

   ~Compilation() {}

   };

}

#endif
