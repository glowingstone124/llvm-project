#include "Lamp.h"
#include "LampISelLowering.h"
#include "LampTargetMachine.h"
#include "MCTargetDesc/LampMCTargetDesc.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/InitializePasses.h"

using namespace llvm;

#define DEBUG_TYPE "lamp-isel"
#define PASS_NAME "Lamp DAG->DAG Pattern Instruction Selection"

namespace {
class LampDAGToDAGISel : public SelectionDAGISel {
public:
  LampDAGToDAGISel() = delete;

  LampDAGToDAGISel(LampTargetMachine &TM, CodeGenOptLevel OptLevel)
      : SelectionDAGISel(TM, OptLevel) {}

  void Select(SDNode *N) override;

#include "LampGenDAGISel.inc"
};

class LampDAGToDAGISelLegacy : public SelectionDAGISelLegacy {
public:
  static char ID;

  explicit LampDAGToDAGISelLegacy(LampTargetMachine &TM,
                                  CodeGenOptLevel OptLevel)
      : SelectionDAGISelLegacy(ID,
                               std::make_unique<LampDAGToDAGISel>(TM, OptLevel)) {}
};

} // namespace

char LampDAGToDAGISelLegacy::ID = 0;

INITIALIZE_PASS(LampDAGToDAGISelLegacy, DEBUG_TYPE, PASS_NAME, false, false)

FunctionPass *llvm::createLampISelDag(LampTargetMachine &TM,
                                      CodeGenOptLevel OptLevel) {
  return new LampDAGToDAGISelLegacy(TM, OptLevel);
}

void LampDAGToDAGISel::Select(SDNode *N) {
  if (N->isMachineOpcode()) {
    N->setNodeId(-1);
    return;
  }

  if (N->getOpcode() == LampISD::RET) {
    CurDAG->SelectNodeTo(N, Lamp::RET, MVT::Other);
    return;
  }

  SelectCode(N);
}
