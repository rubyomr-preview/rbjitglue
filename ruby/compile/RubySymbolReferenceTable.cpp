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

#include "compile/SymbolReferenceTable.hpp"
#include "compile/Compilation.hpp"
#include "env/TRMemory.hpp"                    // for TR_HeapMemory, etc
#include "env/RubyFE.hpp"
#include "il/SymbolReference.hpp"              // for SymbolReference, etc
#include "il/symbol/StaticSymbol.hpp"          // for StaticSymbol, etc
#include "il/symbol/RegisterMappedSymbol.hpp"  // for RegisterMappedSymbol, etc
#include "il/symbol/ResolvedMethodSymbol.hpp"  // for ResolvedMethodSymbol
#include "il/symbol/ParameterSymbol.hpp"
#include "infra/List.hpp"


Ruby::SymbolReferenceTable::SymbolReferenceTable(size_t sizeHint, TR::Compilation *c) :
   OMR::SymbolReferenceTableConnector(sizeHint, c),
     _ruby_vm_event_flags_SymRef(0),
     _ruby_threadSymRefs(c->trMemory()),
     _rubyHelperSymRefs(0),
     _rubyHelperSymRefsBV(sizeHint, c->trMemory(), heapAlloc, growable, TR_Memory::BitVector),
     _rubyRedefinedFlagSymRefs(0),
     _rubyInterrupt_flag_SymRef(0),
     _rubyInterrupt_mask_SymRef(0),
     _ruby_inlined_receiver_temp_SymRef(c->allocator("SymRefTab"))
   {
   }


/**
 * Return the thread symref for a particular resolved method.
 */
TR::SymbolReference *
Ruby::SymbolReferenceTable::findOrCreateRubyThreadSymbolRef(TR::ResolvedMethodSymbol * methodSymbol)
   {

   auto index = methodSymbol->getResolvedMethodIndex();
   ListIterator<TR::SymbolReference> i(&_ruby_threadSymRefs);
   TR::SymbolReference * symRef;
   for (symRef = i.getFirst(); symRef; symRef = i.getNext())
      {
      if (symRef->getOwningMethodIndex() == index)
         return symRef;
      }

   // Not found.
   auto *parmSymbol = methodSymbol->getParameterList().getListHead()->getData();
   parmSymbol->setNotCollected();

   symRef = self()->findOrCreateAutoSymbol(methodSymbol,                 // Owning
                                         parmSymbol->getSlot(),        // Slot
                                         parmSymbol->getDataType(),    // DT
                                         true,                         // isReference
                                         false,                        // isInternalPointer
                                         true,                         // reuse auto
                                         false                         // isAdjunct
                                        );



   symRef->setOwningMethodIndex(index);
   _ruby_threadSymRefs.add(symRef);


   return symRef;

   }


void
Ruby::SymbolReferenceTable::initializeRubyHelperSymbolRefs(uint32_t maxIndex)
   {
   if(_rubyHelperSymRefs == NULL)
     {
     size_t sizeNeeded = (maxIndex + 1) * sizeof(TR::SymbolReference *);
     _rubyHelperSymRefs = (TR::SymbolReference **) trMemory()->allocateMemory(sizeNeeded, heapAlloc);
     memset(_rubyHelperSymRefs, 0, sizeNeeded);
     }
   }


TR::SymbolReference *
Ruby::SymbolReferenceTable::findOrCreateRubyHelperSymbolRef(TR_RuntimeHelper helper,
                                                           bool canGCandReturn,
                                                           bool canGCandExcept,
                                                           bool preservesAllRegisters)
   {
   uint32_t nameIndex = static_cast<uint32_t>(helper);
   if (_rubyHelperSymRefs[nameIndex] == NULL)
      {
      TR::SymbolReference *symRef = findOrCreateRuntimeHelper(helper, canGCandReturn, canGCandExcept, preservesAllRegisters);
      symRef->setEmptyUseDefAliases(self());
      symRef->getSymbol()->castToMethodSymbol()->setSystemLinkageDispatch();
      symRef->getSymbol()->castToMethodSymbol()->setLinkage(TR_System);
      _rubyHelperSymRefsBV.set(symRef->getReferenceNumber());
      _rubyHelperSymRefs[nameIndex] = symRef;
      }

   return _rubyHelperSymRefs[nameIndex];
   }


TR::SymbolReference *
Ruby::SymbolReferenceTable::createRubyNamedShadowSymRef(char* name, TR::DataType dt, size_t size, int32_t offset, bool killedAcrossCalls)
   {
   TR::Symbol* symbol = TR::Symbol::createNamedShadow(comp()->trHeapMemory(), dt, size, name);
   TR::SymbolReference* symRef = new (comp()->trHeapMemory()) TR::SymbolReference(self(), symbol, offset);
   symRef->setEmptyUseDefAliases(self());

   if(killedAcrossCalls)
      symRef->setAliasedTo(_rubyHelperSymRefsBV, self(), true /*symmetric*/);

   return symRef;
   }


