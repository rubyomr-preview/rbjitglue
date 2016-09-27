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
# Defines rules for compiling source files into object files
#
# Each of these rules will also add themselves to jit_cleanobjs and jit_cleandeps
# to clean build artifacts and dependecy files, respectively
#
include $(JIT_MAKE_DIR)/rules/gnu/filetypes.mk

# Convert the source file names to object file names
JIT_PRODUCT_OBJECTS=$(patsubst %,$(FIXED_OBJBASE)/%.o,$(basename $(JIT_PRODUCT_SOURCE_FILES)))

# Figure out the name of the .so file
JIT_PRODUCT_SONAME=$(PRODUCT_DIR)/lib$(PRODUCT_NAME).so

# Add build name to JIT
JIT_PRODUCT_BUILDNAME_SRC=$(FIXED_OBJBASE)/$(JIT_OMR_DIRTY_DIR)/env/TRBuildName.cpp
JIT_PRODUCT_BUILDNAME_OBJ=$(FIXED_OBJBASE)/$(JIT_OMR_DIRTY_DIR)/env/TRBuildName.o
JIT_PRODUCT_OBJECTS+=$(JIT_PRODUCT_BUILDNAME_OBJ)

jit: $(JIT_PRODUCT_SONAME)

$(JIT_PRODUCT_SONAME): $(JIT_PRODUCT_OBJECTS) | jit_createdirs
	$(SOLINK_CMD) $(SOLINK_FLAGS) -o $@ $(SOLINK_PRE_OBJECTS) $(JIT_PRODUCT_OBJECTS) $(SOLINK_POST_OBJECTS) $(patsubst %,-l%,$(SOLINK_SLINK)) $(SOLINK_EXTRA_ARGS)

JIT_DIR_LIST+=$(dir $(JIT_PRODUCT_SONAME))

jit_cleandll::
	rm -f $(JIT_PRODUCT_SONAME)

$(call RULE.cpp,$(JIT_PRODUCT_BUILDNAME_OBJ),$(JIT_PRODUCT_BUILDNAME_SRC))    
    
.PHONY: $(JIT_PRODUCT_BUILDNAME_SRC)
$(JIT_PRODUCT_BUILDNAME_SRC): | jit_createdirs
	$(PERL_PATH) $(GENERATE_VERSION_SCRIPT) $(PRODUCT_RELEASE) > $@

JIT_DIR_LIST+=$(dir $(JIT_PRODUCT_BUILDNAME_SRC))

jit_cleanobjs::
	rm -f $(JIT_PRODUCT_BUILDNAME_SRC)

#
# This part calls the "RULE.x" macros for each source file
#
$(foreach SRCFILE,$(JIT_PRODUCT_SOURCE_FILES),\
    $(call RULE$(suffix $(SRCFILE)),$(FIXED_OBJBASE)/$(basename $(SRCFILE))$(OBJSUFF),$(FIXED_SRCBASE)/$(SRCFILE)) \
 )

#
# Generate a rule that will create every directory before the build starts
#
$(foreach JIT_DIR,$(sort $(JIT_DIR_LIST)),$(eval jit_createdirs:: ; mkdir -p $(JIT_DIR)))
