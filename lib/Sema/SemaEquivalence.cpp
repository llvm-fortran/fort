// SemaEquivalence.cpp - AST Builder and Checker for the EQUIVALENCE statement //
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the checking and AST construction for the EQUIVALENCE
// statement.
//
//===----------------------------------------------------------------------===//

#include "flang/Sema/Sema.h"
#include "flang/Sema/DeclSpec.h"
#include "flang/Sema/SemaDiagnostic.h"
#include "flang/AST/ASTContext.h"
#include "flang/AST/Decl.h"
#include "flang/AST/Expr.h"
#include "flang/AST/Stmt.h"
#include "flang/AST/ExprVisitor.h"
#include "flang/Basic/Diagnostic.h"

namespace flang {

EquivalenceScope::InfluenceObject *EquivalenceScope::GetObject(ASTContext &C, VarDecl *Var) {
  auto Result = Objects.find(Var);
  if(Result != Objects.end())
    return Result->second;

  auto Obj = new(C) EquivalenceScope::InfluenceObject;
  Obj->Var = Var;
  Objects.insert(std::make_pair((const VarDecl*) Var, Obj));
  return Obj;
}

EquivalenceScope::Object EquivalenceScope::GetObject(ASTContext &C, const Expr *E, VarDecl *Var, uint64_t Offset) {
  return Object(E, Offset, GetObject(C, Var));
}

void EquivalenceScope::Connect(Object A, Object B) {
  Connections.push_back(Connection(A, B));
}

class ConnectionFinder {
  ArrayRef<EquivalenceScope::Connection> Connections;
  SmallVector<bool, 16> Visited;
  SmallVector<EquivalenceScope::Connection, 8> Result;
public:
  ConnectionFinder(ArrayRef<EquivalenceScope::Connection> connections);

  void FindRelevantConnections(EquivalenceScope::Object Point,
                               EquivalenceScope::Object Target);

