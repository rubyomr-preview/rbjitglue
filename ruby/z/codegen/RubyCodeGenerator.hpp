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


#ifndef RUBY_Z_CODEGENERATORBASE_INCL
#define RUBY_Z_CODEGENERATORBASE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef RUBY_CODEGENERATORBASE_CONNECTOR
#define RUBY_CODEGENERATORBASE_CONNECTOR

namespace Ruby { namespace Z { class CodeGenerator; } }
namespace Ruby { typedef Ruby::Z::CodeGenerator CodeGeneratorConnector; }

#else
#error Ruby::Z::CodeGenerator expected to be a primary connector, but a Ruby connector is already defined
#endif


#include "codegen/OMRCodeGenerator.hpp"


namespace Ruby
{

namespace Z
{

class OMR_EXTENSIBLE CodeGenerator : public OMR::CodeGeneratorConnector
   {
   public:

   CodeGenerator();

   TR::Linkage *createLinkage(TR_LinkageConventions lc);
   };

}

}
#endif


