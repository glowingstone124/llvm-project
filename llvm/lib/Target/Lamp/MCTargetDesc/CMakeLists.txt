add_llvm_component_library(LLVMLampDesc
  LampInstPrinter.cpp
  LampMCTargetDesc.cpp

  LINK_COMPONENTS
  MC
  MCParser
  Support
  TargetParser
)
