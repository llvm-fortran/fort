//===--- CGExprCharacter.cpp - Emit LLVM Code for Character Exprs --------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This contains code to emit Expr nodes with character types as LLVM code.
//
//===----------------------------------------------------------------------===//

#include "CodeGenFunction.h"
#include "CodeGenModule.h"
#include "flang/AST/ASTContext.h"
#include "flang/AST/ExprVisitor.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"

namespace flang {
namespace CodeGen {

class CharacterExprEmitter
  : public ConstExprVisitor<CharacterExprEmitter, CharacterValueTy> {
  CodeGenFunction &CGF;
  CGBuilderTy &Builder;
  llvm::LLVMContext &VMContext;
public:

  CharacterExprEmitter(CodeGenFunction &cgf);

  CharacterValueTy EmitExpr(const Expr *E);
  CharacterValueTy EmitCharacterConstantExpr(const CharacterConstantExpr *E);
  CharacterValueTy EmitSubstringExpr(const SubstringExpr *E);
};

CharacterExprEmitter::CharacterExprEmitter(CodeGenFunction &cgf)
  : CGF(cgf), Builder(cgf.getBuilder()),
    VMContext(cgf.getLLVMContext()) {
}

CharacterValueTy CharacterExprEmitter::EmitExpr(const Expr *E) {
  return Visit(E);
}

CharacterValueTy CharacterExprEmitter::EmitCharacterConstantExpr(const CharacterConstantExpr *E) {
  return CharacterValueTy(Builder.CreateGlobalStringPtr(E->getValue()),
                          llvm::ConstantInt::get(CGF.getModule().SizeTy,
                                                 1)); // FIXME: length
}

CharacterValueTy CharacterExprEmitter::EmitSubstringExpr(const SubstringExpr *E) {
  auto Str = EmitExpr(E->getTarget());
  return Str;//FIXME
}


}
} // end namespace flang