  ArrayRef<EquivalenceScope::Connection> getResults() const {
    return Result;
  }
};

ConnectionFinder::ConnectionFinder(ArrayRef<EquivalenceScope::Connection> connections)
  : Connections(connections) {
  Visited.resize(Connections.size());
  for(auto &I: Visited) I = false;
}

void ConnectionFinder::FindRelevantConnections(EquivalenceScope::Object Point,
                                               EquivalenceScope::Object Target) {
  for(size_t I = 0; I < Connections.size(); ++I) {
    if(Visited[I]) continue;
    Visited[I] = true;

    // given (a - b), found (a - x) or (x - a)
    if(Connections[I].A.Obj == Point.Obj ||
       Connections[I].B.Obj == Point.Obj) {
      auto Other = Connections[I].A.Obj == Point.Obj? Connections[I].B :
                                                      Connections[I].A;

      // found (a - b) or (b - a)
      if(Other.Obj == Target.Obj) {
        Result.push_back(Connections[I]);
        continue;
      }

      // search for connections in the other object.
      FindRelevantConnections(Other, Target);
    }
  }
}

bool EquivalenceScope::CheckConnection(DiagnosticsEngine &Diags, Object A, Object B) {
  // equivalence (x,x)
  if(A.Obj == B.Obj) {
    Diags.Report(B.E->getLocation(), diag::warn_equivalence_same_object)
      << A.E->getSourceRange()
      << B.E->getSourceRange();
  }
  //
  ConnectionFinder Finder(Connections);
  Finder.FindRelevantConnections(A, B);
  auto Relevant = Finder.getResults();
  if (!Relevant.empty()) {
    Diags.Report(B.E->getLocation(), diag::warn_equivalence_redundant)
      << A.E->getSourceRange()
      << B.E->getSourceRange();
    auto I = Relevant.front();
    Diags.Report(I.A.E->getLocation(), diag::note_equivalence_identical_association)
      << I.A.E->getSourceRange()
      << I.B.E->getSourceRange();
  }
  return true;
}

bool Sema::CheckEquivalenceObject(SourceLocation Loc, Expr *E, VarDecl *& Object) {
  if(auto Var = dyn_cast<VarExpr>(E)) {
    auto VD = Var->getVarDecl();
    if(VD->isArgument() || VD->isParameter()) {
      Diags.Report(Loc, diag::err_spec_requires_local_var)
        << E->getSourceRange();
      Diags.Report(VD->getLocation(), diag::note_previous_definition_kind)
          << VD->getIdentifier() << (VD->isArgument()? 0 : 1)
          << getTokenRange(VD->getLocation());
      return true;
    }
    if(VD->isUnusedSymbol())
      const_cast<VarDecl*>(VD)->MarkUsedAsVariable(E->getLocation());
    Object = const_cast<VarDecl*>(VD);
  }  else {
    Diags.Report(Loc, diag::err_spec_requires_var_or_arr_el)
      << E->getSourceRange();
    return true;
  }
  return false;
}

// FIXME: add support for derived types.
// FIXME: check default character kind.
bool Sema::CheckEquivalenceType(QualType ExpectedType, const Expr *E) {
  auto ObjectType = E->getType();
  if(ObjectType->isArrayType())
    ObjectType = ObjectType->asArrayType()->getElementType();

  if(ExpectedType->isCharacterType()) {
    if(!ObjectType->isCharacterType()) {
      Diags.Report(E->getLocation(),
                   diag::err_typecheck_expected_char_expr)
        << ObjectType << E->getSourceRange();
      return true;
    }
  } else if(ExpectedType->isBuiltinType()) {
    if(IsDefaultBuiltinOrDoublePrecisionType(ExpectedType)) {
      if(ObjectType->isCharacterType()) {
        Diags.Report(E->getLocation(), diag::err_expected_numeric_or_logical_expr)
          << ObjectType << E->getSourceRange();
        return true;
      }
      if(!IsDefaultBuiltinOrDoublePrecisionType(ObjectType)) {
        Diags.Report(E->getLocation(),
                     diag::err_typecheck_expected_default_kind_expr)
          << ObjectType << E->getSourceRange();
        return true;
      }
    } else {
      if(!CheckTypesSameKind(ExpectedType, ObjectType)) {
        Diags.Report(E->getLocation(), diag::err_typecheck_expected_expr_of_type)
          << ExpectedType << ObjectType
          << E->getSourceRange();
        return true;
      }
    }
  }
  return false;
}

StmtResult Sema::ActOnEQUIVALENCE(ASTContext &C, SourceLocation Loc,
                                  SourceLocation PartLoc,
                                  ArrayRef<Expr*> ObjectList,
                                  Expr *StmtLabel) {
  // expression and type check.
  bool HasErrors = false;
  QualType ObjectType;
  EquivalenceScope::Object FirstEquivObject;

  for(auto I : ObjectList) {
    VarDecl *Object = nullptr;
    uint64_t Offset = 0;
    if(auto Arr = dyn_cast<ArrayElementExpr>(I)) {
      if(CheckEquivalenceObject(Loc, Arr->getTarget(), Object))
        HasErrors = true;
      for(auto S : Arr->getSubscripts()) {
        if(!StatementRequiresConstantExpression(Loc, S))
          HasErrors = true;
      }
      if(!Arr->EvaluateOffset(C, Offset))
        Object = nullptr;
    } else if(auto Str = dyn_cast<SubstringExpr>(I)) {
      if(CheckEquivalenceObject(Loc, Str->getTarget(), Object));
        HasErrors = true;
      if(Str->getStartingPoint()) {
        if(!StatementRequiresConstantExpression(Loc, Str->getStartingPoint()))
          HasErrors = true;
      }
      if(Str->getEndPoint()) {
        if(!StatementRequiresConstantExpression(Loc, Str->getEndPoint()))
          HasErrors = true;
      }
    } else if(CheckEquivalenceObject(Loc, I, Object))
      HasErrors = true;

    if(!Object) continue;

    if(ObjectType.isNull()) {
      ObjectType = I->getType();
      if(ObjectType->isArrayType())
        ObjectType = ObjectType->asArrayType()->getElementType();
      FirstEquivObject = getCurrentEquivalenceScope()->GetObject(C, I, Object, Offset);
    } else {
      if(CheckEquivalenceType(ObjectType, I))
        HasErrors = true;
      auto EquivObject = getCurrentEquivalenceScope()->GetObject(C, I, Object, Offset);
      if(getCurrentEquivalenceScope()->CheckConnection(Diags, FirstEquivObject, EquivObject))
        getCurrentEquivalenceScope()->Connect(FirstEquivObject,
                                              EquivObject);
    }
  }

  auto Result = EquivalenceStmt::Create(C, PartLoc, ObjectList, StmtLabel);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

} // end namespace flang
