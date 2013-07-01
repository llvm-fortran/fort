//===--- Spec.cpp - AST Builder and Semantic Analysis Implementation ------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the application of Specification statements to the
// declarations.
//
//===----------------------------------------------------------------------===//

#include "flang/Sema/Sema.h"
#include "flang/Sema/DeclSpec.h"
#include "flang/Sema/SemaDiagnostic.h"
#include "flang/AST/ASTContext.h"
#include "flang/AST/Decl.h"
#include "flang/AST/Expr.h"
#include "flang/AST/Stmt.h"
#include "flang/Basic/Diagnostic.h"
#include "llvm/Support/raw_ostream.h"

namespace flang {

/// Applies the specification statements to the declarations.
void Sema::ActOnSpecificationPart(ArrayRef<StmtResult> Body) {
  for(ArrayRef<StmtResult>::iterator I = Body.begin(), End = Body.end();
      I != End; ++I) {
    if(!I->isUsable()) continue;

    ArrayRef<Stmt*> StmtList;

    if (const BundledCompoundStmt *BundledStmt = dyn_cast<BundledCompoundStmt>(I->get()))
      StmtList = BundledStmt->getBody();
    else StmtList = ArrayRef<Stmt*>(I->get());
    for(auto S : StmtList) {
      if (const DimensionStmt *DimStmt = dyn_cast<DimensionStmt>(S)){
        ApplySpecification(DimStmt);
      }
    }
  }

  /// If necessary, apply the implicit typing rules to the current function and its arguments.
  if(auto FD = dyn_cast<FunctionDecl>(CurContext)) {
    if(FD->getType().isNull()) {
      auto Type = ResolveImplicitType(FD->getIdentifier());
      if(Type.isNull()) {
        Diags.Report(FD->getLocation(), diag::err_func_no_implicit_type)
          << FD->getIdentifier();
        // FIXME: add note implicit none was applied here.
      }
      else FD->setType(Type);
    }
  }
  // FIXME: arg
}

VarDecl *Sema::GetVariableForSpecification(const IdentifierInfo *IDInfo,
                                           SourceLocation ErrorLoc,
                                           SourceRange ErrorRange,
                                           const char *DiagStmtType) {
  auto Declaration = LookupIdentifier(IDInfo);
  if(Declaration) {
    auto VD = dyn_cast<VarDecl>(Declaration);
    if(VD && !VD->isParameter()) return VD;
    Diags.Report(ErrorLoc, diag::err_spec_not_applicable_not_var)
     << DiagStmtType << IDInfo << ErrorRange;
    Diags.Report(Declaration->getLocation(), diag::note_previous_definition);
  } else {
    Diags.Report(ErrorLoc, diag::err_spec_not_applicable_undeclared_ident)
     << DiagStmtType << IDInfo << ErrorRange;
  }

  return nullptr;
}

bool Sema::ApplySpecification(const DimensionStmt *Stmt) {
  auto VD = GetVariableForSpecification(Stmt->getVariableName(),
                                        Stmt->getLocation(),
                                        Stmt->getSourceRange(),
                                        "DIMENSION");
  if(!VD) return true;
  if(isa<ArrayType>(VD->getType())) {
    Diags.Report(Stmt->getLocation(),
                 diag::err_spec_dimension_already_array)
      << Stmt->getVariableName() << Stmt->getSourceRange();
    return true;
  }
  else
    VD->setType(Context.getArrayType(VD->getType(), Stmt->getIDList()));
  return false;
}

} // namespace flang
