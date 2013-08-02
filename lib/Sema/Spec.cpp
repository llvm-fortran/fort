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

void Sema::ActOnFunctionSpecificationPart() {
  /// If necessary, apply the implicit typing rules to the current function and its arguments.
  if(auto FD = dyn_cast<FunctionDecl>(CurContext)) {
    // function type
    if(FD->isNormalFunction() || FD->isStatementFunction()) {
      if(FD->getType().isNull()) {
        auto Type = ResolveImplicitType(FD->getIdentifier());
        if(Type.isNull()) {
          Diags.Report(FD->getLocation(), diag::err_func_no_implicit_type)
            << FD->getIdentifier();
          // FIXME: add note implicit none was applied here.
        }
        else SetFunctionType(FD, Type, FD->getLocation(), SourceRange()); //FIXME: proper loc and range
      }
    }

    // arguments
    for(auto Arg : FD->getArguments()) {
      if(Arg->getType().isNull()) {
        ApplyImplicitRulesToArgument(Arg);
      }
    }
  }
}

/// Applies the specification statements to the declarations.
void Sema::ActOnSpecificationPart() {
  ActOnFunctionSpecificationPart();
  auto Body = getCurrentBody()->getDeclStatements();

  for(ArrayRef<Stmt*>::iterator I = Body.begin(), End = Body.end();
      I != End; ++I) {
    ArrayRef<Stmt*> StmtList;

    if (const CompoundStmt *BundledStmt = dyn_cast<CompoundStmt>(*I))
      StmtList = BundledStmt->getBody();
    else StmtList = ArrayRef<Stmt*>(*I);
    for(auto S : StmtList) {
      if (const DimensionStmt *DimStmt = dyn_cast<DimensionStmt>(S)){
        ApplySpecification(DimStmt);
      }
    }
  }
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
  else {
    auto T = ActOnArraySpec(Context, VD->getType(), Stmt->getIDList());
    VD->setType(T);
    if(T->isArrayType()) {
      CheckArrayTypeDeclarationCompability(T->asArrayType(), VD);
      VD->MarkUsedAsVariable(Stmt->getLocation());
    }
  }
  return false;
}

} // namespace flang
