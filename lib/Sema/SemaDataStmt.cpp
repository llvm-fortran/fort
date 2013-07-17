//===--- SemaDataStmt.cpp - AST Builder and Checker for the DATA statement ===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the checking and AST construction for the DATA
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
#include "llvm/Support/raw_ostream.h"

namespace flang {

/// Iterates over values in a DATA statement, taking repetitions into account.
class DataValueIterator {
  ArrayRef<Expr*> Values;
  Expr *Value;
  size_t   ValueOffset;
  uint64_t CurRepeatOffset;
  uint64_t CurRepeatCount;

  void InitItem();
public:
  DataValueIterator(ArrayRef<Expr*> Vals)
    : Values(Vals), ValueOffset(0) {
    InitItem();
  }

  QualType getType() const {
    return Value->getType();
  }

  Expr *getValue() const {
    return Value;
  }

  bool isEmpty() const {
    return ValueOffset >= Values.size();
  }

  void advance();
};

void DataValueIterator::InitItem() {
  Value = Values[ValueOffset];
  if(auto RepeatedValue = dyn_cast<RepeatedConstantExpr>(Value)) {
    CurRepeatCount = RepeatedValue->getRepeatCount().getLimitedValue();
    Value = RepeatedValue->getExpression();
  } else CurRepeatCount = 1;
  CurRepeatOffset = 0;
}

void DataValueIterator::advance() {
  CurRepeatOffset++;
  if(CurRepeatOffset >= CurRepeatCount) {
    ValueOffset++;
    if(ValueOffset < Values.size())
      InitItem();
  }
}

/// Iterates over the items in a DATA statent and emits corresponding
/// assignment AST nodes.
/// FIXME report source ranges for diagnostics
class DataValueAssigner : public ExprVisitor<DataValueAssigner> {
  DataValueIterator &Values;
  flang::Sema &Sema;
  DiagnosticsEngine &Diags;
  SourceLocation DataStmtLoc;

  SmallVector<Stmt*, 16> ResultingAST;
public:
  DataValueAssigner(DataValueIterator &Vals, flang::Sema &S,
                    DiagnosticsEngine &Diag, SourceLocation Loc)
    : Values(Vals), Sema(S), Diags(Diag),
      DataStmtLoc(Loc) {
  }

  bool HasValues();

  void VisitExpr(Expr *E);
  void VisitVarExpr(VarExpr *E);

  void Emit(Stmt *S);
  ArrayRef<Stmt*> getEmittedStmtList() const {
    return ResultingAST;
  }
};

void DataValueAssigner::Emit(Stmt *S) {
  ResultingAST.push_back(S);
}

bool DataValueAssigner::HasValues() {
  if(Values.isEmpty()) {
    // more items than values.
    Diags.Report(DataStmtLoc, diag::err_data_stmt_not_enought_values);
    return false;
  }
  return true;
}

void DataValueAssigner::VisitExpr(Expr *E) {
  Diags.Report(E->getLocation(), diag::err_data_stmt_invalid_item)
    << E->getSourceRange();
}

void DataValueAssigner::VisitVarExpr(VarExpr *E) {
  auto VD = E->getVarDecl();
  if(VD->isArgument()) {
    VisitExpr(E);
    return;
  }
  else if(VD->isParameter()) {
    VisitExpr(E);
    return;
  }

  auto Type = VD->getType();
  if(auto ATy = dyn_cast<ArrayType>(Type.getTypePtr())) {
    uint64_t ArraySize;
    if(!ATy->EvaluateSize(ArraySize, Sema.getContext())) {
      // FIXME
    }
  } else {
    // single item
    if(!HasValues()) return;
    auto Value = Values.getValue();
    Values.advance();
    Value = Sema.TypecheckAssignment(Type, Value,
                                     Value->getLocation(),
                                     Value->getLocEnd()).get();
    if(Value) {
      VD->setInit(Value);
      Emit(AssignmentStmt::Create(Sema.getContext(), Value->getLocation(),
                                  E, Value, nullptr));
    }
  }
}

/// FIXME add checking for implied do
/// FIXME LHS arrays
/// FIXME LHS array elements, substrings
/// FIXME
StmtResult Sema::ActOnDATA(ASTContext &C, SourceLocation Loc,
                           ArrayRef<ExprResult> LHS,
                           ArrayRef<ExprResult> RHS,
                           Expr *StmtLabel) {
  SmallVector<Expr*, 8> Items  (LHS.size());
  SmallVector<Expr*, 8> Values (RHS.size());
  for(size_t I = 0; I < LHS.size(); ++I)
    Items[I] = LHS[I].take();
  for(size_t I = 0; I < RHS.size(); ++I)
    Values[I] = RHS[I].take();

  DataValueIterator ValuesIt(Values);
  DataValueAssigner LHSVisitor(ValuesIt, *this, Diags, Loc);
  for(size_t I = 0; I < Items.size(); ++I) {
    if(I > 0 && ValuesIt.isEmpty())
      break;
    LHSVisitor.Visit(Items[I]);
  }

  if(!ValuesIt.isEmpty()) {
    // more items than values
    Diags.Report(Loc, diag::err_data_stmt_too_many_values);
  }

  Stmt *ResultStmt;
  auto ResultStmts = LHSVisitor.getEmittedStmtList();
  if(ResultStmts.size() > 1)
    ResultStmt = BlockStmt::Create(Context, Loc, ResultStmts);
  else if(ResultStmts.size() == 1)
    ResultStmt = ResultStmts[0];
  else ResultStmt = nullptr;

  auto Result = DataStmt::Create(C, Loc, Items, Values,
                                 ResultStmt, StmtLabel);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

ExprResult Sema::ActOnDATAConstantExpr(ASTContext &C,
                                       SourceLocation RepeatLoc,
                                       ExprResult RepeatCount,
                                       ExprResult Value) {
  IntegerConstantExpr *RepeatExpr = nullptr;
  bool HasErrors = false;

  if(RepeatCount.isUsable()) {
    RepeatExpr = dyn_cast<IntegerConstantExpr>(RepeatCount.get());
    if(!RepeatExpr ||
       !RepeatExpr->getValue().isStrictlyPositive()) {
      Diags.Report(RepeatCount.get()->getLocation(),
                   diag::err_expected_integer_gt_0)
        << RepeatCount.get()->getSourceRange();
      HasErrors = true;
      RepeatExpr = nullptr;
    }
  }

  auto Constant = dyn_cast<ConstantExpr>(Value.get());
  if(!Constant) {
    /// the value can also be a constant variable
    VarExpr *Var = dyn_cast<VarExpr>(Value.get());
    if(!(Var && Var->getVarDecl()->isParameter())) {
      Diags.Report(Value.get()->getLocation(),
                   diag::err_expected_constant_expr)
        << Value.get()->getSourceRange();
      HasErrors = true;
    }
  }

  if(HasErrors) return ExprError();
  return RepeatExpr? RepeatedConstantExpr::Create(C, RepeatLoc,
                                                  RepeatExpr, Value.take())
                   : Value;
}

} // end namespace flang
