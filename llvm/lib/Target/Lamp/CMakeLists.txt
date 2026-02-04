set(LLVM_TARGET_DEFINITIONS Lamp.td)

tablegen(LLVM LampGenRegisterInfo.inc -gen-register-info)
tablegen(LLVM LampGenInstrInfo.inc    -gen-instr-info)
tablegen(LLVM LampGenDAGISel.inc      -gen-dag-isel)

add_public_tablegen_target(LampCommonTableGen)

add_llvm_target(LampCodeGen
        LampAsmPrinter.cpp
        LampFrameLowering.cpp
        LampInstrInfo.cpp
        LampISelDAGToDAG.cpp
        LampISelLowering.cpp
        LampRegisterInfo.cpp
        LampSubtarget.cpp
        LampTargetMachine.cpp

        DEPENDS
        LampCommonTableGen
)

add_subdirectory(TargetInfo)
add_subdirectory(MCTargetDesc)
add_library(Lamp INTERFACE)
target_link_libraries(Lamp INTERFACE
        LLVMLampCodeGen
        LLVMLampInfo
)
set_property(TARGET Lamp PROPERTY LLVM_COMPONENT_NAME Lamp)