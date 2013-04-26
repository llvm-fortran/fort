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
#include "flang/AST/ASTContext.h"
#include "flang/AST/Decl.h"
#include "flang/AST/Expr.h"
#include "flang/AST/Stmt.h"
#include "flang/Basic/Diagnostic.h"
#include "llvm/Support/raw_ostream.h"

namespace flang {

/// Applies the specification statements to the declarations.
void Sema::ActOnSpecificationPart(ArrayRef<StmtResult> Body) {
  Stmt *Stmt;
  for(ArrayRef<StmtResult>::iterator I = Body.begin(), End = Body.end();
      I != End; ++I) {
    if(!I->isUsable()) continue;
    Stmt = I->get();

    if (const DimensionStmt *DimStmt = dyn_cast<DimensionStmt>(Stmt)){
      ApplySpecification(DimStmt);
    }
  }
}

VarDecl *Sema::GetVariableForSpecification(const IdentifierInfo *IDInfo,
                                           SMLoc ErrorLoc,
                                           const llvm::Twine &ErrorMsg) {
  DeclContext::lookup_result LRes = CurContext->lookup(IDInfo);
  for(; LRes.first != LRes.second; ++LRes.first){
    // FIXME: multiple declarations?
    if(VarDecl *VD = dyn_cast<VarDecl>(*LRes.first)){
      return VD;
    }
  }
  Diags.ReportError(ErrorLoc,ErrorMsg +
                    " can't be applied because the variable '" +
                    IDInfo->getName() +
                    "' isn't declared in the current context!");
  return 0;
}

bool Sema::ApplySpecification(const DimensionStmt *Stmt) {
  VarDecl *VD = GetVariableForSpecification(Stmt->getVariableName(),
                                            Stmt->getLocation(),
                                            "DIMENSION specification");
  if(!VD) return true;
  if(isa<ArrayType>(VD->getType())){
    Diags.ReportError(Stmt->getLocation(),
                llvm::Twine("DIMENSION specification can't be applied to the variable '") +
                VD->getName() + "' because it already has DIMENSION specifier!");
    return true;
  }
  else
    VD->setType(Context.getArrayType(VD->getType(), Stmt->getIDList()));
  return false;
}

} // namespace flang
