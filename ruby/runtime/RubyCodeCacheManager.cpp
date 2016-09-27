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

#include <sys/mman.h>
#include "ruby/env/RubyFE.hpp"
#include "ruby/runtime/RubyCodeCacheManager.hpp"

TR::CodeCacheManager *Ruby::CodeCacheManager::_codeCacheManager = NULL;
TR_RubyJitConfig *Ruby::CodeCacheManager::_jitConfig = NULL;

TR::CodeCacheManager *
Ruby::CodeCacheManager::self()
   {
   return static_cast<TR::CodeCacheManager *>(this);
   }


TR_RubyFE *
Ruby::CodeCacheManager::pyfe()
   {
   return reinterpret_cast<TR_RubyFE *>(self()->fe());
   }

   
TR::CodeCache *
Ruby::CodeCacheManager::initialize(bool useConsolidatedCache, uint32_t numberOfCodeCachesToCreateAtStartup)
   {
   _jitConfig = self()->pyfe()->jitConfig();
   return self()->OMR::CodeCacheManager::initialize(useConsolidatedCache, numberOfCodeCachesToCreateAtStartup);
   }

void *
Ruby::CodeCacheManager::getMemory(size_t sizeInBytes)
   {
   void * ptr = malloc(sizeInBytes);
   return ptr;
   }

void
Ruby::CodeCacheManager::freeMemory(void *memoryToFree)
   {
   free(memoryToFree);
   }

TR::CodeCacheMemorySegment *
Ruby::CodeCacheManager::allocateCodeCacheSegment(size_t segmentSize,
                                              size_t &codeCacheSizeToAllocate,
                                              void *preferredStartAddress)
   {
   // ignore preferredStartAddress for now, since it's NULL anyway
   //   goal would be to allocate code cache segments near the JIT library address
   codeCacheSizeToAllocate = segmentSize;
   TR::CodeCacheConfig & config = self()->codeCacheConfig();
   if (segmentSize < config.codeCachePadKB() << 10)
      codeCacheSizeToAllocate = config.codeCachePadKB() << 10;
   uint8_t *memorySlab = (uint8_t *) mmap(NULL,
                                          codeCacheSizeToAllocate,
                                          PROT_READ | PROT_WRITE | PROT_EXEC,
                                          MAP_ANONYMOUS | MAP_PRIVATE,
                                          0,
                                          0);

   TR::CodeCacheMemorySegment *memSegment = (TR::CodeCacheMemorySegment *) ((size_t)memorySlab + codeCacheSizeToAllocate - sizeof(TR::CodeCacheMemorySegment));
   new (memSegment) TR::CodeCacheMemorySegment(memorySlab, reinterpret_cast<uint8_t *>(memSegment));
   return memSegment;
   }
