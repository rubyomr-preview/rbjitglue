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

#include "ruby/env/RubyMethod.hpp"
#include "env/ConcreteFE.hpp"
#include "control/CompileMethod.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/IO.hpp"
#include "env/VMHeaders.hpp"
#include "ruby/version.h"
#include "ruby/config.h"
#include "env/CompilerEnv.hpp"
#include "env/RawAllocator.hpp"
#include "ras/DebugCounter.hpp"

extern void setupCodeCacheParameters(int32_t *, OMR::CodeCacheCodeGenCallbacks *callBacks, int32_t *numHelpers, int32_t *CCPreLoadedCodeSize);
typedef VALUE (*jit_method_t)(rb_thread_t*);

#ifdef TR_HOST_POWER
extern "C" VALUE compiledCodeDispatch(rb_thread_t *th, jit_method_t code, void *pseudoTOC);
#endif

extern TR_RuntimeHelperTable runtimeHelpers;

#if defined(TR_HOST_POWER) && !defined(__LITTLE_ENDIAN__)
//Big-Endian POWER.
//Helper Address is stored in a function descriptor consisting of [address, TOC, envp]
//Load out Helper address from this function descriptor.
#define helperAddress(x) (*(void**)(x))
#else
//Non-POWER or Little-Endian POWER.
//Load out Helper Address direclty, no function descriptor used.
#define helperAddress(x) (x)
#endif

