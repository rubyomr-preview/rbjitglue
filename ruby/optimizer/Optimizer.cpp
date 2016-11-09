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

//On zOS XLC linker can't handle files with same name at link time
//This workaround with pragma is needed. What this does is essentially
//give a different name to the codesection (csect) for this file. So it
//doesn't conflict with another file with same name.
#pragma csect(CODE,"RubyOptimizer#C")
#pragma csect(STATIC,"RubyOptimizer#S")
#pragma csect(TEST,"RubyOptimizer#T")

#include "optimizer/Optimizer.hpp"
#include "optimizer/GlobalRegisterAllocator.hpp"
#include "optimizer/IsolatedStoreElimination.hpp"
#include "optimizer/LocalOpts.hpp"
#include "optimizer/RubyIlFastpather.hpp"
#include "optimizer/RubyLowerMacroOps.hpp"
#include "optimizer/RubyTrivialInliner.hpp"
#include "optimizer/RubyInliner.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"


// **********************************************************
//
// Ruby Strategies
//
// **********************************************************

static const OptimizationStrategy rubyCheapTacticalGlobalRegisterAllocatorOpts[] =
   {
   { OMR::redundantGotoElimination,          OMR::IfNotProfiling  }, // need to be run before global register allocator
   { OMR::lowerRubyMacroOps,                 OMR::MustBeDone      }, // Also must be done before GRA, to break asyncheck blocks
   { OMR::tacticalGlobalRegisterAllocator                         },
   { OMR::endGroup                                                }
   };

static const OptimizationStrategy rubyNoOptStrategyOpts[] =
   {
   { OMR::lowerRubyMacroOps,                         OMR::MustBeDone              },
   { OMR::endOpts                                                            },
   };

static const OptimizationStrategy rubyColdStrategyOpts[] =
   {
   { OMR::trivialInlining                                                    },
   { OMR::rubyIlFastpather                                                   },
   { OMR::basicBlockExtension                                                },
   { OMR::localCSE                                                           },
   { OMR::treeSimplification                                                 },
   { OMR::localCSE                                                           },
   { OMR::localDeadStoreElimination                                          },
   { OMR::globalDeadStoreGroup                                               },
   { OMR::isolatedStoreGroup                                                 },
   { OMR::deadTreesElimination                                               },
   { OMR::cheapTacticalGlobalRegisterAllocatorGroup                          },
   { OMR::lowerRubyMacroOps,                         OMR::MustBeDone              },
   { OMR::endOpts                                                            },
   };

static const OptimizationStrategy rubyWarmStrategyOpts[] =
   {
   //{ rubyIlFastpather                                                   },
   { OMR::basicBlockExtension                                                },
   { OMR::localCSE                                                           },
   { OMR::treeSimplification                                                 },
   { OMR::localCSE                                                           },
   { OMR::localDeadStoreElimination                                          },
   { OMR::globalDeadStoreGroup                                               },
   { OMR::isolatedStoreGroup                                                 },
   { OMR::deadTreesElimination                                               },
   { OMR::cheapTacticalGlobalRegisterAllocatorGroup                          },
   { OMR::lowerRubyMacroOps,                         OMR::MustBeDone              },
   { OMR::endOpts                                                            },
   };

const OptimizationStrategy *rubyCompilationStrategies[] =
   {
   rubyNoOptStrategyOpts,// only must-be-done opts
   rubyColdStrategyOpts, // <<  specialized
   rubyWarmStrategyOpts, // <<  specialized
   };


const OptimizationStrategy *
Ruby::Optimizer::optimizationStrategy(TR::Compilation *c)
   {
   TR_Hotness strategy = c->getMethodHotness();
   TR_ASSERT(strategy <= lastRubyStrategy, "Invalid optimization strategy");

   // Downgrade strategy rather than crashing in prod.
   if (strategy > lastRubyStrategy)
      strategy = lastRubyStrategy;

   return rubyCompilationStrategies[strategy];
   }


Ruby::Optimizer::Optimizer(TR::Compilation *comp, TR::ResolvedMethodSymbol *methodSymbol, bool isIlGen,
      const OptimizationStrategy *strategy, uint16_t VNType)
   : OMR::Optimizer(comp, methodSymbol, isIlGen, strategy, VNType)
   {
   // initialize additional Ruby optimizations
   _opts[OMR::isolatedStoreElimination] =
      new (comp->allocator()) TR::OptimizationManager(self(), TR_IsolatedStoreElimination::create, OMR::isolatedStoreElimination, "O^O ISOLATED STORE ELIMINATION: ");
   _opts[OMR::redundantGotoElimination] =
      new (comp->allocator()) TR::OptimizationManager(self(), TR_EliminateRedundantGotos::create, OMR::redundantGotoElimination, "O^O GOTO ELIMINATION: ");
   _opts[OMR::tacticalGlobalRegisterAllocator] =
      new (comp->allocator()) TR::OptimizationManager(self(), TR_GlobalRegisterAllocator::create, OMR::tacticalGlobalRegisterAllocator, "O^O GLOBAL REGISTER ASSIGNER: ");

  // NOTE: Please add new Ruby optimizations here!
  //
   _opts[OMR::rubyIlFastpather] =
      new (comp->allocator()) TR::OptimizationManager(self(), Ruby::IlFastpather::create, OMR::rubyIlFastpather, "O^O RUBY IL FASTPATHER");

   _opts[OMR::lowerRubyMacroOps] =
      new (comp->allocator()) TR::OptimizationManager(self(), Ruby::LowerMacroOps::create, OMR::lowerRubyMacroOps, "O^O LOWER RUBY MACRO OPS");

   _opts[OMR::trivialInlining] =
      new (comp->allocator()) TR::OptimizationManager(self(), Ruby::TrivialInliner::create, OMR::trivialInlining, "O^O RUBY TRIVIAL INLINER: ");

   // initialize additional Ruby optimization groups

   _opts[OMR::isolatedStoreGroup] =
      new (comp->allocator()) TR::OptimizationManager(self(), NULL, OMR::isolatedStoreGroup, "", isolatedStoreOpts);

   _opts[OMR::cheapTacticalGlobalRegisterAllocatorGroup] =
      new (comp->allocator()) TR::OptimizationManager(self(), NULL, OMR::cheapTacticalGlobalRegisterAllocatorGroup, "", rubyCheapTacticalGlobalRegisterAllocatorOpts);

   // NOTE: Please add new Ruby optimization groups here!

   // turn requested on for optimizations/groups
   _opts[OMR::lowerRubyMacroOps]->setRequested();

   }

inline
TR::Optimizer *Ruby::Optimizer::self()
   {
   return (static_cast<TR::Optimizer *>(this));
   }

OMR_InlinerUtil *Ruby::Optimizer::getInlinerUtil()
   {
   return new (comp()->allocator()) Ruby::InlinerUtil(comp());
   }
