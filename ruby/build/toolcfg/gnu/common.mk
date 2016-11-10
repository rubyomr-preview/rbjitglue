###################################################################################
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


#
# Explicitly set shell
#
SHELL=/bin/sh

#
# These are the prefixes and suffixes that all GNU tools use for things
#
OBJSUFF=.o
ARSUFF=.a
SOSUFF=.so
EXESUFF=
LIBPREFIX=lib
DEPSUFF=.depend.mk

#
# Paths for default programs on the platform
# Most rules will use these default programs, but they can be overwritten individually if,
# for example, you want to compile .spp files with a different C++ compiler than you use
# to compile .spp files
#
AR_PATH?=ar
SED_PATH?=sed
M4_PATH?=m4
PERL_PATH?=perl

ifeq ($(OS),osx)
    AS_PATH?=clang
else
    AS_PATH?=as
endif

# TODO - Redo GCC version logic that used to be here
ifeq ($(C_COMPILER),gcc)
    CC_PATH?=gcc
    CXX_PATH?=g++
endif

ifeq ($(C_COMPILER),clang)
    CC_PATH?=clang
    CXX_PATH?=clang++
endif

AS_VERSION:=$(shell $(AS_PATH) --version)
ifneq (,$(findstring LLVM,$(AS_VERSION)))
    LLVM_ASSEMBLER:=1
endif

# This is the script that's used to generate TRBuildName.cpp
GENERATE_VERSION_SCRIPT?=$(JIT_SCRIPT_DIR)/generateVersion.pl

#
# First setup C and C++ compilers. 
#
#     Note: "CX" means both C and C++
#
C_CMD?=$(CC_PATH)
CXX_CMD?=$(CXX_PATH)

CX_DEFINES+=\
    $(PRODUCT_DEFINES) \
    $(HOST_DEFINES) \
    $(TARGET_DEFINES) \
    SUPPORTS_THREAD_LOCAL \
    _LONG_LONG

CX_FLAGS+=\
    -pthread \
    -fomit-frame-pointer \
    -fasynchronous-unwind-tables \
    -fno-dollars-in-identifiers \
    -fPIC \
    -fno-strict-aliasing \
    -Wreturn-type \
    -g
    
CXX_FLAGS+=\
    -std=c++0x \
    -fno-rtti \
    -fno-threadsafe-statics \
    -fno-strict-aliasing \
    -Wno-deprecated \
    -Wno-enum-compare \
    -Wno-invalid-offsetof \
    -Wno-write-strings

ifeq ($(C_COMPILER),clang)
    SOLINK_FLAGS+=-undefined dynamic_lookup
endif

CX_DEFINES_DEBUG+=DEBUG
CX_FLAGS_DEBUG+=-g -ggdb3

CX_DEFAULTOPT=-O3
CX_OPTFLAG?=$(CX_DEFAULTOPT)
CX_FLAGS_PROD+=$(CX_OPTFLAG)

ifeq ($(HOST_ARCH),x)
    # Nothing for X
endif

ifeq ($(HOST_ARCH),p)
    CX_DEFINES+=LINUXPPC LINUXPPC64 USING_ANSI
    CX_FLAGS_PROD+=-mcpu=powerpc64

    ifdef ENABLE_SIMD_LIB
        CX_DEFINES+=ENABLE_SPMD_SIMD
        CX_FLAGS+=-qaltivec -qarch=pwr7 -qtune=pwr7
    endif
endif

ifeq ($(HOST_ARCH),z)
    CX_DEFINES+=S390 S39064 FULL_ANSI MAXMOVE
    CX_FLAGS+=-march=z900 -mtune=z9-109 -mzarch
endif

ifeq ($(HOST_BITS),64)
    CX_FLAGS+=-m64
endif

ifeq ($(C_COMPILER),clang)
    CX_FLAGS+=-Wno-parentheses -Werror=header-guard
endif

ifeq ($(BUILD_CONFIG),debug)
    CX_DEFINES+=$(CX_DEFINES_DEBUG)
    CX_FLAGS+=$(CX_FLAGS_DEBUG)
endif

ifeq ($(BUILD_CONFIG),prod)
    CX_DEFINES+=$(CX_DEFINES_PROD)
    CX_FLAGS+=$(CX_FLAGS_PROD)
endif

C_INCLUDES=$(PRODUCT_INCLUDES)
C_DEFINES+=$(CX_DEFINES) $(CX_DEFINES_EXTRA) $(C_DEFINES_EXTRA)
C_FLAGS+=$(CX_FLAGS) $(CX_FLAGS_EXTRA) $(C_FLAGS_EXTRA)

CXX_INCLUDES=$(PRODUCT_INCLUDES)
CXX_DEFINES+=$(CX_DEFINES) $(CX_DEFINES_EXTRA) $(CXX_DEFINES_EXTRA)
CXX_FLAGS+=$(CX_FLAGS) $(CX_FLAGS_EXTRA) $(CXX_FLAGS_EXTRA)

#
# We now setup the GNU Assembler (GAS)
#
S_CMD?=$(AS_PATH)

S_INCLUDES=$(PRODUCT_INCLUDES)

S_DEFINES+=$(HOST_DEFINES) $(TARGET_DEFINES)

ifeq ($(LLVM_ASSEMBLER),1)
    S_FLAGS+=-Wa,--noexecstack
else
    S_FLAGS+=--noexecstack
    S_FLAGS_DEBUG+=--gstabs
endif

S_DEFINES_DEBUG+=DEBUG

