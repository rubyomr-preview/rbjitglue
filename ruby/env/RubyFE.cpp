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

#include "ruby/env/RubyFE.hpp"

#include "codegen/CodeGenerator.hpp"
#include "compile/CompilationTypes.hpp"
#include "env/FEBase_t.hpp"
#include "il/Block.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "optimizer/CallInfo.hpp"
#include "optimizer/InlinerFailureReason.hpp"
#include "ruby/config.h"
#include "ruby/version.h"

TR_RubyFE *TR_RubyFE::_instance = 0;

TR_RubyFE::TR_RubyFE(struct rb_vm_struct *vm)
   : TR::FEBase<TR_RubyFE>(),
     _vm(vm)
   {
   TR_ASSERT(!_instance, "TR_RubyFE must be initialized only once");
   _instance = this;
   }


const char *
TR_RubyFE::id2name(ID id)
   {
   return _vm->jit->callbacks.rb_id2name_f(id);
   }
