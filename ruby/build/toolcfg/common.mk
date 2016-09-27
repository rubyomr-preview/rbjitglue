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
# If build didn't pass in the RUBY_IPATH, we will synthesize it
#
ifeq (,$(RUBY_IPATH))
    $(warning RUBY_IPATH not set, so we're taking a guess)

    # This gives us OMR_INCLUDES_FOR_JIT
    OMR_COMPONENT_INSTALL?=$(OMR_RUBY_INSTALL)/omr
    OMR_DIR:=$(OMR_COMPONENT_INSTALL)
    include $(OMR_DIR)/omrmakefiles/jitinclude.mk

    # TODO - There MUST be a nicer way to do this
    ifeq ($(SPEC),linux_ppc-64)
        OMR_EXT_INCLUDE=$(OMR_RUBY_INSTALL)/.ext/include/powerpc64-linux
    endif
    ifeq ($(SPEC),linux_x86-64)
        OMR_EXT_INCLUDE=$(OMR_RUBY_INSTALL)/.ext/include/x86_64-linux
    endif
    ifeq ($(SPEC),linux_ppcle-64)
        OMR_EXT_INCLUDE=$(OMR_RUBY_INSTALL)/.ext/include/powerpc64le-linux
    endif
    ifeq ($(SPEC),linux_390-64)
        OMR_EXT_INCLUDE=$(OMR_RUBY_INSTALL)/.ext/include/s390x-linux
    endif

    RUBY_INCLUDES=\
        $(OMR_RUBY_INSTALL)/include \
        $(OMR_EXT_INCLUDE) \
        $(OMR_INCLUDES_FOR_JIT) \
        $(OMR_RUBY_INSTALL)/omrglue \
        $(OMR_RUBY_INSTALL)

else
    RUBY_INCLUDES=$(RUBY_IPATH)
endif

PRODUCT_INCLUDES=\
    $(FIXED_SRCBASE)/$(JIT_PRODUCT_DIR)/$(TARGET_ARCH)/$(TARGET_SUBARCH) \
    $(FIXED_SRCBASE)/$(JIT_PRODUCT_DIR)/$(TARGET_ARCH) \
    $(FIXED_SRCBASE)/$(JIT_PRODUCT_DIR) \
    $(FIXED_SRCBASE)/$(RBJITGLUE_DIR) \
    $(RUBY_INCLUDES) \
    $(FIXED_SRCBASE)/$(JIT_OMR_DIRTY_DIR)/$(TARGET_ARCH)/$(TARGET_SUBARCH) \
    $(FIXED_SRCBASE)/$(JIT_OMR_DIRTY_DIR)/$(TARGET_ARCH) \
    $(FIXED_SRCBASE)/$(JIT_OMR_DIRTY_DIR) \
    $(FIXED_SRCBASE)/$(RELATIVE_OMR_DIR) \
    $(FIXED_SRCBASE)


PRODUCT_DEFINES+=OMR_RUBY RUBY_PROJECT_SPECIFIC

# TODO - Better way to get release name than hard-coding it here
PRODUCT_RELEASE?=tr.open.ruby

PRODUCT_DIR=$(FIXED_DLL_DIR)
PRODUCT_NAME?=rbjit

#
# Now we include the host and target tool config
# These don't really do much generally... They set a few defines but there really
# isn't a lot of stuff that's host/target dependent that isn't also dependent
# on what tools you're using
#
include $(JIT_MAKE_DIR)/toolcfg/host/$(HOST_ARCH).mk
include $(JIT_MAKE_DIR)/toolcfg/host/$(HOST_BITS).mk
include $(JIT_MAKE_DIR)/toolcfg/host/$(OS).mk
include $(JIT_MAKE_DIR)/toolcfg/target/$(TARGET_ARCH).mk
include $(JIT_MAKE_DIR)/toolcfg/target/$(TARGET_BITS).mk

#
# Now this is the big tool config file. This is where all the includes and defines
# get turned into flags, and where all the flags get setup for the different
# tools and file types
#
include $(JIT_MAKE_DIR)/toolcfg/$(TOOLCHAIN)/common.mk

