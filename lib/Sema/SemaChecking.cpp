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

unsigned Sema::EvalAndCheckCharacterLength(const Expr *E) {
  auto Type = E->getType();
  if(Type.isNull() || !Type->isIntegerType())
    goto error;
  int64_t Result;
  if(!E->EvaluateAsInt(Result, Context))
    goto error;
  if(Result < 1) {
    Diags.Report(E->getLocation(), diag::err_expected_integer_gt_0)
      << E->getSourceRange();
    return 1;
  }
  if(Result > int64_t(std::numeric_limits<unsigned>::max())) {
    //FIXME overflow diag
    Diags.Report(E->getLocation(), diag::err_expected_integer_constant_expr)
      << E->getSourceRange();
    return 1;
  }
  return Result;

error:
  Diags.Report(E->getLocation(), diag::err_expected_integer_constant_expr)
    << E->getSourceRange();
  return 1;
}

} // end namespace flang
