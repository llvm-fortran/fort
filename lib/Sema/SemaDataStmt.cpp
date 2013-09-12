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

class ImpliedDoInliner : public ExprVisitor<ImpliedDoInliner, Expr *> {
  llvm::SmallDenseMap<const VarDecl*, Expr*, 16> InlinedVars;
  ASTContext &Context;

public:

  ImpliedDoInliner(ASTContext &C);

  Expr *VisitOptional(Expr *E);
  Expr *VisitVarExpr(VarExpr *E);
  Expr *VisitExpr(Expr *E);
  Expr *VisitArrayElementExpr(ArrayElementExpr *E);
  Expr *VisitSubstringExpr(SubstringExpr *E);

  Expr *Process(Expr *E);

  void Assign(const VarDecl *Var, Expr* Value);
};

ImpliedDoInliner::ImpliedDoInliner(ASTContext &C)
  : Context(C) {}

Expr *ImpliedDoInliner::VisitOptional(Expr *E) {
  if(E) return Visit(E);
  return nullptr;
}

Expr *ImpliedDoInliner::VisitVarExpr(VarExpr *E) {
  auto Substitute = InlinedVars.find(E->getVarDecl());
  if(Substitute != InlinedVars.end())
    return Substitute->second;
  return E;
}

Expr *ImpliedDoInliner::VisitExpr(Expr *E) {
  return E;
}

Expr *ImpliedDoInliner::VisitArrayElementExpr(ArrayElementExpr *E) {
  SmallVector<Expr*, 8> Subscripts;
  for(auto I : E->getSubscripts())
    Subscripts.push_back(Visit(I));
  return ArrayElementExpr::Create(Context, E->getLocation(), E->getTarget(), Subscripts);
}

Expr *ImpliedDoInliner::VisitSubstringExpr(SubstringExpr *E) {
  return SubstringExpr::Create(Context, E->getLocation(), E->getTarget(),
                               VisitOptional(E->getStartingPoint()),
                               VisitOptional(E->getEndPoint()));
}

void ImpliedDoInliner::Assign(const VarDecl *Var, Expr* Value) {
  auto I = InlinedVars.find(Var);
  if(I != InlinedVars.end())
    I->second = Value;
  else
    InlinedVars.insert(std::make_pair(Var, Value));
}

Expr *ImpliedDoInliner::Process(Expr *E) {
  if(InlinedVars.empty())
    return E;
  return Visit(E);
}

/// Iterates over the items in a DATA statent and emits corresponding
/// assignment AST nodes.
/// FIXME report source ranges for diagnostics (ArrayElement and Substring)
class DataValueAssigner : public ExprVisitor<DataValueAssigner> {
  DataValueIterator &Values;
  flang::Sema &Sem;
  DiagnosticsEngine &Diags;
  SourceLocation DataStmtLoc;

  ImpliedDoInliner ImpliedDoEvaluator;
  SmallVector<Stmt*, 16> ResultingAST;

  bool Done;
public:
  DataValueAssigner(DataValueIterator &Vals, flang::Sema &S,
                    DiagnosticsEngine &Diag, SourceLocation Loc)
    : Values(Vals), Sem(S), Diags(Diag),
      DataStmtLoc(Loc), Done(false),
      ImpliedDoEvaluator(S.getContext()) {
  }

  bool HasValues(const Expr *Where);

  bool IsDone() const {
    return Done;
  }

  void VisitExpr(Expr *E);
  void VisitVarExpr(VarExpr *E);
  void VisitArrayElementExpr(ArrayElementExpr *E);
  void VisitSubstringExpr(SubstringExpr *E);
  void VisitImpliedDoExpr(ImpliedDoExpr *E);

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
    Diags.Report(DataStmtLoc, diag::err_data_stmt_not_enough_values)
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
    ASTContext &C = Sem.getContext();

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
      auto RHS = Sem.CheckAndApplyAssignmentConstraints(Value->getLocation(),
                                                        ElementType, Value,
                                                        Sema::AssignmentAction::Initializing,
                                                        E);
      if(RHS.isUsable())
        Items.push_back(RHS.get());
    }

    Value = ArrayConstructorExpr::Create(C, SourceLocation(),
                                         Items, Type);
  } else {
    // single item
    if(!HasValues(E)) return;
    Value = Values.getValue();
    Values.advance();
    auto RHS = Sem.CheckAndApplyAssignmentConstraints(Value->getLocation(),
                                                      Type, Value,
                                                      Sema::AssignmentAction::Initializing,
                                                      E);
    Value = RHS.get();
  }

  if(Value) {
    VD->setInit(Value);
    Emit(AssignmentStmt::Create(Sem.getContext(), Value->getLocation(),
                                E, Value, nullptr));
  }
}

void DataValueAssigner::EmitAssignment(Expr *LHS) {
  if(!HasValues(LHS)) return;
  auto Value = Values.getValue();
  Values.advance();

  LHS = ImpliedDoEvaluator.Process(LHS);
  auto RHS = Sem.CheckAndApplyAssignmentConstraints(Value->getLocation(),
                                                    LHS->getType(), Value,
                                                    Sema::AssignmentAction::Initializing,
                                                    LHS);
  if(RHS.isUsable())
    Emit(AssignmentStmt::Create(Sem.getContext(), Value->getLocation(),
                                LHS, RHS.get(), nullptr));
}

void DataValueAssigner::VisitArrayElementExpr(ArrayElementExpr *E) {
  EmitAssignment(E);
}

void DataValueAssigner::VisitSubstringExpr(SubstringExpr *E) {
  EmitAssignment(E);
}

void DataValueAssigner::VisitImpliedDoExpr(ImpliedDoExpr *E) {
  auto Start = Sem.EvalAndCheckIntExpr(E->getInitialParameter(), 1);
  auto End = Sem.EvalAndCheckIntExpr(E->getTerminalParameter(), 1);
  int64_t Inc = 1;
  if(E->hasIncrementationParameter())
    Inc = Sem.EvalAndCheckIntExpr(E->getIncrementationParameter(), 1);
  for(; Start <= End; Start+=Inc) {
    ImpliedDoEvaluator.Assign(E->getVarDecl(), IntegerConstantExpr::Create(Sem.getContext(),
                                                                           Start));
    for(auto I : E->getBody())
      Visit(I);
  }
}

StmtResult Sema::ActOnDATA(ASTContext &C, SourceLocation Loc,
                           ArrayRef<Expr*> Objects,
                           ArrayRef<Expr*> Values,
                           Expr *StmtLabel) {
  DataValueIterator ValuesIt(Values);
  DataValueAssigner LHSVisitor(ValuesIt, *this, Diags, Loc);
  for(auto I : Objects) {
    LHSVisitor.Visit(I);
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

  auto Result = DataStmt::Create(C, Loc, Objects, Values,
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
