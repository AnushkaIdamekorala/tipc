#include "Optimizer.h"

#include "llvm/Passes/PassBuilder.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Scalar/InstSimplifyPass.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/Transforms/Scalar/DeadStoreElimination.h"

#include "loguru.hpp"
using namespace llvm;

void Optimizer::optimize(Module* theModule) {

    ModuleAnalysisManager MAM;


    ModulePassManager MPM;
    FunctionPassManager FPM;


    FPM.addPass(llvm::DSEPass());


    MPM.addPass(createModuleToFunctionPassAdaptor(std::move(FPM)));



    MPM.run(*theModule, MAM);
}