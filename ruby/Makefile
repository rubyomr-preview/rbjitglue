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



.PHONY: all clean cleanobjs cleandeps cleandll
all:
clean:
cleanobjs:
cleandeps:
cleandll:

# This is the logic right now for locating Clang and LLVM-config
# There's probably a nicer way to do all of this... it's pretty bad

ifeq ($(PLATFORM),amd64-linux64-clang)
    # Luckily we can just use the default path for Clang :)
endif

ifeq ($(PLATFORM),ppc64-linux64-clang)
    export CC_PATH?=/tr/llvm_checker/ppc-64/sles11/bin/clang
    export CXX_PATH?=/tr/llvm_checker/ppc-64/sles11/bin/clang++
endif

ifeq ($(PLATFORM),s390-zos64-vacpp)
    export CC_PATH?=/usr/lpp/cbclib/xlc/bin/xlc
    export CXX_PATH?=/usr/lpp/cbclib/xlc/bin/xlC
    export A2E_INCLUDE_PATH?=/usr/lpp/cbclib/include
endif

ifeq ($(PLATFORM),arm-linux-gcc-cross)
    export CC_PATH?=arm-bcm2708hardfp-linux-gnueabi-gcc
    export CXX_PATH?=arm-bcm2708hardfp-linux-gnueabi-g++
    export AS_PATH?=arm-bcm2708hardfp-linux-gnueabi-as
    SUBMAKE_EXTRAARGS=PLATFORM=arm-linux-gcc
endif

SUBMAKE_EXTRAARGS=ASSUMES=1

#
# Add Ruby to base targets and lint targets
#
all: ruby
clean: ruby_clean
cleanobjs: ruby_cleanobjs
cleandeps: ruby_cleandeps
cleandll: ruby_cleandll

#
# Add Ruby targets
#
.PHONY: ruby ruby_clean ruby_cleanobjs ruby_cleandeps ruby_cleandll

ruby:
	$(MAKE) -f ruby.mk $(SUBMAKE_EXTRAARGS)

ruby_clean:
	$(MAKE) -f ruby.mk clean $(SUBMAKE_EXTRAARGS)

ruby_cleanobjs:
	$(MAKE) -f ruby.mk cleanobjs $(SUBMAKE_EXTRAARGS)

ruby_cleandeps:
	$(MAKE) -f ruby.mk cleandeps $(SUBMAKE_EXTRAARGS)

ruby_cleandll:
	$(MAKE) -f ruby.mk cleandll $(SUBMAKE_EXTRAARGS)
