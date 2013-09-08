//===- SemaDecl.cpp - Declaration AST Builder and Semantic Analysis -------===//
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
#include "flang/Sema/SemaInternal.h"
#include "flang/AST/ASTContext.h"
#include "flang/AST/Decl.h"
#include "flang/AST/Expr.h"
#include "flang/AST/Stmt.h"
#include "flang/Basic/Diagnostic.h"

namespace flang {

bool Sema::CheckDeclaration(const IdentifierInfo *IDInfo, SourceLocation IDLoc) {
  if(!IDInfo) return false;
  if (auto Prev = LookupIdentifier(IDInfo)) {
    Diags.Report(IDLoc, diag::err_redefinition) << IDInfo;
    Diags.Report(Prev->getLocation(), diag::note_previous_definition);
    return false;
  }
  return true;
}

RecordDecl *Sema::ActOnDerivedTypeDecl(ASTContext &C, SourceLocation Loc,
                                       SourceLocation IDLoc,
                                       const IdentifierInfo* IDInfo) {
  RecordDecl* Record = RecordDecl::Create(C, CurContext, Loc, IDLoc, IDInfo);
  if(CheckDeclaration(IDInfo, IDLoc))
    CurContext->addDecl(Record);
  PushDeclContext(Record);
  return Record;
}

FieldDecl *Sema::ActOnDerivedTypeFieldDecl(ASTContext &C, DeclSpec &DS, SourceLocation IDLoc,
                                     const IdentifierInfo *IDInfo,
                                     ExprResult Init) {
  //FIXME: TODO same field name check
  //FIXME: TODO init expression

  QualType T = ActOnTypeName(C, DS);
  FieldDecl* Field = FieldDecl::Create(C, CurContext, IDLoc, IDInfo, T);
  CurContext->addDecl(Field);

  return Field;
}

void Sema::ActOnENDTYPE(ASTContext &C, SourceLocation Loc,
                        SourceLocation IDLoc, const IdentifierInfo* IDInfo) {
  auto Record = dyn_cast<RecordDecl>(CurContext);
  if(IDInfo && IDInfo != Record->getIdentifier()) {
    Diags.Report(IDLoc, diag::err_expected_type_name)
      << Record->getIdentifier() << /*Kind=*/ 0
      << getTokenRange(IDLoc);
  }
}

void Sema::ActOnEndDerivedTypeDecl(ASTContext &C) {
  PopDeclContext();
}

} // namespace flang
