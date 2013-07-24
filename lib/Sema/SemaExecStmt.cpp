//= SemaExecStmt.cpp - AST Builder and Checker for the executable statements =//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the checking and AST construction for the executable
// statements.
//
//===----------------------------------------------------------------------===//

#include "flang/Sema/Sema.h"
#include "flang/Sema/DeclSpec.h"
#include "flang/Sema/SemaDiagnostic.h"
#include "flang/Sema/SemaInternal.h"
#include "flang/Parse/ParseDiagnostic.h"
#include "flang/AST/ASTContext.h"
#include "flang/AST/Decl.h"
#include "flang/AST/Stmt.h"
#include "flang/Basic/Diagnostic.h"
#include "llvm/Support/raw_ostream.h"

namespace flang {

void StmtLabelResolver::VisitAssignStmt(AssignStmt *S) {
  S->setAddress(StmtLabelReference(StmtLabelDecl));
  StmtLabelDecl->setStmtLabelUsedAsAssignTarget();
}

StmtResult Sema::ActOnAssignStmt(ASTContext &C, SourceLocation Loc,
                                 ExprResult Value, VarExpr* VarRef,
                                 Expr *StmtLabel) {
  StmtRequiresIntegerVar(Loc, VarRef);
  CheckVarIsAssignable(VarRef);
  Stmt *Result;
  auto Decl = getCurrentStmtLabelScope()->Resolve(Value.get());
  if(!Decl) {
    Result = AssignStmt::Create(C, Loc, StmtLabelReference(),VarRef, StmtLabel);
    getCurrentStmtLabelScope()->DeclareForwardReference(
      StmtLabelScope::ForwardDecl(Value.get(), Result));
  } else {
    Result = AssignStmt::Create(C, Loc, StmtLabelReference(Decl),
                                VarRef, StmtLabel);
    Decl->setStmtLabelUsedAsAssignTarget();
  }

  getCurrentBody()->Append(Result);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

void StmtLabelResolver::VisitAssignedGotoStmt(AssignedGotoStmt *S) {
  S->setAllowedValue(Info.ResolveCallbackData,StmtLabelReference(StmtLabelDecl));
}

StmtResult Sema::ActOnAssignedGotoStmt(ASTContext &C, SourceLocation Loc,
                                       VarExpr* VarRef,
                                       ArrayRef<Expr*> AllowedValues,
                                       Expr *StmtLabel) {
  StmtRequiresIntegerVar(Loc, VarRef);

  SmallVector<StmtLabelReference, 4> AllowedLabels(AllowedValues.size());
  for(size_t I = 0; I < AllowedValues.size(); ++I) {
    auto Decl = getCurrentStmtLabelScope()->Resolve(AllowedValues[I]);
    AllowedLabels[I] = Decl? StmtLabelReference(Decl): StmtLabelReference();
  }
  auto Result = AssignedGotoStmt::Create(C, Loc, VarRef, AllowedLabels, StmtLabel);

  for(size_t I = 0; I < AllowedValues.size(); ++I) {
    if(!AllowedLabels[I].Statement) {
      getCurrentStmtLabelScope()->DeclareForwardReference(
        StmtLabelScope::ForwardDecl(AllowedValues[I], Result, I));
    }
  }

  getCurrentBody()->Append(Result);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

void StmtLabelResolver::VisitGotoStmt(GotoStmt *S) {
  S->setDestination(StmtLabelReference(StmtLabelDecl));
  StmtLabelDecl->setStmtLabelUsedAsGotoTarget();
}

StmtResult Sema::ActOnGotoStmt(ASTContext &C, SourceLocation Loc,
                               ExprResult Destination, Expr *StmtLabel) {
  Stmt *Result;
  auto Decl = getCurrentStmtLabelScope()->Resolve(Destination.get());
  if(!Decl) {
    Result = GotoStmt::Create(C, Loc, StmtLabelReference(), StmtLabel);
    getCurrentStmtLabelScope()->DeclareForwardReference(
      StmtLabelScope::ForwardDecl(Destination.get(), Result));
  } else {
    Result = GotoStmt::Create(C, Loc, StmtLabelReference(Decl), StmtLabel);
    Decl->setStmtLabelUsedAsGotoTarget();
  }

  getCurrentBody()->Append(Result);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

// =========================================================================
// Block statements entry
// =========================================================================

StmtResult Sema::ActOnIfStmt(ASTContext &C, SourceLocation Loc,
                             ExprResult Condition, Expr *StmtLabel) {
  if(Condition.isUsable())
    StmtRequiresLogicalExpression(Loc, Condition.get());

  auto Result = IfStmt::Create(C, Loc, Condition.get(), StmtLabel);
  if(Condition.isUsable())
    getCurrentBody()->Append(Result);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  getCurrentBody()->Enter(Result);
  return Result;
}

void StmtLabelResolver::VisitDoStmt(DoStmt *S) {
  S->setTerminatingStmt(StmtLabelReference(StmtLabelDecl));
}

/// FIXME: TODO Transfer of control into the range of a DO-loop from outside the range is not permitted.
StmtResult Sema::ActOnDoStmt(ASTContext &C, SourceLocation Loc, SourceLocation EqualLoc,
                             ExprResult TerminatingStmt,
                             VarExpr *DoVar, ExprResult E1, ExprResult E2,
                             ExprResult E3, Expr *StmtLabel) {
  // typecheck
  bool AddToBody = true;
  if(DoVar) {
    StmtRequiresScalarNumericVar(Loc, DoVar, diag::err_typecheck_stmt_requires_int_var);
    CheckVarIsAssignable(DoVar);
    auto DoVarType = DoVar->getType();
    if(E1.isUsable()) {
      if(CheckScalarNumericExpression(E1.get()))
        E1 = TypecheckAssignment(DoVarType, E1);
    } else AddToBody = false;
    if(E2.isUsable()) {
      if(CheckScalarNumericExpression(E2.get()))
        E2 = TypecheckAssignment(DoVarType, E2);
    } else AddToBody = false;
    if(E3.isUsable()) {
      if(CheckScalarNumericExpression(E3.get()))
        E3 = TypecheckAssignment(DoVarType, E3);
    }
  } else AddToBody = false;

  // Make sure the statement label isn't already declared
  if(TerminatingStmt.isUsable()) {
    if(auto Decl = getCurrentStmtLabelScope()->Resolve(TerminatingStmt.get())) {
      std::string String;
      llvm::raw_string_ostream Stream(String);
      TerminatingStmt.get()->dump(Stream);
      Diags.Report(TerminatingStmt.get()->getLocation(),
                   diag::err_stmt_label_must_decl_after)
          << Stream.str() << "DO"
          << TerminatingStmt.get()->getSourceRange();
      Diags.Report(Decl->getStmtLabel()->getLocation(),
                   diag::note_previous_definition)
          << Decl->getStmtLabel()->getSourceRange();
      return StmtError();
    }
  }
  auto Result = DoStmt::Create(C, Loc, StmtLabelReference(),
                               DoVar, E1.get(), E2.get(),
                               E3.get(), StmtLabel);
  if(DoVar)
    AddLoopVar(DoVar);
  if(AddToBody)
    getCurrentBody()->Append(Result);
  if(TerminatingStmt.get())
    getCurrentStmtLabelScope()->DeclareForwardReference(
      StmtLabelScope::ForwardDecl(TerminatingStmt.get(), Result));
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  if(TerminatingStmt.get())
    getCurrentBody()->Enter(BlockStmtBuilder::Entry(
                             Result,TerminatingStmt.get()));
  else getCurrentBody()->Enter(Result);
  return Result;
}

StmtResult Sema::ActOnDoWhileStmt(ASTContext &C, SourceLocation Loc, ExprResult Condition,
                                  Expr *StmtLabel) {
  if(Condition.isUsable())
    StmtRequiresLogicalExpression(Loc, Condition.get());

  auto Result = DoWhileStmt::Create(C, Loc, Condition.get(), StmtLabel);
  if(Condition.isUsable()) getCurrentBody()->Append(Result);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  getCurrentBody()->Enter(Result);
  return Result;
}


// =============================================================
// Block statements termination and control flow
// =============================================================

void Sema::ReportUnterminatedStmt(const BlockStmtBuilder::Entry &S,
                                  SourceLocation Loc,
                                  bool ReportUnterminatedLabeledDo) {
  const char *Keyword;
  const char *BeginKeyword;
  switch(S.Statement->getStmtClass()) {
  case Stmt::IfStmtClass:
    Keyword = "end if";
    BeginKeyword = "if";
    break;
  case Stmt::DoWhileStmtClass:
  case Stmt::DoStmtClass: {
    if(S.ExpectedEndDoLabel) {
      if(ReportUnterminatedLabeledDo) {
        std::string Str;
        llvm::raw_string_ostream Stream(Str);
        S.ExpectedEndDoLabel->dump(Stream);
        Diags.Report(Loc, diag::err_expected_stmt_label_end_do) << Stream.str();
        Diags.Report(S.Statement->getLocation(), diag::note_matching) << "do";
      }
      return;
    }
    Keyword = "end do";
    BeginKeyword = "do";
    break;
  }
  default:
    llvm_unreachable("Invalid stmt");
  }
  Diags.Report(Loc, diag::err_expected_kw) << Keyword;
  Diags.Report(S.Statement->getLocation(), diag::note_matching) << BeginKeyword;
}

void Sema::LeaveLastBlock() {
  auto Last = getCurrentBody()->LastEntered().Statement;
  if(auto Do = dyn_cast<DoStmt>(Last)) {
    RemoveLoopVar(Do->getDoVar());
  }
  getCurrentBody()->Leave(Context);
}

IfStmt *Sema::LeaveBlocksUntilIf(SourceLocation Loc) {
  auto Stack = getCurrentBody()->ControlFlowStack;
  for(size_t I = Stack.size(); I != 0;) {
    --I;
    if(auto If = dyn_cast<IfStmt>(Stack[I].Statement))
      return If;
    ReportUnterminatedStmt(Stack[I], Loc);
    LeaveLastBlock();
  }
  return nullptr;
}

Stmt *Sema::LeaveBlocksUntilDo(SourceLocation Loc) {
  auto Stack = getCurrentBody()->ControlFlowStack;
  for(size_t I = Stack.size(); I != 0;) {
    --I;
    auto S = Stack[I].Statement;
    if(isa<DoWhileStmt>(S) ||
       isa<DoStmt>(S) && !Stack[I].hasExpectedDoLabel())
      return S;
    ReportUnterminatedStmt(Stack[I], Loc);
    LeaveLastBlock();
  }
  return nullptr;
}

/// The terminal statement of a DO-loop must not be an unconditional GO TO,
/// assigned GO TO, arithmetic IF, block IF, ELSE IF, ELSE, END IF, RETURN, STOP, END, or DO statement.
/// If the terminal statement of a DO-loop is a logical IF statement,
/// it may contain any executable statement except a DO,
/// block IF, ELSE IF, ELSE, END IF, END, or another logical IF statement.
///
/// FIXME: TODO full
static bool IsValidDoLogicalIfThenStatement(const Stmt *S) {
  switch(S->getStmtClass()) {
  case Stmt::DoStmtClass: case Stmt::IfStmtClass: case Stmt::DoWhileStmtClass:
  case Stmt::ConstructPartStmtClass:
    return false;
  default:
    return true;
  }
}

bool Sema::IsValidDoTerminatingStatement(const Stmt *S) {
  switch(S->getStmtClass()) {
  case Stmt::GotoStmtClass: case Stmt::AssignedGotoStmtClass:
  case Stmt::StopStmtClass: case Stmt::DoStmtClass:
  case Stmt::DoWhileStmtClass:
  case Stmt::ConstructPartStmtClass:
    return false;
  case Stmt::IfStmtClass: {
    auto NextStmt = cast<IfStmt>(S)->getThenStmt();
    return NextStmt && IsValidDoLogicalIfThenStatement(NextStmt);
  }
  default:
    return true;
  }
}

bool Sema::IsInLabeledDo(const Expr *StmtLabel) {
  auto Stack = getCurrentBody()->ControlFlowStack;
  for(size_t I = Stack.size(); I != 0;) {
    --I;
    if(isa<DoStmt>(Stack[I].Statement)) {
      if(Stack[I].hasExpectedDoLabel()) {
        if(getCurrentStmtLabelScope()->IsSame(Stack[I].ExpectedEndDoLabel, StmtLabel))
          return true;
      }
    }
  }
  return false;
}

DoStmt *Sema::LeaveBlocksUntilLabeledDo(SourceLocation Loc, const Expr *StmtLabel) {
  auto Stack = getCurrentBody()->ControlFlowStack;
  for(size_t I = Stack.size(); I != 0;) {
    --I;
    if(auto Do = dyn_cast<DoStmt>(Stack[I].Statement)) {
      if(Stack[I].hasExpectedDoLabel()) {
        if(getCurrentStmtLabelScope()->IsSame(Stack[I].ExpectedEndDoLabel, StmtLabel))
          return Do;
      }
    }
    ReportUnterminatedStmt(Stack[I], Loc);
    LeaveLastBlock();
  }
  return nullptr;
}

StmtResult Sema::ActOnElseIfStmt(ASTContext &C, SourceLocation Loc,
                                 ExprResult Condition, Expr *StmtLabel) {
  auto IfBegin = LeaveBlocksUntilIf(Loc);
  if(!IfBegin)
    Diags.Report(Loc, diag::err_stmt_not_in_if) << "else if";

  // typecheck
  if(Condition.isUsable())
    StmtRequiresLogicalExpression(Loc, Condition.get());

  auto Result = IfStmt::Create(C, Loc, Condition.get(), StmtLabel);
  if(IfBegin) {
    LeaveLastBlock();
    if(Condition.isUsable())
      IfBegin->setElseStmt(Result);
  }
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  getCurrentBody()->Enter(Result);
  return Result;
}

StmtResult Sema::ActOnElseStmt(ASTContext &C, SourceLocation Loc, Expr *StmtLabel) {
  auto IfBegin = LeaveBlocksUntilIf(Loc);
  if(!IfBegin)
    Diags.Report(Loc, diag::err_stmt_not_in_if) << "else";

  auto Result = ConstructPartStmt::Create(C, ConstructPartStmt::ElseStmtClass, Loc, nullptr, StmtLabel);
  getCurrentBody()->Append(Result);
  if(IfBegin) getCurrentBody()->LeaveIfThen(C);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

StmtResult Sema::ActOnEndIfStmt(ASTContext &C, SourceLocation Loc, Expr *StmtLabel) {
  auto IfBegin = LeaveBlocksUntilIf(Loc);
  if(!IfBegin)
    Diags.Report(Loc, diag::err_stmt_not_in_if) << "end if";

  auto Result = ConstructPartStmt::Create(C, ConstructPartStmt::EndIfStmtClass, Loc, nullptr, StmtLabel);
  getCurrentBody()->Append(Result);
  if(IfBegin) LeaveLastBlock();
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

StmtResult Sema::ActOnEndDoStmt(ASTContext &C, SourceLocation Loc, Expr *StmtLabel) {
  auto DoBegin = LeaveBlocksUntilDo(Loc);
  if(!DoBegin)
    Diags.Report(Loc, diag::err_end_do_without_do);

  auto Result = ConstructPartStmt::Create(C, ConstructPartStmt::EndDoStmtClass, Loc, nullptr, StmtLabel);
  getCurrentBody()->Append(Result);
  if(DoBegin) LeaveLastBlock();
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

/// FIXME: Fortran 90+: make multiple do end at one label obsolete
void Sema::CheckStatementLabelEndDo(Expr *StmtLabel, Stmt *S) {
  if(!getCurrentBody()->HasEntered()) return;
  if(!IsInLabeledDo(StmtLabel)) return;
  auto DoBegin = LeaveBlocksUntilLabeledDo(S->getLocation(), StmtLabel);

  getCurrentStmtLabelScope()->RemoveForwardReference(DoBegin);
  if(!IsValidDoTerminatingStatement(S))
    Diags.Report(S->getLocation(), diag::err_invalid_do_terminating_stmt);
  DoBegin->setTerminatingStmt(StmtLabelReference(S));
  LeaveLastBlock();
}


StmtResult Sema::ActOnContinueStmt(ASTContext &C, SourceLocation Loc, Expr *StmtLabel) {
  auto Result = ContinueStmt::Create(C, Loc, StmtLabel);
  getCurrentBody()->Append(Result);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

StmtResult Sema::ActOnStopStmt(ASTContext &C, SourceLocation Loc, ExprResult StopCode, Expr *StmtLabel) {
  auto Result = StopStmt::Create(C, Loc, StopCode.take(), StmtLabel);
  getCurrentBody()->Append(Result);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

StmtResult Sema::ActOnReturnStmt(ASTContext &C, SourceLocation Loc, ExprResult E, Expr *StmtLabel) {
  if(!IsInsideFunctionOrSubroutine()) {
    Diags.Report(Loc, diag::err_stmt_not_in_func) << "RETURN";
    return StmtError();
  }
  auto Result = ReturnStmt::Create(C, Loc, E.take(), StmtLabel);
  getCurrentBody()->Append(Result);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

StmtResult Sema::ActOnCallStmt(ASTContext &C, SourceLocation Loc, SourceLocation RParenLoc,
                               SourceRange IdRange,
                               const IdentifierInfo *IDInfo,
                               ArrayRef<Expr*> Arguments, Expr *StmtLabel) {
  auto Prev = ResolveIdentifier(IDInfo);
  FunctionDecl *Function;
  if(Prev && Prev->getDeclContext() == CurContext) {
    Function = dyn_cast<FunctionDecl>(Prev);
    if(!Function) {
      Diags.Report(Loc, diag::err_call_requires_subroutine)
        << /* intrinsicfunction|variable= */ (isa<IntrinsicFunctionDecl>(Prev)? 1: 0)
        << IDInfo << IdRange;
      return StmtError();
    }
  } else {
    Function = dyn_cast_or_null<FunctionDecl>(Prev);
    if(!Function) {
      // an implicit function declaration.
      Function = FunctionDecl::Create(Context, FunctionDecl::External, CurContext,
                                      DeclarationNameInfo(IDInfo, IdRange.Start), QualType());
      CurContext->addDecl(Function);
    }
  }

  if(Function->isNormalFunction() || Function->isStatementFunction()) {
      Diags.Report(Loc, diag::err_call_requires_subroutine)
        << /* function= */ 2 << IDInfo << IdRange;
      return StmtError();
  }

  CheckCallArgumentCount(Function, Arguments, RParenLoc, IdRange);

  auto Result = CallStmt::Create(C, Loc, Function, Arguments, StmtLabel);
  getCurrentBody()->Append(Result);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

} // end namespace flang
