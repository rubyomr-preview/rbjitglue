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


JIT_PRODUCT_SOURCE_FILES+=\
    $(JIT_OMR_DIRTY_DIR)/p/codegen/BinaryEvaluator.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/ControlFlowEvaluator.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/FPTreeEvaluator.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/GenerateInstructions.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/OMRMemoryReference.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/OpBinary.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/OpProperties.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/PPCAOTRelocation.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/PPCBinaryEncoding.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/OMRCodeGenerator.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/OMRInstruction.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/PPCDebug.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/PPCHelperCallSnippet.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/PPCInstruction.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/OMRLinkage.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/PPCSystemLinkage.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/OMRMachine.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/PPCOutOfLineCodeSection.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/OMRRealRegister.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/OMRRegisterDependency.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/OMRSnippet.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/PPCTableOfConstants.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/OMRTreeEvaluator.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/TreeEvaluatorVMX.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/UnaryEvaluator.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/OMRConstantDataSnippet.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/OMRRegisterIterator.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/env/OMRCPU.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/env/OMRDebugEnv.cpp \
