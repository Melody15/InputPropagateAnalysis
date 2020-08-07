#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/IR/DebugInfo.h"
#include <vector>

using namespace llvm;

typedef std::pair<Instruction *, Function *> CallSite;
typedef std::vector<CallSite> CallSites;
typedef std::vector<std::string> InlineAsmPatternSet;
typedef std::vector<Instruction *> PropaChain;

namespace {
struct ValuePropagatePass : public ModulePass {
  static char ID;
  ValuePropagatePass() : ModulePass(ID) {}

  virtual bool runOnModule(Module &M) override;
  void valuePropagate(Instruction *Inst, PropaChain &cur_propa_chain, int depth, Function *F);
  CallSites getCallSites(Function *called_function);
  bool rePropagateDetech(Instruction *U_inst, PropaChain &cur_propa_chain);
  void printSourceInfo(Instruction *Inst);

  InlineAsmPatternSet INPUT_INLINEASM_PATTERN_SET{
  "inb",
  "inw",
  "inl",
  "movb $1,$0",
  "movl $1,$0",
  "movw $1,$0",
  "movq $1,$0",
  "movb %gs:$1,$0",
  "movl %gs:$1,$0",
  "movw %gs:$1,$0",
  "movq %gs:$1,$0",
  "movb %gs:$1,$0",
  "movl %fs:$1,$0",
  "movw %fs:$1,$0",
  "movw %fs:$1,$0"
  };
};
} // namespace

bool ValuePropagatePass::runOnModule(Module &M) {
  errs() << "In module called: " << M.getName() << "!\n";
  int BBCount = 0;
  for (auto &F : M) {
    errs() << "Function name: ";
    errs() << F.getName() << '\n';
    // Focus on rtc_cmos_read within kernel
    // if (F.getName() != "rtc_cmos_read")
    //   continue;
    for (Function::iterator BB = F.begin(), e = F.end(); BB != e; BB++) {
      for (BasicBlock::iterator I = BB->begin(), e2 = BB->end(); I != e2; I++) {
        if (CallInst *call_inst = dyn_cast<CallInst>(I)) {
          // called_function is an inline asm
          if (call_inst->isInlineAsm()) {
            // return value was used
            if (call_inst->getNumUses() > 0) {
              // inline_asm inst is IO input
              bool is_input_inst = false;
              Value *called_value = call_inst->getCalledValue();
              InlineAsm *inline_asm = dyn_cast<InlineAsm>(called_value);
              const std::string asm_str = inline_asm->getAsmString();
              for (auto pattern : INPUT_INLINEASM_PATTERN_SET) {
                if (asm_str.find(pattern) != std::string::npos)
                  is_input_inst = true;
              }
              if (is_input_inst) {
                outs() << "Function name: " << F.getName() << '\n';
                outs() << *call_inst << "\n";
                outs() << "  ";
                printSourceInfo(call_inst);
                PropaChain cur_propa_chain;
                //cur_propa_chain.push_back(call_inst);
                valuePropagate(call_inst, cur_propa_chain, 1, &F);
                outs() << "\n--------------------------------------------------"
                          "-----------------------------------------\n\n";
              }
            }
          }
        }
      }
      BBCount++;
    }
  }
  outs() << "\nBasic Block num: " << BBCount << "\n";
  return false;
}

