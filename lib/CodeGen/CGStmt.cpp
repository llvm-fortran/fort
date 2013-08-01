//===--- CGStmt.cpp - Emit LLVM Code from Statements ----------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This contains code to emit Stmt nodes as LLVM code.
//
//===----------------------------------------------------------------------===//

#include "CodeGenFunction.h"
#include "CodeGenModule.h"
#include "CGIORuntime.h"
#include "flang/AST/StmtVisitor.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/Support/CallSite.h"

namespace flang {
namespace CodeGen {

void CodeGenFunction::EmitStmt(const Stmt *S) {
  class Visitor : public ConstStmtVisitor<Visitor> {
  public:
    CodeGenFunction *CG;

    Visitor(CodeGenFunction *P) : CG(P) {}

    void VisitCompoundStmt(const CompoundStmt *S) {
      for(auto I : S->getBody())
        CG->EmitStmt(I);
    }
    void VisitDataStmt(const DataStmt *S) {
      CG->EmitStmt(S->getBody());
    }
    void VisitBlockStmt(const BlockStmt *S) {
      for(auto I : S->getStatements())
        CG->EmitStmt(I);
    }
    void VisitGotoStmt(const GotoStmt *S) {
      CG->EmitGotoStmt(S);
    }
    void VisitAssignStmt(const AssignStmt *S) {
      CG->EmitAssignStmt(S);
    }
    void VisitAssignedGotoStmt(const AssignedGotoStmt *S) {
      CG->EmitAssignedGotoStmt(S);
    }
    void VisitIfStmt(const IfStmt *S) {
      CG->EmitIfStmt(S);
    }
    void VisitDoStmt(const DoStmt *S) {
      CG->EmitDoStmt(S);
    }
    void VisitDoWhileStmt(const DoWhileStmt *S) {
      CG->EmitDoWhileStmt(S);
    }
    void VisitCycleStmt(const CycleStmt *S) {
      CG->EmitCycleStmt(S);
    }
    void VisitExitStmt(const ExitStmt *S) {
      CG->EmitExitStmt(S);
    }
    void VisitStopStmt(const StopStmt *S) {
      CG->EmitStopStmt(S);
    }
    void VisitReturnStmt(const ReturnStmt *S) {
      CG->EmitReturnStmt(S);
    }
    void VisitCallStmt(const CallStmt *S) {
      CG->EmitCallStmt(S);
    }
    void VisitAssignmentStmt(const AssignmentStmt *S) {
      CG->EmitAssignmentStmt(S);
    }
    void VisitWriteStmt(const WriteStmt *S) {
      CG->getModule().getIORuntime().EmitWriteStmt(*CG, S);
    }
    void VisitPrintStmt(const PrintStmt *S) {
      CG->getModule().getIORuntime().EmitPrintStmt(*CG, S);
    }
  };
  Visitor SV(this);

  if(S->getStmtLabel())
    EmitStmtLabel(S);
  SV.Visit(S);
}

void CodeGenFunction::EmitBlock(llvm::BasicBlock *BB) {
  auto CurBB = Builder.GetInsertBlock();
  EmitBranch(BB);
  // Place the block after the current block, if possible, or else at
  // the end of the function.
  if (CurBB && CurBB->getParent())
    CurFn->getBasicBlockList().insertAfter(CurBB, BB);
  else
    CurFn->getBasicBlockList().push_back(BB);
  Builder.SetInsertPoint(BB);
}

void CodeGenFunction::EmitBranch(llvm::BasicBlock *Target) {
  // Emit a branch from the current block to the target one if this
  // was a real block.  If this was just a fall-through block after a
  // terminator, don't emit it.
  llvm::BasicBlock *CurBB = Builder.GetInsertBlock();

  if (!CurBB || CurBB->getTerminator()) {
    // If there is no insert point or the previous block is already
    // terminated, don't touch it.
  } else {
    // Otherwise, create a fall-through branch.
    Builder.CreateBr(Target);
  }

  Builder.ClearInsertionPoint();
}

void CodeGenFunction::EmitBranchOnLogicalExpr(const Expr *Condition,
                                              llvm::BasicBlock *ThenBB,
                                              llvm::BasicBlock *ElseBB) {
  auto CV = EmitLogicalConditionExpr(Condition);
  Builder.CreateCondBr(CV, ThenBB, ElseBB);
}

void CodeGenFunction::EmitStmtLabel(const Stmt *S) {
  if(!S->isStmtLabelUsedAsGotoTarget())
    return;
  if(S->isStmtLabelUsedAsAssignTarget())
    AssignedGotoTargets.push_back(S);

  auto AlreadyCreated = GotoTargets.find(S);
  if(AlreadyCreated != GotoTargets.end()) {
    auto Block = AlreadyCreated->second;
    EmitBlock(Block);
    return;
  }

  auto Block = createBasicBlock("");
  EmitBlock(Block);
  GotoTargets.insert(std::make_pair(S, Block));
}

void CodeGenFunction::EmitGotoStmt(const GotoStmt *S) {
  auto Dest = S->getDestination().Statement;
  auto Result = GotoTargets.find(Dest);
  if(Result != GotoTargets.end()) {
    Builder.CreateBr(Result->second);
    return;
  }

  auto Block = createBasicBlock("");
  Builder.CreateBr(Block);
  GotoTargets.insert(std::make_pair(Dest, Block));
  EmitBlock(createBasicBlock("next"));
}

void CodeGenFunction::EmitAssignStmt(const AssignStmt *S) {
  auto Val = EmitScalarExpr(S->getAddress().Statement->getStmtLabel());
  // FIXME: verify that destination type can actually hold the value of a statement label
  EmitAssignment(EmitLValue(S->getDestination()),
                   EmitScalarToScalarConversion(
                     Val, S->getDestination()->getType()));
}

void CodeGenFunction::EmitAssignedGotoStmt(const AssignedGotoStmt *S) {
  if(!AssignedGotoVarPtr)
    AssignedGotoVarPtr = CreateTempAlloca(CGM.Int64Ty, "assigned-goto-val");
  auto Val = Builder.CreateZExtOrTrunc(EmitScalarExpr(S->getDestination()),
                                       CGM.Int64Ty);
  Builder.CreateStore(Val, AssignedGotoVarPtr);
  if(!AssignedGotoDispatchBlock)
    AssignedGotoDispatchBlock = createBasicBlock("assigned-goto-dispatch");
  Builder.CreateBr(AssignedGotoDispatchBlock);
  EmitBlock(createBasicBlock("assigned-goto-after"));
}

void CodeGenFunction::EmitAssignedGotoDispatcher() {
  assert(AssignedGotoDispatchBlock);
  EmitBlock(AssignedGotoDispatchBlock);
  auto DefaultCase = createBasicBlock("assigned-goto-dispatch-failed");
  auto VarVal = Builder.CreateLoad(AssignedGotoVarPtr);
  auto Switch = Builder.CreateSwitch(VarVal, DefaultCase,
                                     AssignedGotoTargets.size());
  for(size_t I = 0; I < AssignedGotoTargets.size(); ++I) {
    auto Target = AssignedGotoTargets[I];
    auto Dest = GotoTargets[Target];
    auto Val = llvm::ConstantInt::get(VarVal->getType(),
                                      cast<IntegerConstantExpr>(Target->getStmtLabel())->getValue());
    Switch->addCase(cast<llvm::ConstantInt>(Val), Dest);
  }
  EmitBlock(DefaultCase);
  // FiXME raise error
  Builder.CreateUnreachable();
}

void CodeGenFunction::EmitIfStmt(const IfStmt *S) {
  auto ElseStmt = S->getElseStmt();
  auto ThenArm = createBasicBlock("if-then");
  auto MergeBlock = createBasicBlock("end-if");
  auto ElseArm = ElseStmt? createBasicBlock("if-else") :
                           MergeBlock;

  EmitBranchOnLogicalExpr(S->getCondition(), ThenArm, ElseArm);
  EmitBlock(ThenArm);
  EmitStmt(S->getThenStmt());
  EmitBranch(MergeBlock);
  if(ElseStmt) {
    EmitBlock(ElseArm);
    EmitStmt(S->getElseStmt());
  }
  EmitBlock(MergeBlock);
}

class LoopScope {
public:
  CodeGenFunction *CGF;
  const LoopScope *Previous;
  const Stmt *Loop;
  llvm::BasicBlock *ContinueTarget;
  llvm::BasicBlock *BreakTarget;