ifeq ($(HOST_ARCH),x)
  ifeq ($(LLVM_ASSEMBLER),1)
        S_FLAGS+=-march=x86-64 -c
    else
        S_FLAGS+=--64
    endif
endif

ifeq ($(HOST_ARCH),z)
    S_FLAGS+=-march=z990 -mzarch
endif

ifeq ($(BUILD_CONFIG),debug)
    S_DEFINES+=$(S_DEFINES_DEBUG)
    S_FLAGS+=$(S_FLAGS_DEBUG)
endif

ifeq ($(BUILD_CONFIG),prod)
    S_DEFINES+=$(S_DEFINES_PROD)
    S_FLAGS+=$(S_FLAGS_PROD)
endif

S_DEFINES+=$(S_DEFINES_EXTRA)
S_FLAGS+=$(S_FLAGS_EXTRA)

#
# Now setup ASM and Preprocessed ASM (PASM) on x86
#
# Because GNU Assembler doesn't understand MASM syntax, we use MASM2GAS
# to translate it
#
ifeq ($(HOST_ARCH),x)
    ASM_SCRIPT?=$(JIT_SCRIPT_DIR)/masm2gas.pl

    ASM_INCLUDES=$(PRODUCT_INCLUDES)
    
    ASM_FLAGS+=--64

    ifeq ($(BUILD_CONFIG),debug)
        ASM_FLAGS+=$(ASM_FLAGS_DEBUG)
    endif

    ifeq ($(BUILD_CONFIG),prod)
        ASM_FLAGS+=$(ASM_FLAGS_PROD)
    endif

    ASM_FLAGS+=$(ASM_FLAGS_EXTRA)
    
    PASM_CMD?=$(CC_PATH)

    PASM_INCLUDES=$(PRODUCT_INCLUDES)
    
    ifeq ($(BUILD_CONFIG),debug)
        PASM_FLAGS+=$(PASM_FLAGS_DEBUG)
    endif

    ifeq ($(BUILD_CONFIG),prod)
        PASM_FLAGS+=$(PASM_FLAGS_PROD)
    endif
    
    PASM_FLAGS+=$(PASM_FLAGS_EXTRA)
endif

#
# PowerPC has these .spp files that are preprocessed PPC assembly
# However, after being preprocessed with CPP, they contain comments that start
# with a bang '!' that need to be converted to start with a pound '#' so they can
# be understood by GAS
#
ifeq ($(HOST_ARCH),p)
    IPP_CMD?=$(SED_PATH)
    
    ifeq ($(BUILD_CONFIG),debug)
        IPP_FLAGS+=$(IPP_FLAGS_DEBUG)
    endif

    ifeq ($(BUILD_CONFIG),prod)
        IPP_FLAGS+=$(IPP_FLAGS_PROD)
    endif
    
    IPP_FLAGS+=$(IPP_FLAGS_EXTRA)
    
    SPP_CMD?=$(CC_PATH)
    
    SPP_INCLUDES=$(PRODUCT_INCLUDES)
    SPP_DEFINES+=$(CX_DEFINES)
    SPP_FLAGS+=$(CX_FLAGS)
    
    ifeq ($(BUILD_CONFIG),debug)
        SPP_DEFINES+=$(SPP_DEFINES_DEBUG)
        SPP_FLAGS+=$(SPP_FLAGS_DEBUG)
    endif

    ifeq ($(BUILD_CONFIG),prod)
        SPP_DEFINES+=$(SPP_DEFINES_PROD)
        SPP_FLAGS+=$(SPP_FLAGS_PROD)
    endif
    
    SPP_DEFINES+=$(SPP_DEFINES_EXTRA)
    SPP_FLAGS+=$(SPP_FLAGS_EXTRA)
endif

#
# System/z uses preprocessed assembly written using GNU M4
#
ifeq ($(HOST_ARCH),z)
    M4_CMD?=$(M4_PATH)

    M4_INCLUDES=$(PRODUCT_INCLUDES)

    M4_DEFINES+=$(HOST_DEFINES) $(TARGET_DEFINES)
    
    ifeq ($(BUILD_CONFIG),debug)
        M4_DEFINES+=$(M4_DEFINES_DEBUG)
        M4_FLAGS+=$(M4_FLAGS_DEBUG)
    endif

    ifeq ($(BUILD_CONFIG),prod)
        M4_DEFINES+=$(M4_DEFINES_PROD)
        M4_FLAGS+=$(M4_FLAGS_PROD)
    endif

    M4_DEFINES+=$(M4_DEFINES_EXTRA)
    M4_FLAGS+=$(M4_FLAGS_EXTRA)
endif

#
# The GNU archiver is nothing special... pretty much standard UNIX syntax
# It doesn't have to use ranlib, so that's rather nice :)
#
AR_CMD?=$(AR_PATH)
AR_OPTS?=rcv

#
# There really isn't much to setting up the linker for Ruby actually
# This will look a lot crazier on J9
#
SOLINK_CMD?=$(CXX_PATH)
SOLINK_FLAGS+=-shared
SOLINK_SLINK+=dl pthread
SOLINK_FLAGS_PROD=

ifeq ($(BUILD_CONFIG),debug)
    SOLINK_FLAGS+=$(SOLINK_FLAGS_DEBUG)
endif

ifeq ($(BUILD_CONFIG),prod)
    SOLINK_FLAGS+=$(SOLINK_FLAGS_PROD)
endif

SOLINK_FLAGS+=$(SOLINK_FLAGS_EXTRA)
