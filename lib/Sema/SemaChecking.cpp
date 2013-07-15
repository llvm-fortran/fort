//===--- SemaChecking.cpp - Extra Semantic Checking -----------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "flang/Sema/Sema.h"
#include "flang/Sema/DeclSpec.h"
#include "flang/Sema/SemaDiagnostic.h"
#include "flang/AST/ASTContext.h"
#include "flang/AST/Decl.h"
#include "flang/AST/Expr.h"
#include "flang/Basic/Diagnostic.h"
#include "llvm/Support/raw_ostream.h"

namespace flang {

int64_t Sema::EvalAndCheckIntExpr(const Expr *E,
                                  int64_t ErrorValue) {
  auto Type = E->getType();
  if(Type.isNull() || !Type->isIntegerType())
    goto error;
  int64_t Result;
  if(!E->EvaluateAsInt(Result, Context))
    goto error;

  return Result;
error:
  Diags.Report(E->getLocation(), diag::err_expected_integer_constant_expr)
    << E->getSourceRange();
  return ErrorValue;
}

int64_t Sema::CheckIntGT0(const Expr *E, int64_t EvalResult,
                          int64_t ErrorValue) {
  if(EvalResult < 1) {
    Diags.Report(E->getLocation(), diag::err_expected_integer_gt_0)
      << E->getSourceRange();
    return ErrorValue;
  }
  return EvalResult;
}

BuiltinType::TypeKind Sema::EvalAndCheckTypeKind(QualType T,
                                                 const Expr *E) {
  auto Result = EvalAndCheckIntExpr(E, 4);
  switch(cast<BuiltinType>(T.getTypePtr())->getTypeSpec()) {
  case BuiltinType::Integer:
  case BuiltinType::Logical: {
    switch(Result) {
    case 1: return BuiltinType::Int1;
    case 2: return BuiltinType::Int2;
    case 4: return BuiltinType::Int4;
    case 8: return BuiltinType::Int8;
    }
    break;
  }
  case BuiltinType::Real:
  case BuiltinType::Complex: {
    switch(Result) {
    case 4: return BuiltinType::Real4;
    case 8: return BuiltinType::Real8;
    }
    break;
  }
  default:
    llvm_unreachable("invalid type kind");
  }
  Diags.Report(E->getLocation(), diag::err_invalid_kind_selector)
    << int(Result) << T << E->getSourceRange();
  return BuiltinType::NoKind;
}

unsigned Sema::EvalAndCheckCharacterLength(const Expr *E) {
  auto Result = CheckIntGT0(E, EvalAndCheckIntExpr(E, 1));
  if(Result > int64_t(std::numeric_limits<unsigned>::max())) {
    //FIXME overflow diag
    Diags.Report(E->getLocation(), diag::err_expected_integer_constant_expr)
      << E->getSourceRange();
    return 1;
  }
  return Result;
}

} // end namespace flang