TR::SymbolReference *
Ruby::SymbolReferenceTable::createRubyNamedStaticSymRef(char* name, TR::DataType dt, void* addr, int32_t offset, bool killedAcrossCalls)
   {
   TR::Symbol* symbol = TR::StaticSymbol::createNamed(comp()->trHeapMemory(), dt, addr, name);
   TR::SymbolReference* symRef = new (comp()->trHeapMemory()) TR::SymbolReference(self(), symbol, offset);
   symRef->setEmptyUseDefAliases(self());

   if (killedAcrossCalls)
      symRef->setAliasedTo(_rubyHelperSymRefsBV, self(), true /*symmetric*/);

   return symRef;
   }


void
Ruby::SymbolReferenceTable::initializeRubyRedefinedFlagSymbolRefs(uint32_t maxIndex)
   {
   if(_rubyRedefinedFlagSymRefs == NULL)
     {
     size_t sizeNeeded = (maxIndex + 1) * sizeof(TR::SymbolReference *);
     _rubyRedefinedFlagSymRefs = (TR::SymbolReference **) trMemory()->allocateMemory(sizeNeeded, heapAlloc);
     memset(_rubyRedefinedFlagSymRefs, 0, sizeNeeded);
     }
   }


TR::SymbolReference *
Ruby::SymbolReferenceTable::findOrCreateRubyRedefinedFlagSymbolRef(int32_t bop, char* name, TR::DataType dt, void* addr, int32_t offset, bool killedAcrossCalls)
   {

   if(_rubyRedefinedFlagSymRefs[bop] == NULL)
      _rubyRedefinedFlagSymRefs[bop] = createRubyNamedStaticSymRef(name, dt, addr, offset, killedAcrossCalls);

   return _rubyRedefinedFlagSymRefs[bop];
   }


TR::SymbolReference *
Ruby::SymbolReferenceTable::findRubyRedefinedFlagSymbolRef(int32_t bop)
   {
   return _rubyRedefinedFlagSymRefs[bop];
   }


TR::SymbolReference *
Ruby::SymbolReferenceTable::findOrCreateRubyVMEventFlagsSymbolRef(char* name, TR::DataType dt, void* addr, int32_t offset, bool killedAcrossCalls)
   {
   if (_ruby_vm_event_flags_SymRef == NULL)
      _ruby_vm_event_flags_SymRef = createRubyNamedStaticSymRef(name, dt, addr, offset, killedAcrossCalls);

   return _ruby_vm_event_flags_SymRef;
   }


TR::SymbolReference *
Ruby::SymbolReferenceTable::findRubyVMEventFlagsSymbolRef()
   {
   return _ruby_vm_event_flags_SymRef;
   }


TR::SymbolReference *
Ruby::SymbolReferenceTable::findOrCreateRubyInterruptFlagSymRef()
   {
   if (!_rubyInterrupt_flag_SymRef)
      _rubyInterrupt_flag_SymRef = createRubyNamedShadowSymRef("interrupt_flag",
                                                               TR_RubyFE::slotType(),
                                                               TR_RubyFE::SLOTSIZE,
                                                               offsetof(rb_thread_t, interrupt_flag),
                                                               true);
   return _rubyInterrupt_flag_SymRef;
   }


TR::SymbolReference *
Ruby::SymbolReferenceTable::findOrCreateRubyInterruptMaskSymRef()
   {
   if (!_rubyInterrupt_mask_SymRef)
      _rubyInterrupt_mask_SymRef = createRubyNamedShadowSymRef("interrupt_mask",
                                                               TR_RubyFE::slotType(),
                                                               TR_RubyFE::SLOTSIZE,
                                                               offsetof(rb_thread_t, interrupt_mask),
                                                               true);
   return _rubyInterrupt_mask_SymRef;
   }


TR::SymbolReference *
Ruby::SymbolReferenceTable::setRubyInlinedReceiverTempSymRef(TR_CallSite* callSite, TR::SymbolReference* receiverTempSymRef)
{
  _ruby_inlined_receiver_temp_SymRef.Add(callSite, receiverTempSymRef);
  return receiverTempSymRef;
}


TR::SymbolReference *
Ruby::SymbolReferenceTable::getRubyInlinedReceiverTempSymRef(TR_CallSite* callSite)
{
  if(_ruby_inlined_receiver_temp_SymRef.Locate(callSite))
    {
      return _ruby_inlined_receiver_temp_SymRef.Get(callSite);
    }
  else
    {
      return NULL;
    }
}
