set(LLVM_TARGET_DEFINITIONS Lamp.td)

tablegen(LLVM LampGenCallingConv.inc  -gen-callingconv)
tablegen(LLVM LampGenRegisterInfo.inc -gen-register-info)
tablegen(LLVM LampGenInstrInfo.inc    -gen-instr-info)
tablegen(LLVM LampGenSubtargetInfo.inc -gen-subtarget)
tablegen(LLVM LampGenAsmWriter.inc -gen-asm-writer)
tablegen(LLVM LampGenDAGISel.inc      -gen-dag-isel)

add_public_tablegen_target(LampCommonTableGen)

add_llvm_target(LampCodeGen
        LampAsmPrinter.cpp
        LampFrameLowering.cpp
        LampInstrInfo.cpp
        LampISelDAGToDAG.cpp
        LampISelLowering.cpp
        LampZeroBranchPrep.cpp
        LampRegisterInfo.cpp
        LampSelectionDAGInfo.cpp
        LampSubtarget.cpp
        LampTargetMachine.cpp

        DEPENDS
        LampCommonTableGen

        LINK_COMPONENTS
        AsmPrinter
        CodeGen
        CodeGenTypes
        Core
        MC
        SelectionDAG
        Support
        Target
        TargetParser
        LampDesc
        LampInfo
)

add_subdirectory(TargetInfo)
add_subdirectory(MCTargetDesc)
add_subdirectory(AsmParser)
add_library(Lamp INTERFACE)
target_link_libraries(Lamp INTERFACE
        LLVMLampCodeGen
        LLVMLampInfo
)
set_property(TARGET Lamp PROPERTY LLVM_COMPONENT_NAME Lamp)
