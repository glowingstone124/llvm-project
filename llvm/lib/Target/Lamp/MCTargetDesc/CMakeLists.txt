add_llvm_component_library(LLVMLampDesc
  LampAsmBackend.cpp
  LampELFObjectWriter.cpp
  LampMCCodeEmitter.cpp
  LampInstPrinter.cpp
  LampMCTargetDesc.cpp

  LINK_COMPONENTS
  BinaryFormat
  MC
  MCParser
  Support
  TargetParser
)
