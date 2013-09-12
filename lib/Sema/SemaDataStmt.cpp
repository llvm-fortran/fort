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
#include "flang/AST/ExprConstant.h"
#include "flang/Basic/Diagnostic.h"
#include "llvm/ADT/SmallString.h"
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
class DataValueAssigner : public ExprVisitor<DataValueAssigner> {
  DataValueIterator &Values;
  flang::Sema &Sem;
  ASTContext &Context;
  DiagnosticsEngine &Diags;
  SourceLocation DataStmtLoc;

  ExprEvalScope ImpliedDoEvaluator;
  SmallVector<Stmt*, 16> ResultingAST;

  bool Done;
public:
  DataValueAssigner(DataValueIterator &Vals, flang::Sema &S,
                    DiagnosticsEngine &Diag, SourceLocation Loc)
    : Values(Vals), Sem(S), Context(S.getContext()),
      Diags(Diag), DataStmtLoc(Loc), Done(false),
      ImpliedDoEvaluator(S.getContext()) {
  }

  bool HasValues(const Expr *Where);
  ExprResult getAndCheckValue(QualType LHSType, Expr *LHS);

  bool IsDone() const {
    return Done;
  }

  void VisitExpr(Expr *E);
  void VisitVarExpr(VarExpr *E);
  void VisitArrayElementExpr(ArrayElementExpr *E);
  void VisitSubstringExpr(SubstringExpr *E);
  void VisitImpliedDoExpr(ImpliedDoExpr *E);

  void Emit(Stmt *S);
  ArrayRef<Stmt*> getEmittedStmtList() const {
    return ResultingAST;
  }

  bool CheckVar(VarExpr *E);
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

bool DataValueAssigner::CheckVar(VarExpr *E) {
  auto VD = E->getVarDecl();
  if(VD->isArgument() || VD->isParameter()) {
    VisitExpr(E);
    return true;
  }
  if(VD->isUnusedSymbol())
    const_cast<VarDecl*>(VD)->MarkUsedAsVariable(E->getLocation());
  return false;
}

ExprResult DataValueAssigner::getAndCheckValue(QualType LHSType,
                                               Expr *LHS) {
  if(!HasValues(LHS)) return ExprResult(true);
  auto Value = Values.getValue();
  Values.advance();
  return Sem.CheckAndApplyAssignmentConstraints(Value->getLocation(),
                                                LHSType, Value,
                                                Sema::AssignmentAction::Initializing,
                                                LHS);
}

void DataValueAssigner::VisitVarExpr(VarExpr *E) {
  if(CheckVar(E))
    return;
  auto VD = E->getVarDecl();
  auto Type = VD->getType();
  if(auto ATy = Type->asArrayType()) {
    uint64_t ArraySize;
    if(!ATy->EvaluateSize(ArraySize, Context)) {
      VisitExpr(E);
      return;
    }

    // Construct an array constructor expression for initializer
    SmallVector<Expr*, 32> Items(ArraySize);
    bool IsUsable = true;
    SourceLocation Loc;
    auto ElementType = ATy->getElementType();
    for(uint64_t I = 0; I < ArraySize; ++I) {
      if(!HasValues(E)) return;
      auto Val = getAndCheckValue(ElementType, E);
      if(Val.isUsable()) {
        Items[I] = Val.get();
        if(!Loc.isValid())
          Loc = Val.get()->getLocation();
      }
      else IsUsable = false;
    }

    if(IsUsable) {
      VD->setInit(ArrayConstructorExpr::Create(Context, Loc,
                                               Items, Type));
    }
    return;
  }

  // single item
  auto Val = getAndCheckValue(Type, E);
  if(Val.isUsable())
    VD->setInit(Val.get());
}

void DataValueAssigner::VisitArrayElementExpr(ArrayElementExpr *E) {
  auto Target = dyn_cast<VarExpr>(E->getTarget());
  if(!Target)
    return VisitExpr(E);
  if(CheckVar(Target))
    return;

  auto VD = Target->getVarDecl();
  auto ATy = VD->getType()->asArrayType();
  auto ElementType = ATy->getElementType();

  uint64_t ArraySize;
  if(!ATy->EvaluateSize(ArraySize, Context))
    return VisitExpr(E);

  SmallVector<Expr*, 32> Items(ArraySize);
  if(VD->hasInit()) {
    assert(isa<ArrayConstructorExpr>(VD->getInit()));
    auto InsertPoint = cast<ArrayConstructorExpr>(VD->getInit())->getItems();
    for(uint64_t I = 0; I < ArraySize; ++I)
      Items[I] = InsertPoint[I];
  } else {
    for(uint64_t I = 0; I < ArraySize; ++I)
      Items[I] = nullptr;
  }

  uint64_t Offset;
  if(!E->EvaluateOffset(Context, Offset, &ImpliedDoEvaluator))
    return VisitExpr(E);
  auto Val = getAndCheckValue(ElementType, E);
  if(Val.isUsable() && Offset < Items.size()) {
    Items[Offset] = Val.get();
    VD->setInit(ArrayConstructorExpr::Create(Context, Val.get()->getLocation(),
                                             Items, VD->getType()));
  }
}

void DataValueAssigner::VisitSubstringExpr(SubstringExpr *E) {
  if(isa<DesignatorExpr>(E->getTarget())) {
    // FIXME: todo.
    return VisitExpr(E);
  }

  auto Target = dyn_cast<VarExpr>(E->getTarget());
  if(!Target)
    return VisitExpr(E);
  if(CheckVar(Target))
    return;

  auto VD = Target->getVarDecl();

  uint64_t Len = 1;
  auto CharTy = VD->getType().getSelfOrArrayElementType();
  if(auto Ext = CharTy.getExtQualsPtrOrNull()) {
    if(Ext->hasLengthSelector())
      Len = Ext->getLengthSelector();
  }

  uint64_t Begin, End;
  if(!E->EvaluateRange(Context, Len, Begin, End, &ImpliedDoEvaluator))
    return VisitExpr(E);

  auto Val = getAndCheckValue(E->getType(), E);
  if(!Val.isUsable())
    return;
  auto StrVal = StringRef(cast<CharacterConstantExpr>(Val.get())->getValue());

  llvm::SmallString<64> Str;
  Str.resize(Len, ' ');
  uint64_t I;
  for(I = Begin; I < End; ++I) {
    if((I - Begin) >= StrVal.size())
      break;
    Str[I] = StrVal[I - Begin];
  }
  for(; I < End; ++I) Str[I] = ' ';
  VD->setInit(CharacterConstantExpr::Create(Context, Val.get()->getLocation(),
                                            Val.get()->getLocation(), Str));
}

void DataValueAssigner::VisitImpliedDoExpr(ImpliedDoExpr *E) {
  auto Start = Sem.EvalAndCheckIntExpr(E->getInitialParameter(), 1);
  auto End = Sem.EvalAndCheckIntExpr(E->getTerminalParameter(), 1);
  int64_t Inc = 1;
  if(E->hasIncrementationParameter())
    Inc = Sem.EvalAndCheckIntExpr(E->getIncrementationParameter(), 1);
  for(; Start <= End; Start+=Inc) {
    ImpliedDoEvaluator.Assign(E->getVarDecl(), Start);
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
