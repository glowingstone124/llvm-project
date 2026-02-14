add_llvm_component_library(LLVMLampAsmParser
  LampAsmParser.cpp

  LINK_COMPONENTS
  MC
  MCParser
  LampDesc
  LampInfo
  Support

)
