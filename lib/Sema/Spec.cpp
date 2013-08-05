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

    auto StmtLoc = (*I)->getLocation();
    if (const CompoundStmt *BundledStmt = dyn_cast<CompoundStmt>(*I)) {
      StmtList = BundledStmt->getBody();
      StmtLoc = BundledStmt->getLocation();
    }
    else StmtList = ArrayRef<Stmt*>(*I);
    for(auto S : StmtList) {
      if (const DimensionStmt *DimStmt = dyn_cast<DimensionStmt>(S)){
        ApplySpecification(StmtLoc, DimStmt);
      }
      else if(const SaveStmt *SavStmt = dyn_cast<SaveStmt>(S)) {
        ApplySpecification(StmtLoc, SavStmt);
      }
    }
  }
}

VarDecl *Sema::GetVariableForSpecification(SourceLocation StmtLoc,
                                           const IdentifierInfo *IDInfo,
                                           SourceLocation IDLoc,
                                           bool CanBeArgument) {
  auto IdRange = getIdentifierRange(IDLoc, IDInfo);
  auto Declaration = LookupIdentifier(IDInfo);
  if(Declaration) {
    auto VD = dyn_cast<VarDecl>(Declaration);
    if(VD && !(VD->isParameter() || (!CanBeArgument && VD->isArgument())))
      return VD;
    Diags.Report(StmtLoc, CanBeArgument? diag::err_spec_requires_local_var_arg : diag::err_spec_requires_local_var)
      << IDInfo << IdRange;
    if(VD) {
      Diags.Report(Declaration->getLocation(), diag::note_previous_definition_kind)
          << IDInfo << (VD->isArgument()? 0 : 1) << getIdentifierRange(Declaration->getLocation(), VD->getIdentifier());
    } else
      Diags.Report(Declaration->getLocation(), diag::note_previous_definition);
  } else {
    Diags.Report(IDLoc, diag::err_undeclared_var_use)
     << IDInfo << IdRange;
  }

  return nullptr;
}

bool Sema::ApplySpecification(SourceLocation StmtLoc, const DimensionStmt *Stmt) {
  auto VD = GetVariableForSpecification(StmtLoc,
                                        Stmt->getVariableName(),
                                        Stmt->getLocation());
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

bool Sema::ApplySpecification(SourceLocation StmtLoc, const SaveStmt *S) {
  if(!S->getIdentifier()) {
    for(DeclContext::decl_iterator I = CurContext->decls_begin(),
        End = CurContext->decls_end(); I != End; ++I) {
      auto VD = dyn_cast<VarDecl>(*I);
      if(VD && !(VD->isParameter() || VD->isArgument())) {
        ApplySpecification(StmtLoc, S, VD);
      }
    }
    return false;
  }
  auto VD = GetVariableForSpecification(StmtLoc,
                                        S->getIdentifier(),
                                        S->getLocation(),
                                        false);
  if(!VD) return true;
  return ApplySpecification(StmtLoc, S, VD);
}

bool Sema::ApplySpecification(SourceLocation StmtLoc, const SaveStmt *S, VarDecl *VD) {
  auto Type = VD->getType();
  auto Ext = Type.getExtQualsPtrOrNull();
  Qualifiers Quals;
  if(Ext){
    Quals = Ext->getQualifiers();
    if(Quals.hasAttributeSpec(Qualifiers::AS_save)) {
      if(S->getIdentifier()) {
        Diags.Report(StmtLoc, diag::err_spec_qual_reapplication)
          << "save" << VD->getIdentifier() << getIdentifierRange(S->getLocation(), S->getIdentifier());
      } else {
        Diags.Report(StmtLoc, diag::err_spec_qual_reapplication)
          << "save" << VD->getIdentifier();
      }
      return true;
    }
  }
  Quals.addAttributeSpecs(Qualifiers::AS_save);
  VD->setType(Context.getQualifiedType(Type, Quals));
  return false;
}

} // namespace flang
