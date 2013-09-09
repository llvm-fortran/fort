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

bool Sema::ActOnAttrSpec(SourceLocation Loc, DeclSpec &DS, DeclSpec::AS Val) {
  if (DS.hasAttributeSpec(Val)) {
    Diags.Report(Loc, diag::err_duplicate_attr_spec)
      << DeclSpec::getSpecifierName(Val);
    return true;
  }
  DS.setAttributeSpec(Val);
  return false;
}

bool Sema::ActOnDimensionAttrSpec(ASTContext &C, SourceLocation Loc,
                                  DeclSpec &DS,
                                  ArrayRef<ArraySpec*> Dimensions) {
  if (DS.hasAttributeSpec(DeclSpec::AS_dimension)) {
    Diags.Report(Loc, diag::err_duplicate_attr_spec)
      << DeclSpec::getSpecifierName(DeclSpec::AS_dimension);
    return true;
  }
  DS.setAttributeSpec(DeclSpec::AS_dimension);
  DS.setDimensions(Dimensions);
  return false;
}

bool Sema::ActOnAccessSpec(SourceLocation Loc, DeclSpec &DS, DeclSpec::AC Val) {
  if (DS.hasAccessSpec(Val)) {
    Diags.Report(Loc, diag::err_duplicate_access_spec)
      << DeclSpec::getSpecifierName(Val);
    return true;
  }
  DS.setAccessSpec(Val);
  return false;
}

bool Sema::ActOnIntentSpec(SourceLocation Loc, DeclSpec &DS, DeclSpec::IS Val) {
  if (DS.hasIntentSpec(Val)) {
    Diags.Report(Loc, diag::err_duplicate_intent_spec)
      << DeclSpec::getSpecifierName(Val);
    return true;
  }
  DS.setIntentSpec(Val);
  return false;
}

bool Sema::ActOnObjectArraySpec(ASTContext &C, SourceLocation Loc,
                                DeclSpec &DS,
                                ArrayRef<ArraySpec*> Dimensions) {
  if(!DS.hasAttributeSpec(DeclSpec::AS_dimension))
    DS.setAttributeSpec(DeclSpec::AS_dimension);
  DS.setDimensions(Dimensions);
  return false;
}

void Sema::ActOnTypeDeclSpec(ASTContext &C, SourceLocation Loc,
                             const IdentifierInfo *IDInfo, DeclSpec &DS) {
  DS.SetTypeSpecType(DeclSpec::TST_struct);
  auto D = ResolveIdentifier(IDInfo);
  if(!D) {
    Diags.Report(Loc, diag::err_undeclared_var_use)
      << IDInfo;
    return;
  }
  auto Record = dyn_cast<RecordDecl>(D);
  if(!Record) {
    Diags.Report(Loc, diag::err_use_of_not_typename)
      << IDInfo;
    return;
  }
  DS.setRecord(Record);
}

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
  auto Record = RecordDecl::Create(C, CurContext, Loc, IDLoc, IDInfo);
  if(CheckDeclaration(IDInfo, IDLoc))
    CurContext->addDecl(Record);
  PushDeclContext(Record);
  return Record;
}

void Sema::ActOnDerivedTypeSequenceStmt(ASTContext &C, SourceLocation Loc) {
  cast<RecordDecl>(CurContext)->setIsSequence(true);
}

FieldDecl *Sema::ActOnDerivedTypeFieldDecl(ASTContext &C, DeclSpec &DS, SourceLocation IDLoc,
                                           const IdentifierInfo *IDInfo,
                                           ExprResult Init) {
  QualType T = ActOnTypeName(C, DS);
  if(auto ArrTy = T->asArrayType()) {
    // FIXME: check deferred shape when pointer is used.
    for(auto Dim : ArrTy->getDimensions()) {
      if(!isa<ExplicitShapeSpec>(Dim)) {
        Diags.Report(IDLoc, diag::err_invalid_type_field_array_shape);
        break;
      }
    }
  }

  FieldDecl* Field = FieldDecl::Create(C, CurContext, IDLoc, IDInfo, T);

  if (auto Prev = LookupIdentifier(IDInfo)) {
    Diags.Report(IDLoc, diag::err_duplicate_member) << IDInfo;
    Diags.Report(Prev->getLocation(), diag::note_previous_declaration);
  } else
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