  LoopScope(CodeGenFunction *cgf,
            const Stmt *S,
            llvm::BasicBlock *ContinueBB,
            llvm::BasicBlock *BreakBB)
    : CGF(cgf), Previous(cgf->CurLoopScope), Loop(S),
      ContinueTarget(ContinueBB), BreakTarget(BreakBB) {
      cgf->CurLoopScope = this;
    }
  ~LoopScope() {
    CGF->CurLoopScope = Previous;
  }
  const LoopScope *getScope(const Stmt *S) const {
    if(Loop == S) return this;
    return Previous->getScope(S);
  }
};

void CodeGenFunction::EmitDoStmt(const DoStmt *S) {
  // Init
  auto VarPtr = GetVarPtr(cast<VarExpr>(S->getDoVar())->getVarDecl());
  auto InitValue = EmitScalarExpr(S->getInitialParameter());
  Builder.CreateStore(InitValue, VarPtr);  
  auto EndValue = EmitScalarExpr(S->getTerminalParameter());
  llvm::Value *IncValue;
  bool UseIterationCount = true;
  if(S->getIncrementationParameter())
    IncValue = EmitScalarExpr(S->getIncrementationParameter());
  else {
    IncValue = GetConstantOne(S->getDoVar()->getType());
    if(S->getDoVar()->getType()->isIntegerType())
      UseIterationCount = false;
  }

  auto Loop = createBasicBlock("do");
  auto LoopBody = createBasicBlock("loop");
  auto LoopIncrement = createBasicBlock("loop-inc");
  auto EndLoop = createBasicBlock("end-do");

  // IterationCount = MAX( INT( (m2 - m1 + m3)/m3), 0)
  llvm::Value *IterationCountVar;
  auto Zero = llvm::ConstantInt::get(CGM.SizeTy, 0);
  if(UseIterationCount) {
    IterationCountVar = CreateTempAlloca(CGM.SizeTy,
                                         "iteration-count");
    auto Val = EmitScalarBinaryExpr(BinaryExpr::Minus,
                                    EndValue, InitValue);
    Val = EmitScalarBinaryExpr(BinaryExpr::Plus,
                               Val, IncValue);
    Val = EmitScalarBinaryExpr(BinaryExpr::Divide,
                               Val, IncValue);
    if(Val->getType()->isFloatingPointTy())
      Val = Builder.CreateFPToSI(Val, CGM.SizeTy);
    else
      Val = Builder.CreateSExtOrTrunc(Val, CGM.SizeTy);
    Val = Builder.CreateSelect(Builder.CreateICmpSGE(Val, Zero),
                               Val, Zero, "max");
    Builder.CreateStore(Val, IterationCountVar);
  } else {
    // DO i = -1, -5 => IterationCount is 0 => don't run
    auto Cond = EmitScalarRelationalExpr(
                  BinaryExpr::GreaterThanEqual,
                  EndValue, InitValue);
    Builder.CreateCondBr(Cond, Loop, EndLoop);
  }

  LoopScope Scope(this, S, LoopIncrement, EndLoop);

  EmitBlock(Loop);
  // Check condition
  llvm::Value *Cond;
  if(UseIterationCount)
    Cond = Builder.CreateICmpNE(Builder.CreateLoad(IterationCountVar),
                                Zero);
  else
    Cond = EmitScalarRelationalExpr(
             BinaryExpr::LessThanEqual,
             Builder.CreateLoad(VarPtr), EndValue);
  Builder.CreateCondBr(Cond, LoopBody, EndLoop);

  EmitBlock(LoopBody);
  EmitStmt(S->getBody());

  EmitBlock(LoopIncrement);
  // increment the do variable
  llvm::Value *CurVal = Builder.CreateLoad(VarPtr);
  CurVal = EmitScalarBinaryExpr(BinaryExpr::Plus,
                                CurVal, IncValue);
  Builder.CreateStore(CurVal, VarPtr);
  // decrement loop counter if its used.
  if(UseIterationCount) {
    Builder.CreateStore(Builder.CreateSub(
                        Builder.CreateLoad(IterationCountVar),
                        llvm::ConstantInt::get(CGM.SizeTy, 1)), IterationCountVar);
  }

  EmitBranch(Loop);
  EmitBlock(EndLoop);
}

void CodeGenFunction::EmitDoWhileStmt(const DoWhileStmt *S) {
  auto Loop = createBasicBlock("do-while");
  auto LoopBody = createBasicBlock("loop");
  auto EndLoop = createBasicBlock("end-do-while");

  LoopScope Scope(this, S, Loop, EndLoop);

  EmitBlock(Loop);
  EmitBranchOnLogicalExpr(S->getCondition(), LoopBody, EndLoop);
  EmitBlock(LoopBody);
  EmitStmt(S->getBody());
  EmitBranch(Loop);
  EmitBlock(EndLoop);
}

void CodeGenFunction::EmitCycleStmt(const CycleStmt *S) {
  EmitBranch(CurLoopScope->getScope(S->getLoop())->ContinueTarget);
  EmitBlock(createBasicBlock("after-cycle"));
}

void CodeGenFunction::EmitExitStmt(const ExitStmt *S) {
  EmitBranch(CurLoopScope->getScope(S->getLoop())->BreakTarget);
  EmitBlock(createBasicBlock("after-exit"));
}

void CodeGenFunction::EmitStopStmt(const StopStmt *S) {
  EmitRuntimeCall(CGM.GetRuntimeFunction("stop", ArrayRef<llvm::Type*>()));
}

void CodeGenFunction::EmitReturnStmt(const ReturnStmt *S) {
  Builder.CreateBr(ReturnBlock);
}

void CodeGenFunction::EmitCallStmt(const CallStmt *S) {
  CallArgList ArgList;
  EmitCall(S->getFunction(), ArgList, S->getArguments(), true);
}

void CodeGenFunction::EmitAssignmentStmt(const AssignmentStmt *S) {
  auto RHS = S->getRHS();
  auto RHSType = RHS->getType();

  if(RHSType->isArrayType())
    EmitArrayAssignment(S->getLHS(), S->getRHS());
  auto Destination = EmitLValue(S->getLHS());

  if(RHSType->isIntegerType() || RHSType->isRealType()) {
    auto Value = EmitScalarExpr(RHS);
    Builder.CreateStore(Value, Destination.getPointer());
  } else if(RHSType->isLogicalType()) {
    auto Value = EmitLogicalValueExpr(RHS);
    Builder.CreateStore(Value, Destination.getPointer());
  } else if(RHSType->isComplexType()) {
    auto Value = EmitComplexExpr(RHS);
    EmitComplexStore(Value, Destination.getPointer());
  } else if(RHSType->isCharacterType())
    EmitCharacterAssignment(S->getLHS(), S->getRHS());
}

void CodeGenFunction::EmitAssignment(LValueTy LHS, RValueTy RHS) {
  if(RHS.isScalar())
    Builder.CreateStore(RHS.asScalar(), LHS.getPointer());
  else if(RHS.isComplex())
    EmitComplexStore(RHS.asComplex(), LHS.getPointer());
  else if(RHS.isCharacter())
    EmitCharacterAssignment(GetCharacterValueFromPtr(LHS.getPointer(), LHS.getType()),
                            RHS.asCharacter());
}

}
} // end namespace flang