void ValuePropagatePass::valuePropagate(Instruction *Inst, PropaChain &cur_propa_chain, int depth, Function *F) {
  if (Inst->getNumUses() > 0) {
    // outs() << Inst->getNumUses() << "\n";
    for (User *U : Inst->users()) {
      if (Instruction *U_inst = dyn_cast<Instruction>(U)) {
        // loop detect
        bool inst_exist = rePropagateDetech(U_inst, cur_propa_chain);
        if (!inst_exist) {
          // outs() << U_inst->getNumUses() << "\n";
          for (int i = 0; i < depth; i++)
            outs() << "  ";
          outs() << "###" << depth << " " << F->getName() << ": " << *U_inst << "\n";
          for (int i = 0; i < depth; i++)
            outs() << "  ";
          printSourceInfo(U_inst);
          // Inst not br
          // if (!U_inst->isTerminator()){
          //   valuePropagate(U_inst, depth + 1);
          // }
          cur_propa_chain.push_back(U_inst);
          if (U_inst->getOpcode() == 1) { // isRetInst
            CallSites cur_call_sites = getCallSites(F); // return type vector<pair<Inst*, Function*>>
            for (auto CallSite : cur_call_sites) {
              for (int i = 0; i < depth; i++)
                outs() << "  ";
              outs() << "value return to callsite"
                    << "\n";
              for (int i = 0; i < depth + 1; i++)
                outs() << "  ";
              outs() << "###" << depth + 1 << " " << CallSite.second->getName()
                    << ": " << *(CallSite.first) << "\n";
              for (int i = 0; i < depth + 1; i++)
                outs() << "  ";
              printSourceInfo(CallSite.first);
              cur_propa_chain.push_back(CallSite.first);
              valuePropagate(CallSite.first, cur_propa_chain, depth + 2, CallSite.second);
            }
          } else if (U_inst->getOpcode() == 33) { // isStore type
            StoreInst *store_inst = dyn_cast<StoreInst>(U_inst);
            // outs() << *(store_inst->getPointerOperand()) << "\n";
            for (User *U : store_inst->getPointerOperand()->users()) {
              Instruction *store_user_inst = dyn_cast<Instruction>(U);
              if (store_user_inst != U_inst) {
                for (int i = 0; i < depth + 1; i++)
                  outs() << "  ";
                outs() << "###" << depth + 1 << " " << F->getName() << ": "
                       << *U << "\n";
                for (int i = 0; i < depth + 1; i++)
                  outs() << "  ";
                printSourceInfo(store_user_inst);
                cur_propa_chain.push_back(store_user_inst);
                valuePropagate(store_user_inst, cur_propa_chain, depth + 2, F);
              }
            }
          } else
            valuePropagate(U_inst, cur_propa_chain, depth + 1, F);
        } else {
          for (int i = 0; i < depth; i++)
            outs() << "  ";
          outs() << "###" << depth << " " << F->getName() << ": " << *U_inst << "\n";
          for (int i = 0; i < depth; i++)
            outs() << "  ";
          printSourceInfo(U_inst);
          for (int i = 0; i < depth; i++)
            outs() << "  ";
          outs() << "### WARNING: LOOP/REPLICATIIN DETECT! STOP PROPAGATE\n";
        }
      }
    }
  }
}

CallSites ValuePropagatePass::getCallSites(Function *called_function) {
  CallSites cur_call_sites;
  for (User *F_user : called_function->users()) {
    if (Instruction *UF_inst = dyn_cast<Instruction>(F_user)) {
      cur_call_sites.push_back(CallSite(UF_inst, UF_inst->getFunction()));
      errs() << *UF_inst << "!\n"; //callsite
      // outs() << "--------" << *UF_inst << "-------\n";
      // outs() << "---NAME:" << UF_inst->getFunction()->getName() <<
      // "-------\n";
    }
  }
  return cur_call_sites;
}

bool ValuePropagatePass::rePropagateDetech(Instruction *U_inst, PropaChain &cur_propa_chain){
  bool inst_exist = false;
  if(cur_propa_chain.empty())
    return inst_exist;
  for (auto Last_inst: cur_propa_chain) {
    if (U_inst == Last_inst)
      inst_exist = true;
  }
  return inst_exist;
}

void ValuePropagatePass::printSourceInfo(Instruction *Inst) {
  if (DILocation *Loc = Inst->getDebugLoc()) { // Here I is an LLVM instruction
    unsigned Line = Loc->getLine();
    StringRef File = Loc->getFilename();
    StringRef Dir = Loc->getDirectory();
    // bool ImplicitCode = Loc->isImplicitCode();
    outs() << "### SOURCE INFO: " << Dir << "/" << File << ": line " << Line << "\n";
  } else
    outs() << "### WARNING: NO MATCHED SOURCE INFO\n";
}

char ValuePropagatePass::ID = 0;

static RegisterPass<ValuePropagatePass> X("ValuePropagate",
                                          "This is ValuePropagetes Pass");

/*
Unhandled case
- value propagate via store: indirect propagate => pointer analysis
 - Example: ###3 cmos_suspend:   store i8 %call1, i8* %1, align 1, !dbg !8495
- value propagate via call: value as a function parameter
 - Example: ###8 cmos_suspend:   call void @rtc_update_irq(%struct.rtc_device* %10, i64 1, i64 %conv8.i) #9, !dbg !8561
- implicit control flow: value as a br target 
 - Example: ###7 cmos_suspend:   br i1 %or.cond49, label %if.then20, label %if.end27, !dbg !8626

Potential solution:
 - indirect call: function type match
 - store value: CCS18'
*/