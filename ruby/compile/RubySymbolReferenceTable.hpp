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

#ifndef RUBY_SYMBOLREFERENCETABLE_INCL
#define RUBY_SYMBOLREFERENCETABLE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef RUBY_SYMBOLREFERENCETABLE_CONNECTOR
#define RUBY_SYMBOLREFERENCETABLE_CONNECTOR
namespace Ruby { class SymbolReferenceTable; }
namespace Ruby { typedef Ruby::SymbolReferenceTable SymbolReferenceTableConnector; }
#endif


#include "compile/OMRSymbolReferenceTable.hpp"
#include "infra/BitVector.hpp"
#include "cs2/hashtab.h"                       // for HashTable, etc

namespace TR { class Compilation; }
namespace OMR { class SymbolReference; }
class TR_CallSite;


namespace Ruby
{

class SymbolReferenceTable : public OMR::SymbolReferenceTableConnector
   {
   public:

   SymbolReferenceTable(size_t size, TR::Compilation *comp);

   TR::SymbolReference * findOrCreateRubyThreadSymbolRef(TR::ResolvedMethodSymbol*);

   void initializeRubyHelperSymbolRefs(uint32_t maxIndex);
   TR::SymbolReference * findOrCreateRubyHelperSymbolRef(TR_RuntimeHelper helper, bool canGCandReturn, bool canGCandExcept, bool preservesAllRegisters);
   TR::SymbolReference * createRubyNamedShadowSymRef(char* name, TR::DataType dt, size_t size, int32_t offset, bool killedAcrossCalls);
   TR::SymbolReference * createRubyNamedStaticSymRef(char* name, TR::DataType dt, void* addr,  int32_t offset, bool killedAcrossCalls);

   void initializeRubyRedefinedFlagSymbolRefs(uint32_t maxIndex);
   TR::SymbolReference * findRubyRedefinedFlagSymbolRef(int32_t bop);
   TR::SymbolReference * findOrCreateRubyRedefinedFlagSymbolRef(int32_t bop, char* name, TR::DataType dt, void* addr, int32_t offset, bool killedAcrossCalls);
   TR::SymbolReference * findRubyVMEventFlagsSymbolRef();
   TR::SymbolReference * findOrCreateRubyVMEventFlagsSymbolRef(char* name, TR::DataType dt, void* addr, int32_t offset, bool killedAcrossCalls);

   TR::SymbolReference * findOrCreateRubyInterruptFlagSymRef();
   TR::SymbolReference * findOrCreateRubyInterruptMaskSymRef();

   //Inlining
   TR::SymbolReference *setRubyInlinedReceiverTempSymRef(TR_CallSite* callSite, TR::SymbolReference* receiverTempSymRef);
   TR::SymbolReference *getRubyInlinedReceiverTempSymRef(TR_CallSite* callSite);

   private:

   // Ruby support
   TR::SymbolReference           *_ruby_vm_event_flags_SymRef;
   List<TR::SymbolReference>      _ruby_threadSymRefs;

   //Helper SymbolRefs and associated BitVector
   TR::SymbolReference **         _rubyHelperSymRefs;
   TR_BitVector                    _rubyHelperSymRefsBV;

   //Redefined Flag SymbolRefs
   TR::SymbolReference **         _rubyRedefinedFlagSymRefs;

   // Pending interrupt SymbolRefs.
   TR::SymbolReference *          _rubyInterrupt_flag_SymRef;
   TR::SymbolReference *          _rubyInterrupt_mask_SymRef;

   //Inlining
   CS2::HashTable<TR_CallSite*, TR::SymbolReference*, TR::Allocator> _ruby_inlined_receiver_temp_SymRef;

   };

}

#endif
