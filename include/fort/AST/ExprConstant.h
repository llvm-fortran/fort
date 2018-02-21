//===--- ExprConstant.h - Expression Constant Evaluator -------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef FORT_AST_EXPR_CONSTANT_H
#define FORT_AST_EXPR_CONSTANT_H

#include "fort/Basic/LLVM.h"
#include "llvm/ADT/DenseMap.h"

namespace fort {

class Expr;
class VarDecl;
class ASTContext;

/// ExprEvalScope - represents a scope which can be used
/// to associate variables with values when evaluating expressions.
class ExprEvalScope {
  llvm::SmallDenseMap<const VarDecl *, int64_t, 16> InlinedVars;
  ASTContext &Context;

public:
  ExprEvalScope(ASTContext &C);

  std::pair<int64_t, bool> get(const Expr *E) const;
  void Assign(const VarDecl *Var, int64_t Value);
};

} // namespace fort

#endif
