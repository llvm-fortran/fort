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

    void VisitBlockStmt(const BlockStmt *S) {
      for(auto I : S->getStatements())
        CG->EmitStmt(I);
    }
    void VisitGotoStmt(const GotoStmt *S) {
      CG->EmitGotoStmt(S);
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
    void VisitStopStmt(const StopStmt *S) {
      CG->EmitStopStmt(S);
    }
    void VisitReturnStmt(const ReturnStmt *S) {
      CG->EmitReturnStmt(S);
    }
    void VisitAssignmentStmt(const AssignmentStmt *S) {
      CG->EmitAssignmentStmt(S);
    }
  };
  Visitor SV(this);

  if(S->getStmtLabel())
    EmitStmtLabel(S);
  SV.Visit(S);
}

// FIXME: create blocks only when used.
void CodeGenFunction::EmitStmtLabel(const Stmt *S) {
  auto AlreadyCreated = GotoTargets.find(S);
  if(AlreadyCreated != GotoTargets.end()) {
    auto Block = AlreadyCreated->second;
    Block->moveAfter(Builder.GetInsertBlock());
    Builder.SetInsertPoint(Block);
    return;
  }

  auto Block = createBasicBlock("",getCurrentFunction());
  Builder.CreateBr(Block);
  Builder.SetInsertPoint(Block);
  GotoTargets.insert(std::make_pair(S, Block));
}

void CodeGenFunction::EmitGotoStmt(const GotoStmt *S) {
  auto Dest = S->getDestination().Statement;
  auto Result = GotoTargets.find(Dest);
  if(Result != GotoTargets.end()) {
    Builder.CreateBr(Result->second);
    return;
  }

  auto Block = createBasicBlock("",getCurrentFunction());
  Builder.CreateBr(Block);
  GotoTargets.insert(std::make_pair(Dest, Block));
}

void CodeGenFunction::EmitIfStmt(const IfStmt *S) {

}

void CodeGenFunction::EmitDoStmt(const DoStmt *S) {

}

void CodeGenFunction::EmitDoWhileStmt(const DoWhileStmt *S) {

}

void CodeGenFunction::EmitStopStmt(const StopStmt *S) {
  if(IsMainProgram) {
    Builder.CreateBr(ReturnBlock);
    return;
  }
  // FIXME: todo in functions;
}

void CodeGenFunction::EmitReturnStmt(const ReturnStmt *S) {
  Builder.CreateBr(ReturnBlock);
}

void CodeGenFunction::EmitAssignmentStmt(const AssignmentStmt *S) {
  auto RHS = S->getRHS();
  auto RHSType = RHS->getType();
  if(RHSType->isIntegerType() || RHSType->isRealType() ||
     RHSType->isLogicalType()) {
    EmitScalarRValue(RHS);
  }
}

}
} // end namespace flang
