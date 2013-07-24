//= SemaExecStmt.cpp - AST Builder and Checker for the executable statements =//
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
#include "flang/Sema/SemaInternal.h"
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

StmtResult Sema::ActOnElseIfStmt(ASTContext &C, SourceLocation Loc,
                                 ExprResult Condition, Expr *StmtLabel) {
  if(!getCurrentBody()->HasEntered() ||
     !getCurrentBody()->LastEntered().is(Stmt::IfStmtClass)) {
    if(getCurrentBody()->HasEntered(Stmt::IfStmtClass))
      ReportUnterminatedStmt( getCurrentBody()->LastEntered(), Loc);
    else
      Diags.Report(Loc, diag::err_stmt_not_in_if) << "ELSE IF";
    return StmtError();
  }

  // typecheck
  if(Condition.isUsable())
    StmtRequiresLogicalExpression(Loc, Condition.get());

  auto Result = IfStmt::Create(C, Loc, Condition.get(), StmtLabel);
  auto ParentIf = cast<IfStmt>(getCurrentBody()->LastEntered().Statement);
  getCurrentBody()->Leave(C);
  if(Condition.isUsable())
    ParentIf->setElseStmt(Result);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  getCurrentBody()->Enter(Result);
  return Result;
}

StmtResult Sema::ActOnElseStmt(ASTContext &C, SourceLocation Loc, Expr *StmtLabel) {
  if(!getCurrentBody()->HasEntered() ||
     !getCurrentBody()->LastEntered().is(Stmt::IfStmtClass)) {
    if(getCurrentBody()->HasEntered(Stmt::IfStmtClass))
      ReportUnterminatedStmt(getCurrentBody()->LastEntered(), Loc);
    else
      Diags.Report(Loc, diag::err_stmt_not_in_if) << "ELSE";
    return StmtError();
  }
  auto Result = ConstructPartStmt::Create(C, ConstructPartStmt::ElseStmtClass, Loc, nullptr, StmtLabel);
  getCurrentBody()->Append(Result);
  getCurrentBody()->LeaveIfThen(C);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

StmtResult Sema::ActOnEndIfStmt(ASTContext &C, SourceLocation Loc, Expr *StmtLabel) {
  // Report begin .. end mismatch
  if(!getCurrentBody()->HasEntered() ||
     !getCurrentBody()->LastEntered().is(Stmt::IfStmtClass)) {
    if(getCurrentBody()->HasEntered(Stmt::IfStmtClass))
      ReportUnterminatedStmt(getCurrentBody()->LastEntered(), Loc);
    else
      Diags.Report(Loc, diag::err_stmt_not_in_if) << "END IF";
    return StmtError();
  }

  auto Result = ConstructPartStmt::Create(C, ConstructPartStmt::EndIfStmtClass, Loc, nullptr, StmtLabel);
  getCurrentBody()->Append(Result);
  getCurrentBody()->Leave(C);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
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

/// FIXME: Fortran 90+: make multiple do end at one label obsolete
void Sema::CheckStatementLabelEndDo(Expr *StmtLabel, Stmt *S) {
  if(!getCurrentBody()->HasEntered())
    return;

  auto I = getCurrentBody()->ControlFlowStack.size();
  size_t LastUnterminated = 0;
  do {
    I--;
    if(!isa<DoStmt>(getCurrentBody()->ControlFlowStack[I].Statement)) {
      if(!LastUnterminated) LastUnterminated = I;
    } else {
      auto ParentDo = cast<DoStmt>(getCurrentBody()->ControlFlowStack[I].Statement);
      auto ParentDoExpectedLabel = getCurrentBody()->ControlFlowStack[I].ExpectedEndDoLabel;
      if(!ParentDoExpectedLabel) {
        if(!LastUnterminated) LastUnterminated = I;
      } else {
        if(getCurrentStmtLabelScope()->IsSame(ParentDoExpectedLabel, StmtLabel)) {
          // END DO
          getCurrentStmtLabelScope()->RemoveForwardReference(ParentDo);
          if(!IsValidDoTerminatingStatement(S)) {
            Diags.Report(S->getLocation(),
                         diag::err_invalid_do_terminating_stmt);
          }
          ParentDo->setTerminatingStmt(StmtLabelReference(S));
          if(!LastUnterminated) {
            RemoveLoopVar(ParentDo->getDoVar());
            getCurrentBody()->Leave(Context);
          }
          else
            ReportUnterminatedStmt(getCurrentBody()->ControlFlowStack[LastUnterminated],
                                   S->getLocation());
        } else if(!LastUnterminated) LastUnterminated = I;
      }
    }
  } while(I>0);
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

StmtResult Sema::ActOnEndDoStmt(ASTContext &C, SourceLocation Loc, Expr *StmtLabel) {
  // Report begin .. end mismatch
  if(!getCurrentBody()->HasEntered() ||
     (!isa<DoStmt>(getCurrentBody()->LastEntered().Statement) &&
      !isa<DoWhileStmt>(getCurrentBody()->LastEntered().Statement))) {
    bool HasMatchingDo = false;
    for(auto I : getCurrentBody()->ControlFlowStack) {
      if(isa<DoWhileStmt>(I.Statement) ||
        (isa<DoStmt>(I.Statement) && !I.ExpectedEndDoLabel)) {
        HasMatchingDo = true;
        break;
      }
    }
    if(!HasMatchingDo)
      Diags.Report(Loc, diag::err_end_do_without_do);
    else ReportUnterminatedStmt(getCurrentBody()->LastEntered(), Loc);
    return StmtError();
  }

  auto Last = getCurrentBody()->LastEntered();

  if(auto Do = dyn_cast<DoStmt>(Last.Statement)) {
    // If last loop was a DO with terminating label, we expect it to finish before this loop
    if(getCurrentBody()->LastEntered().ExpectedEndDoLabel) {
      //FIXME: ReportUnterminatedLabeledDoStmt(getCurrentBody()->LastEntered(), Loc);
      return StmtError();
    }
    RemoveLoopVar(Do->getDoVar());
  }

  auto Result = ConstructPartStmt::Create(C, ConstructPartStmt::EndDoStmtClass, Loc, nullptr, StmtLabel);
  getCurrentBody()->Append(Result);
  getCurrentBody()->Leave(C);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
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
                               FunctionDecl *Function,
                               ArrayRef<Expr*> Arguments, Expr *StmtLabel) {
  CheckCallArgumentCount(Function, Arguments, RParenLoc, IdRange);

  auto Result = CallStmt::Create(C, Loc, Function, Arguments, StmtLabel);
  getCurrentBody()->Append(Result);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

} // end namespace flang
