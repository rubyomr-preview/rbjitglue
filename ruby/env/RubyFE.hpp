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


#ifndef RUBYFE_HPP_gfgYoA
#define RUBYFE_HPP_gfgYoA


#include "env/VMHeaders.hpp"
#include "env/FEBase.hpp"
#include "env/jittypes.h"
#include "runtime/RubyJitConfig.hpp"
#include "runtime/CodeCache.hpp"


class TR_RubyFE;

namespace TR
{
template <> struct FETraits<TR_RubyFE>
   {
   typedef ::TR_RubyJitConfig        JitConfig;
   typedef TR::CodeCacheManager      CodeCacheManager;
   typedef TR::CodeCache             CodeCache;
   static const size_t DEFAULT_SEG_SIZE = (128 * 1024); // 128kb
   static const size_t SLOTSIZE         = (sizeof(VALUE));
   };
}

class TR_RubyFE : public TR::FEBase<TR_RubyFE>
   {
   private:
   static TR_RubyFE *_instance;
   struct rb_vm_struct *_vm;

   public:
   // The constructor can only be called once by jitInit
   TR_RubyFE(struct rb_vm_struct *vm);
   static TR_RubyFE *instance()  { TR_ASSERT(_instance, "bad singleton"); return _instance; }

   // Map some of the traits in here for convenience
   static const size_t DEFAULT_SEG_SIZE = TR::FETraits<TR_RubyFE>::DEFAULT_SEG_SIZE;
   static const size_t SLOTSIZE         = TR::FETraits<TR_RubyFE>::SLOTSIZE;

   static TR::DataType slotType()
      { return SLOTSIZE == 8 ? TR::Int64 : TR::Int32; }


#if defined(TR_TARGET_S390)
   virtual void generateBinaryEncodingPrologue(TR_BinaryEncodingData *beData, TR::CodeGenerator *cg);
#endif

   rb_jit_t *getJitInterface() { return _vm->jit; }
   const char *id2name(ID);

   };

#endif /* RUBYFE_HPP_gfgYoA */
