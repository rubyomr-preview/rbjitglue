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


#ifndef RUBY_ILGENERATOR_METHOD_DETAILS_INCL
#define RUBY_ILGENERATOR_METHOD_DETAILS_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef RUBY_ILGENERATOR_METHOD_DETAILS_CONNECTOR
#define RUBY_ILGENERATOR_METHOD_DETAILS_CONNECTOR
namespace Ruby { class IlGeneratorMethodDetails; }
namespace Ruby { typedef Ruby::IlGeneratorMethodDetails IlGeneratorMethodDetailsConnector; }
#endif

#include "ilgen/OMRIlGeneratorMethodDetails.hpp"

#include "infra/Annotations.hpp"
#include "env/IO.hpp"

class ResolvedRubyMethod;
class TR_ResolvedMethod;
class TR_IlGenerator;
class TR_InlineBlocks;
namespace TR { class ResolvedMethodSymbol; }
namespace TR { class SymbolReferenceTable; }

namespace Ruby
{

class OMR_EXTENSIBLE IlGeneratorMethodDetails : public OMR::IlGeneratorMethodDetailsConnector
   {

public:

   IlGeneratorMethodDetails() : _method(NULL) { }

   IlGeneratorMethodDetails(ResolvedRubyMethod *method) : _method(method) { }

   IlGeneratorMethodDetails(TR_ResolvedMethod *method);

   ResolvedRubyMethod * getMethod() { return _method; }

   bool sameAs(TR::IlGeneratorMethodDetails & other, TR_FrontEnd *fe);

   void print(TR_FrontEnd *fe, TR::FILE *file);

   virtual TR_IlGenerator *getIlGenerator(TR::ResolvedMethodSymbol *methodSymbol,
                                          TR_FrontEnd * fe,
                                          TR::Compilation *comp,
                                          TR::SymbolReferenceTable *symRefTab,
                                          bool forceClassLookahead,
                                          TR_InlineBlocks *blocksToInline);

protected:

   ResolvedRubyMethod * _method;
   };

}

#endif
