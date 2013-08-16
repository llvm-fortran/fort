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

  Expr *getActualValue() const {
    return Values[ValueOffset];
  }

  Expr *getActualLastValue() const {
    return Values.back();
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
/// FIXME report source ranges for diagnostics (ArrayElement and Substring)
class DataValueAssigner : public ExprVisitor<DataValueAssigner> {
  DataValueIterator &Values;
  flang::Sema &Sema;
  DiagnosticsEngine &Diags;
  SourceLocation DataStmtLoc;

  SmallVector<Stmt*, 16> ResultingAST;
  bool Done;
public:
  DataValueAssigner(DataValueIterator &Vals, flang::Sema &S,
                    DiagnosticsEngine &Diag, SourceLocation Loc)
    : Values(Vals), Sema(S), Diags(Diag),
      DataStmtLoc(Loc), Done(false) {
  }

  bool HasValues(const Expr *Where);

  bool IsDone() const {
    return Done;
  }

  void VisitExpr(Expr *E);
  void VisitVarExpr(VarExpr *E);
  void VisitArrayElementExpr(ArrayElementExpr *E);
  void VisitSubstringExpr(SubstringExpr *E);

  void EmitAssignment(Expr *LHS);

  void Emit(Stmt *S);
  ArrayRef<Stmt*> getEmittedStmtList() const {
    return ResultingAST;
  }
};

void DataValueAssigner::Emit(Stmt *S) {
  ResultingAST.push_back(S);
}

bool DataValueAssigner::HasValues(const Expr *Where) {
  if(Values.isEmpty()) {
    // more items than values.
    Diags.Report(DataStmtLoc, diag::err_data_stmt_not_enought_values)
      << Where->getSourceRange();
    Done = true;
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
  Expr *Value = nullptr;

  if(auto ATy = dyn_cast<ArrayType>(Type.getTypePtr())) {
    ASTContext &C = Sema.getContext();

    uint64_t ArraySize;
    if(!ATy->EvaluateSize(ArraySize, C)) {
      VisitExpr(E);
    }
    SmallVector<Expr*, 32> Items;
    auto ElementType = ATy->getElementType();
    for(uint64_t I = 0; I < ArraySize; ++I) {
      if(!HasValues(E)) return;
      auto Value = Values.getValue();
      Values.advance();
      Value = Sema.TypecheckAssignment(ElementType, Value,
                                       E->getLocation(),
                                       Value->getSourceRange(),
                                       E->getSourceRange()).get();
      if(Value)
        Items.push_back(Value);
    }

    Value = ArrayConstructorExpr::Create(C, SourceLocation(),
                                         Items, Type);
  } else {
    // single item
    if(!HasValues(E)) return;
    Value = Values.getValue();
    Values.advance();
    Value = Sema.TypecheckAssignment(Type, Value,
                                     E->getLocation(),
                                     Value->getSourceRange(),
                                     E->getSourceRange()).get();
  }

  if(Value) {
    VD->setInit(Value);
    Emit(AssignmentStmt::Create(Sema.getContext(), Value->getLocation(),
                                E, Value, nullptr));
  }
}

void DataValueAssigner::EmitAssignment(Expr *LHS) {
  if(!HasValues(LHS)) return;
  auto Value = Values.getValue();
  Values.advance();
  auto Result = Sema.ActOnAssignmentStmt(Sema.getContext(), Value->getLocation(),
                                         LHS, Value, nullptr);
  if(Result.isUsable())
    Emit(Result.get());
}

void DataValueAssigner::VisitArrayElementExpr(ArrayElementExpr *E) {
  EmitAssignment(E);
}

void DataValueAssigner::VisitSubstringExpr(SubstringExpr *E) {
  EmitAssignment(E);
}

/// FIXME add checking for implied do
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
    LHSVisitor.Visit(Items[I]);
    if(LHSVisitor.IsDone()) break;
  }

  if(!ValuesIt.isEmpty()) {
    // more items than values
    auto FirstVal = ValuesIt.getActualValue();
    auto LastVal = ValuesIt.getActualLastValue();
    Diags.Report(FirstVal->getLocation(), diag::err_data_stmt_too_many_values)
      << SourceRange(FirstVal->getLocStart(), LastVal->getLocEnd());
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

  if(!CheckConstantExpression(Value.get()))
    HasErrors = true;

  if(HasErrors) return ExprError();
  return RepeatExpr? RepeatedConstantExpr::Create(C, RepeatLoc,
                                                  RepeatExpr, Value.take())
                   : Value;
}

} // end namespace flang