#define initHelper(NAME) \
   runtimeHelpers.setAddress(RubyHelper_##NAME, helperAddress((void*)vm->jit->callbacks.NAME##_f))

static void
initializeAllHelpers(struct rb_vm_struct *vm, TR_RubyJitConfig *jitConfig)
   {
   initializeJitRuntimeHelperTable(false);

#if defined(TR_HOST_POWER)
   jitConfig->setInterpreterTOC(((size_t*)vm->jit->callbacks.rb_funcallv_f)[1]);
#endif

   initHelper(rb_funcallv);
   initHelper(vm_send);
   initHelper(vm_send_without_block);
   initHelper(vm_setconstant);
   initHelper(vm_getspecial);
   initHelper(lep_svar_set);
   initHelper(vm_getivar);
   initHelper(vm_setivar);
   initHelper(vm_opt_plus);
   initHelper(vm_opt_minus);
   initHelper(vm_opt_mult);
   initHelper(vm_opt_div);
   initHelper(vm_opt_mod);
   initHelper(vm_opt_eq);
   initHelper(vm_opt_neq);
   initHelper(vm_opt_lt);
   initHelper(vm_opt_le);
   initHelper(vm_opt_gt);
   initHelper(vm_opt_ge);
   initHelper(vm_opt_ltlt);
   initHelper(vm_opt_not);
   initHelper(vm_opt_aref);
   initHelper(vm_opt_aset);
   initHelper(vm_opt_length);
   initHelper(vm_opt_size);
   initHelper(vm_opt_empty_p);
   initHelper(vm_opt_succ);
   initHelper(rb_ary_new_capa);
   initHelper(rb_ary_new_from_values);
   initHelper(vm_expandarray);
   initHelper(rb_ary_resurrect);
   initHelper(vm_concatarray);
   initHelper(vm_splatarray);
   initHelper(rb_range_new);
   initHelper(rb_hash_new);
   initHelper(rb_hash_aset);
   initHelper(vm_trace);
   initHelper(rb_str_new);
   initHelper(rb_str_new_cstr);
   initHelper(rb_str_resurrect);
   initHelper(vm_get_ev_const);
   initHelper(vm_check_if_namespace);
   initHelper(rb_const_set);
   initHelper(rb_gvar_get);
   initHelper(rb_gvar_set);
   initHelper(rb_iseq_add_mark_object);
   initHelper(vm_setinlinecache);
   initHelper(vm_throw);
   initHelper(rb_threadptr_execute_interrupts);
   initHelper(rb_obj_as_string);
   initHelper(rb_str_append);
   initHelper(rb_vm_ep_local_ep);
   initHelper(vm_get_cbase);
   initHelper(vm_get_const_base);
   initHelper(rb_vm_get_cref);
   initHelper(vm_get_cvar_base);
   initHelper(rb_cvar_get);
   initHelper(rb_cvar_set);
   initHelper(vm_checkmatch);
   initHelper(rb_ary_tmp_new);
   initHelper(rb_ary_store);
   initHelper(rb_reg_new_ary);
   initHelper(vm_opt_regexpmatch1);
   initHelper(vm_opt_regexpmatch2);
   initHelper(vm_defined);
   initHelper(vm_invokesuper);
   initHelper(vm_invokeblock);
   initHelper(rb_method_entry);
   initHelper(rb_class_of);
   /*
   initHelper(vm_send_woblock_jit_inline_frame);
   initHelper(vm_send_woblock_inlineable_guard);
   */
   initHelper(rb_bug);
   initHelper(vm_exec_core);
#ifdef OMR_JIT_PROFILING
   initHelper(ruby_omr_is_valid_object);
#endif
   initHelper(rb_class2name);
   initHelper(vm_opt_aref_with);
   initHelper(vm_opt_aset_with);
   initHelper(rb_vm_env_write); 
   initHelper(vm_jit_stack_check); 
   initHelper(rb_str_freeze);
   initHelper(rb_ivar_set);
   initHelper(vm_compute_case_dest); 
   }

static void
initializeCodeCache(TR::CodeCacheManager &codeCacheManager)
   {
   TR::CodeCacheConfig &codeCacheConfig = codeCacheManager.codeCacheConfig();
   // setupCodeCacheParameters must stay before
   // TR_RubyCodeCacheManager::initialize() because it needs trampolineCodeSize
   setupCodeCacheParameters(&codeCacheConfig._trampolineCodeSize,
                            &codeCacheConfig._mccCallbacks,
                            &codeCacheConfig._numOfRuntimeHelpers,
                            &codeCacheConfig._CCPreLoadedCodeSize);
   codeCacheConfig._needsMethodTrampolines = true;
   codeCacheConfig._trampolineSpacePercentage = 5;
   codeCacheConfig._allowedToGrowCache = true;
   codeCacheConfig._lowCodeCacheThreshold = 0;
   codeCacheConfig._verboseCodeCache = false;
   codeCacheConfig._verbosePerformance = false;
   codeCacheConfig._verboseReclamation = false;
   codeCacheConfig._doSanityChecks = false;
   codeCacheConfig._codeCacheTotalKB = 128*1024;
   codeCacheConfig._codeCacheKB = 512;
   codeCacheConfig._codeCachePadKB = 0;
   codeCacheConfig._codeCacheAlignment = 32;
   codeCacheConfig._codeCacheFreeBlockRecylingEnabled = true;
   codeCacheConfig._largeCodePageSize = 0;
   codeCacheConfig._largeCodePageFlags = 0;
   codeCacheConfig._maxNumberOfCodeCaches = 256;
   codeCacheConfig._canChangeNumCodeCaches = true;
   codeCacheConfig._emitElfObject = TR::Options::getCmdLineOptions()->getOption(TR_PerfTool);

   codeCacheManager.initialize(true, 1);
   }

int jitInit(struct rb_vm_struct *vm, char *options)
   {

   // Create a bootstrap raw allocator.
   //
   TR::RawAllocator rawAllocator;

   try
      {
      // Allocate the host environment structure
      //
      TR::Compiler = new (rawAllocator) TR::CompilerEnv(rawAllocator, TR::PersistentAllocatorKit(rawAllocator));
      }
   catch (const std::bad_alloc& ba)
      {
      return -1;
      }

   TR::Compiler->initialize();

   // --------------------------------------------------------------------------

   static TR_RubyFE fe(vm); // singleton object

   initializeAllHelpers(vm, fe.jitConfig());

   if (commonJitInit(fe, options) < 0)
      return -1;

   initializeCodeCache(fe.codeCacheManager());

   vm->jit->default_count = TR::Options::getCmdLineOptions()->getInitialCount();

   if (TR::Options::getCmdLineOptions()->getOption(TR_EnableRubyTieredCompilation))
      {
      vm->jit->options |= TIERED_COMPILATION;
      }

   if (TR::Options::getCmdLineOptions()->getOption(TR_EnableRubyCodeCacheReclamation))
      {
      vm->jit->options |= CODE_CACHE_RECLAMATION;
      }

   return 0;
   }


static void accumulateAndPrintDebugCounters(TR_RubyFE& fe)
   {
   TR_Debug *debug = TR::Options::getDebug();
   if (debug)
      {
      TR::DebugCounterGroup *counters;
      counters = fe.getPersistentInfo()->getStaticCounters();
      if (counters)
         {
         counters->accumulate();
         debug->printDebugCounters(counters, "Static debug counters");
         }
      counters = fe.getPersistentInfo()->getDynamicCounters();
      if (counters)
         {
         counters->accumulate();
         debug->printDebugCounters(counters, "Dynamic debug counters");
         }
      }
   }

/**
 * Dumps information relevant to a crash.
 */
void jitCrashReport() 
   {
   auto &fe = TR_RubyFE::singleton();

   // Dump the debug counters: Can be very useful to 
   // investigate failures that are suspected to be 
   // related to the entry switch! 
   accumulateAndPrintDebugCounters(fe);
   }

int jitTerminate(void *)
   {
   auto &fe = TR_RubyFE::singleton();
   accumulateAndPrintDebugCounters(fe);


   TR::CodeCacheManager &codeCacheManager = fe.codeCacheManager();
   codeCacheManager.destroy();

   return 0;
   }

void *compileRubyISeq(rb_iseq_t *iseq, const char *name, TR_Hotness optLevel)
   {
   int32_t rc = 0;
   RubyMethodBlock mb(iseq, name);
   ResolvedRubyMethod compilee(mb);

   return compileMethod(NULL, compilee, optLevel, rc);
   }

extern "C"
{
/*
   _____      _                        _
  | ____|_  _| |_ ___ _ __ _ __   __ _| |
  |  _| \ \/ / __/ _ \ '__| '_ \ / _` | |
  | |___ >  <| ||  __/ |  | | | | (_| | |
  |_____/_/\_\\__\___|_|  |_| |_|\__,_|_|

   ___       _             __
  |_ _|_ __ | |_ ___ _ __ / _| __ _  ___ ___
   | || '_ \| __/ _ \ '__| |_ / _` |/ __/ _ \
   | || | | | ||  __/ |  |  _| (_| | (_|  __/
  |___|_| |_|\__\___|_|  |_|  \__,_|\___\___|

*/

#include "ruby.h"
#include "method.h"       // for rb_method_*
#include "vm_core.h"      // for rb_iseq_
#include "jit.h"

void jit_init(rb_vm_t *vm, char * options)
   {
   jitInit(vm, options);
   }

void jit_terminate(rb_vm_t *vm)
   {
   jitTerminate(vm);
   }

void *jit_compile(rb_iseq_t *iseq)
   {
   iseq_jit_body_info *body_info = NULL;
   void *startPC = NULL;
   TR_Hotness optLevel = cold;

   int32_t len =
      RSTRING_LEN(iseq->body->location.path) +
      (sizeof(size_t) * 3) +                // first_lineno: estimate three decimal digits per byte
      RSTRING_LEN(iseq->body->location.label) +
      3;                                    // two colons and a null terminator

   // FIXME: use std::string when it becomes possible
   char *name = (char*) malloc(len);
   auto truncated       = false;
   auto written         = snprintf(name, len, "%s:%ld:%s",
           (char*) RSTRING_PTR(iseq->body->location.path),
           FIX2LONG(iseq->body->location.first_lineno),
           (char* )RSTRING_PTR(iseq->body->location.label));

   if (written > len)
      {
      truncated = true;
      name[len - 1] = '\0'; //Null terminate truncated string.
      }

   // Prevent recompiling more than once:
   // If we have a chain of two body_info structs
   // for a given iseq, then no need to recompile again.
   if (TR::Options::getCmdLineOptions()->getOption(TR_EnableRubyTieredCompilation)
       && iseq->jit.body_info
       && iseq->jit.body_info->next)
      {
      if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseOptions))
         TR_VerboseLog::writeLineLocked(TR_Vlog_INFO,"%s%s @ %p already compiled twice, not compiling again",
                                        name,
                                        truncated ? "(truncated)" : "",
                                        iseq->jit.body_info->startPC);

      return NULL;
      }

   auto &fe = TR_RubyFE::singleton();

   if ((iseq->body->param.flags.has_opt
         && feGetEnv("TR_DISABLE_OPTIONAL_ARGUMENTS"))   ||
       iseq->body->param.flags.has_rest   ||
       iseq->body->param.flags.has_post   ||
       iseq->body->param.flags.has_block  ||
       iseq->body->param.flags.has_kw     ||
       iseq->body->param.flags.has_kwrest
       )
      {
      if (TR::Options::getVerboseOption(TR_VerboseOptions))
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_COMPFAIL,
            "<JIT: %s %s cannot be translated: complex arguments:"
            " opts %d rest %d post %d block %d keywords %d kwrest %d>\n",
            name,
            truncated ? "(truncated)" : "",
            iseq->body->param.flags.has_opt,
            iseq->body->param.flags.has_rest,
            iseq->body->param.flags.has_post,
            iseq->body->param.flags.has_block,
            iseq->body->param.flags.has_kw,
            iseq->body->param.flags.has_kwrest);
         }
      goto cleanupAndExit;
      }

   startPC = compileRubyISeq(iseq, name, optLevel);

   if (startPC)
      {
      body_info = ALLOC(iseq_jit_body_info);
      assert(body_info && "Failed to allocate body_info");

      body_info->opt_level = optLevel;
      body_info->startPC = startPC;
      }

cleanupAndExit:
   free(name);
   return (void *)body_info;
   }


VALUE jit_dispatch(rb_thread_t *th, jit_method_t code)
   {
#if !defined(TR_HOST_POWER)
   return (*code)(th);
#else
   /* On Power, we need to setup the TOC and the pseudoTOC for the
      jitted code -- compiledCodeDispatch does this for us */
   TR_RubyFE &fe = TR_RubyFE::singleton();
   return compiledCodeDispatch(th, code,
                               fe.jitConfig()->getPseudoTOC());
#endif
   }

/**
 * Invoked when Ruby crashes from the Ruby crash reporter. 
 *
 * Dump information of relevance to a crash!
 */
void jit_crash(void*) 
   {
   jitCrashReport();
   }

} /* extern C */
