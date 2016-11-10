##################################################################################
# (c) Copyright IBM Corp. 2000, 2016
#
#  This program and the accompanying materials are made available
#  under the terms of the Eclipse Public License v1.0 and
#  Apache License v2.0 which accompanies this distribution.
#
#      The Eclipse Public License is available at
#      http://www.eclipse.org/legal/epl-v10.html
#
#      The Apache License v2.0 is available at
#      http://www.opensource.org/licenses/apache2.0.php
#
# Contributors:
#    Multiple authors (IBM Corp.) - initial implementation and documentation
##################################################################################

# To lint: 
#
# PLATFORM=foo-bar-clang make -f linter.mk 
#

.PHONY: linter
linter::

# Handy macro to check to make sure variables are set
REQUIRE_VARS=$(foreach VAR,$(1),$(if $($(VAR)),,$(error $(VAR) must be set)))
$(call REQUIRE_VARS,OMR_RUBY_INSTALL)

ifeq ($(PLATFORM),ppc64-linux64-clangLinter)
    export LLVM_CONFIG?=/tr/llvm_checker/ppc-64/sles11/bin/llvm-config
    export CC_PATH?=/tr/llvm_checker/ppc-64/sles11/bin/clang
    export CXX_PATH?=/tr/llvm_checker/ppc-64/sles11/bin/clang++
else 
    #default paths, unless overriden 
    export LLVM_CONFIG?=llvm-config
    export CC_PATH?=clang
    export CXX_PATH?=clang++
endif

#
# First setup some important paths
# Personally, I feel it's best to default to out-of-tree build but who knows, there may be
# differing opinions on that.
#
JIT_SRCBASE?=..
JIT_OBJBASE?=../objs/ruby_$(BUILD_CONFIG)
JIT_DLL_DIR?=$(JIT_OBJBASE)

#
# Windows users will likely use backslashes, but Make tends to not like that so much
#
FIXED_SRCBASE=$(subst \,/,$(JIT_SRCBASE))
FIXED_OBJBASE=$(subst \,/,$(JIT_OBJBASE))
FIXED_DLL_DIR=$(subst \,/,$(JIT_DLL_DIR))

# TODO - "debug" as default?
BUILD_CONFIG?=prod

#
# This is where we setup our component dirs
# Note these are all relative to JIT_SRCBASE and JIT_OBJBASE
# It just makes sense since source and build dirs may be in different places 
# in the filesystem :)
#
JIT_OMR_DIRTY_DIR?=omr/compiler
JIT_PRODUCT_DIR?=ruby

#
# Dirs used internally by the makefiles
#
JIT_MAKE_DIR?=$(FIXED_SRCBASE)/ruby/build
JIT_SCRIPT_DIR?=$(FIXED_SRCBASE)/ruby/build/scripts

#
# First we set a bunch of tokens about the platform that the rest of the
# makefile will use as conditionals
#
include $(JIT_MAKE_DIR)/platform/common.mk

#
# Now we include the names of all the files that will go into building the JIT
# Will automatically include files needed from HOST and TARGET platform
#
include $(JIT_MAKE_DIR)/files/common.mk

#
# Now we configure all the tooling we will use to build the files
#
# There is quite a bit of shared stuff, but the overwhelming majority of this
# is toolchain-dependent.
#
# That makes sense - You can't expect XLC and GCC to take the same arguments
#
include $(JIT_MAKE_DIR)/toolcfg/common.mk


#
# Add OMRChecker targets
#
# This likely ought to be using the OMRChecker fragment that 
# exists in that repo, but in the mean time, this is fine. 
# 

OMRCHECKER_DIR?=$(JIT_SRCBASE)/omr/tools/compiler/OMRChecker

OMRCHECKER_OBJECT=$(OMRCHECKER_DIR)/OMRChecker.so

OMRCHECKER_LDFLAGS=`$(LLVM_CONFIG) --ldflags`
OMRCHECKER_CXXFLAGS=`$(LLVM_CONFIG) --cxxflags`

$(OMRCHECKER_OBJECT):
	cd $(OMRCHECKER_DIR); sh smartmake.sh

.PHONY: omrchecker omrchecker_test omrchecker_clean omrchecker_cleandll omrchecker_cleanall
omrchecker: $(OMRCHECKER_OBJECT)

omrchecker_test: $(OMRCHECKER_OBJECT)
	cd $(OMRCHECKER_DIR); make test

omrchecker_clean:
	cd $(OMRCHECKER_DIR); make clean

omrchecker_cleandll:
	cd $(OMRCHECKER_DIR); make cleandll

omrchecker_cleanall:
	cd $(OMRCHECKER_DIR); make cleanall


# The core linter bits.  
# 
# linter:: is the default target, and we construct a pre-req for each
#  .cpp file.
#
linter:: omrchecker 


# The clang invocation magic line.
LINTER_EXTRA=-Xclang -load -Xclang $(OMRCHECKER_OBJECT) -Xclang -add-plugin -Xclang omr-checker 
LINTER_FLAGS=-std=c++0x -w -fsyntax-only -ferror-limit=0 $(LINTER_FLAGS_EXTRA)

define DEF_RULE.linter
.PHONY: $(1).linted 

$(1).linted: $(1) omrchecker 
	$$(CXX_CMD) $(LINTER_FLAGS) $(LINTER_EXTRA)  $$(patsubst %,-D%,$$(CXX_DEFINES)) $$(patsubst %,-I'%',$$(CXX_INCLUDES)) -o $$@ -c $$<

linter:: $(1).linted 

endef # DEF_RULE.linter

RULE.linter=$(eval $(DEF_RULE.linter))

# The list of sources. 
JIT_CPP_FILES=$(filter %.cpp,$(JIT_PRODUCT_SOURCE_FILES) $(JIT_PRODUCT_BACKEND_SOURCES))

# Construct lint dependencies. 
$(foreach SRCFILE,$(JIT_CPP_FILES),\
   $(call RULE.linter,$(FIXED_SRCBASE)/$(SRCFILE)))

