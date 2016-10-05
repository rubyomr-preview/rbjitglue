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

/**
 * \file Provides includes from the Ruby VM in a reliable manner to avoid 
 *       mistakes. 
 */


#ifndef VMHEADERS_INCL
#define VMHEADERS_INCL

#include "omrcfg.h"                   //For OMR_JIT -- needs to come before vm_core.h


#ifndef OMR_JIT
#error "Can't compile the the JIT against jitless OMR" 
#endif

/* Ruby */
extern "C" {
#define RUBY_DONT_SUBST
#include "ruby.h"
#include "vm_core.h"
#include "jit.h"
#include "vm_insnhelper.h" // For BOP_MINUS and FIXNUM_ruby_redefined_OP_FLAG etc.
#include "iseq.h" 
#include "insns.inc" 
}

#endif

