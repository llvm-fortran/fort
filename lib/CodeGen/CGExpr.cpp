//===--- CGExpr.cpp - Emit LLVM Code from Expressions ---------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This contains code to emit Expr nodes as LLVM code.
//
//===----------------------------------------------------------------------===//

#include "CodeGenFunction.h"
#include "CodeGenModule.h"
#include "flang/AST/ASTContext.h"
#include "flang/AST/ExprVisitor.h"
#include "flang/Frontend/CodeGenOptions.h"
#include "llvm/ADT/Hashing.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/MDBuilder.h"

namespace flang {
namespace CodeGen {

/// CreateTempAlloca - This creates a alloca and inserts it into the entry
/// block.
llvm::AllocaInst *CodeGenFunction::CreateTempAlloca(llvm::Type *Ty,
                                                    const llvm::Twine &Name) {
  return new llvm::AllocaInst(Ty, 0, Name, AllocaInsertPt);
}

RValueTy CodeGenFunction::EmitRValue(const Expr *E) {
  auto EType = E->getType();
  if(EType->isComplexType())
    return EmitComplexExpr(E);
  else if(EType->isCharacterType())
    return EmitCharacterExpr(E);
  else if(EType->isLogicalType())
    return EmitLogicalValueExpr(E);
  else
    return EmitScalarExpr(E);
}

class LValueExprEmitter
  : public ConstExprVisitor<LValueExprEmitter, LValueTy> {
  CodeGenFunction &CGF;
  CGBuilderTy &Builder;
  llvm::LLVMContext &VMContext;
public:

  LValueExprEmitter(CodeGenFunction &cgf);

  LValueTy VisitVarExpr(const VarExpr *E);
  LValueTy VisitReturnedValueExpr(const ReturnedValueExpr *E);
  LValueTy VisitArrayElementExpr(const ArrayElementExpr *E);
};

LValueExprEmitter::LValueExprEmitter(CodeGenFunction &cgf)
  : CGF(cgf), Builder(cgf.getBuilder()),
    VMContext(cgf.getLLVMContext()) {
}

LValueTy LValueExprEmitter::VisitVarExpr(const VarExpr *E) {
  return LValueTy(CGF.GetVarPtr(E->getVarDecl()));
}

LValueTy LValueExprEmitter::VisitReturnedValueExpr(const ReturnedValueExpr *E) {
  return LValueTy(CGF.GetRetVarPtr());
}

LValueTy LValueExprEmitter::VisitArrayElementExpr(const ArrayElementExpr *E) {
  return CGF.EmitArrayElementPtr(E->getTarget(), E->getSubscripts());
}

LValueTy CodeGenFunction::EmitLValue(const Expr *E) {
  LValueExprEmitter EV(*this);
  return EV.Visit(E);
}

}
} // end namespace flang
