add_llvm_component_library(LLVMLampDesc
  LampMCTargetDesc.cpp
  LINK_COMPONENTS
  MC
  Support
)
