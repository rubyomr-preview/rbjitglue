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


#ifndef RUBY_CODECACHEMANAGER_INCL
#define RUBY_CODECACHEMANAGER_INCL

#ifndef RUBY_CODECACHEMANAGER_COMPOSED
#define RUBY_CODECACHEMANAGER_COMPOSED
namespace Ruby { class CodeCacheManager; }
namespace Ruby { typedef CodeCacheManager CodeCacheManagerConnector; }
#endif

#include "runtime/OMRCodeCacheManager.hpp"

#include <stddef.h>
#include <stdint.h>
#include "codegen/FrontEnd.hpp"
#include "runtime/CodeCacheMemorySegment.hpp"

class TR_RubyJitConfig;
class TR_RubyFE;

namespace TR { class CodeCache; }
namespace TR { class CodeCacheManager; }

namespace Ruby
{

class OMR_EXTENSIBLE CodeCacheManager : public OMR::CodeCacheManagerConnector
   {
   TR::CodeCacheManager *self();
   
public:
   CodeCacheManager(TR_FrontEnd *fe) : OMR::CodeCacheManagerConnector(fe)
      {
      _codeCacheManager = reinterpret_cast<TR::CodeCacheManager *>(this);
      }

   void *operator new(size_t s, TR::CodeCacheManager *m) { return m; }

   static TR::CodeCacheManager *instance() { return _codeCacheManager; }
   static TR_RubyJitConfig *jitConfig()    { return _jitConfig; }
   TR_RubyFE *pyfe();

   TR::CodeCache *initialize(bool useConsolidatedCache, uint32_t numberOfCodeCachesToCreateAtStartup);

   void *getMemory(size_t sizeInBytes);
   void  freeMemory(void *memoryToFree);

   TR::CodeCacheMemorySegment *allocateCodeCacheSegment(size_t segmentSize,
                                                        size_t &codeCacheSizeToAllocate,
                                                        void *preferredStartAddress);

private :
   static TR::CodeCacheManager *_codeCacheManager;
   static TR_RubyJitConfig *_jitConfig;
   };

} // namespace Ruby

#endif // RUBY_CODECACHEMANAGER_INCL
