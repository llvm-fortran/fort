//===- SemaFormat.cpp - FORMAT AST Builder and Semantic Analysis Implementation -===//
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

ExprResult Sema::ActOnFORMATDataEditDesc(ASTContext &C, SourceLocation Loc,
                                         tok::TokenKind Kind, ExprResult Repeat,
                                         ExprResult W, ExprResult MD,
                                         ExprResult E) {
  return ExprError();
}

ExprResult Sema::ActOnFORMATControlEditDesc(ASTContext &C, SourceLocation Loc,
                                            tok::TokenKind Kind) {
  return ExprError();
}

ExprResult Sema::ActOnFORMATPositionEditDesc(ASTContext &C, SourceLocation Loc,
                                             tok::TokenKind Kind, ExprResult N) {
  return ExprError();
}

} // end namespace flang
