//===-- ParseExec.cpp - Parse Executable Construct ------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Functions to parse the executable construct (R213).
//
//===----------------------------------------------------------------------===//

#include "flang/Parse/Parser.h"
#include "flang/Parse/ParseDiagnostic.h"
#include "flang/AST/Decl.h"
#include "flang/AST/Expr.h"
#include "flang/AST/Stmt.h"
#include "flang/Basic/TokenKinds.h"
#include "flang/Sema/DeclSpec.h"
#include "flang/Sema/Sema.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"

namespace flang {

/// ParseExecutableConstruct - Parse the executable construct.
///
///   [R213]:
///     executable-construct :=
///         action-stmt
///      or associate-construct
///      or block-construct
///      or case-construct
///      or critical-construct
///      or do-construct
///      or forall-construct
///      or if-construct
///      or select-type-construct
///      or where-construct
StmtResult Parser::ParseExecutableConstruct() {
  StmtResult SR = ParseActionStmt();
  if (SR.isInvalid()) return StmtError();
  if (!SR.isUsable()) return StmtResult();

  return SR;
}

/// ParseActionStmt - Parse an action statement.
///
///   [R214]:
///     action-stmt :=
///         allocate-stmt
///      or assignment-stmt
///      or backspace-stmt
///      or call-stmt
///      or close-stmt
///      or continue-stmt
///      or cycle-stmt
///      or deallocate-stmt
///      or end-function-stmt
///      or end-mp-subprogram-stmt
///      or end-program-stmt
///      or end-subroutine-stmt
///      or endfile-stmt
///      or error-stop-stmt
///      or exit-stmt
///      or flush-stmt
///      or forall-stmt
///      or goto-stmt
///      or if-stmt
///      or inquire-stmt
///      or lock-stmt
///      or nullify-stmt
///      or open-stmt
///      or pointer-assignment-stmt
///      or print-stmt
///      or read-stmt
///      or return-stmt
///      or rewind-stmt
///      or stop-stmt
///      or sync-all-stmt
///      or sync-images-stmt
///      or sync-memory-stmt
///      or unlock-stmt
///      or wait-stmt
///      or where-stmt
///      or write-stmt
///[obs] or arithmetic-if-stmt
///[obs] or computed-goto-stmt
Parser::StmtResult Parser::ParseActionStmt() {
  ParseStatementLabel();

  // This is an assignment.
  const Token &NextTok = PeekAhead();
  if (Tok.getIdentifierInfo() && !NextTok.isAtStartOfStatement() &&
      NextTok.is(tok::equal))
    return ParseAssignmentStmt();
      
  StmtResult SR;
  switch (Tok.getKind()) {
  default:
    return ParseAssignmentStmt();
  case tok::kw_ASSIGN:
    return ParseAssignStmt();
  case tok::kw_GOTO:
    return ParseGotoStmt();
  case tok::kw_IF:
    return ParseIfStmt();
  case tok::kw_ELSEIF:
    return ParseElseIfStmt();
  case tok::kw_ELSE:
    return ParseElseStmt();
  case tok::kw_ENDIF:
    return ParseEndIfStmt();
  case tok::kw_DO:
    return ParseDoStmt();
  case tok::kw_ENDDO:
    return ParseEndDoStmt();
  case tok::kw_CONTINUE:
    return ParseContinueStmt();
  case tok::kw_STOP:
    return ParseStopStmt();
  case tok::kw_PRINT:
    return ParsePrintStmt();

  case tok::eof:
  case tok::kw_END:
    // TODO: All of the end-* stmts.
  case tok::kw_ENDFUNCTION:
  case tok::kw_ENDPROGRAM:
  case tok::kw_ENDSUBPROGRAM:
  case tok::kw_ENDSUBROUTINE:
    // Handle in parent.
    return StmtResult();
  }

  return SR;
}

/// ParseBlockStmt
///   [R801]:
///     block :=
///       execution-part-construct ..
StmtResult Parser::ParseBlockStmt() {
  SMLoc Loc = Tok.getLocation();

  std::vector<StmtResult> body;
  if(ParseExecutionPart(body))
    return StmtResult(true);
  return Actions.ActOnBlock(Context, Loc, body);
}

Parser::StmtResult Parser::ParseAssignStmt() {
  SMLoc Loc = Tok.getLocation();
  Lex();

  auto Value = ParseStatementLabelReference();
  if(Value.isInvalid()) {
    Diag.Report(Tok.getLocation(), diag::err_expected_stmt_label_after)
        << "ASSIGN";
    return StmtError();
  }
  if(!EatIfPresent(tok::kw_TO)) {
    Diag.Report(Tok.getLocation(), diag::err_expected_kw)
        << "TO";
    return StmtError();
  }
  auto Var = ParseIntegerVariableReference();
  if(!Var) {
    Diag.Report(Tok.getLocation(), diag::err_expected_int_var)
        << "TO";
    return StmtError();
  }
  return Actions.ActOnAssignStmt(Context, Loc, Value, Var, StmtLabel);
}

Parser::StmtResult Parser::ParseGotoStmt() {
  SMLoc Loc = Tok.getLocation();
  Lex();

  auto Destination = ParseStatementLabelReference();
  if(Destination.isInvalid()) {
    auto Var = ParseIntegerVariableReference();
    if(!Var) {
      Diag.Report(Tok.getLocation(), diag::err_expected_stmt_label_after)
          << "GO TO";
      return StmtError();
    }

    // Assigned goto
    SmallVector<ExprResult, 4> AllowedValues;
    if(EatIfPresent(tok::l_paren)) {
      do {
        auto E = ParseStatementLabelReference();
        if(E.isInvalid()) {
          Diag.Report(Tok.getLocation(), diag::err_expected_stmt_label);
          return StmtError();
        }
        AllowedValues.append(1, E);
      } while(EatIfPresent(tok::comma));
      if(!EatIfPresent(tok::r_paren))
        Diag.Report(Tok.getLocation(), diag::err_expected_rparen);
    }
    return Actions.ActOnAssignedGotoStmt(Context, Loc, Var, AllowedValues, StmtLabel);
  }
  // Uncoditional goto
  return Actions.ActOnGotoStmt(Context, Loc, Destination, StmtLabel);
}

/// ParseIfStmt
///   [R802]:
///     if-construct :=
///       if-then-stmt
///         block
///       [else-if-stmt
///          block
///       ]
///       ...
///       [
///       else-stmt
///          block
///       ]
///       end-if-stmt
///   [R803]:
///     if-then-stmt :=
///       [ if-construct-name : ]
///       IF (scalar-logical-expr) THEN
///   [R804]:
///     else-if-stmt :=
///       ELSE IF (scalar-logical-expr) THEN
///       [ if-construct-name ]
///   [R805]:
///     else-stmt :=
///       ELSE
///       [ if-construct-name ]
///   [R806]:
///     end-if-stmt :=
///       END IF
///       [ if-construct-name ]
///
///   [R807]:
///     if-stmt :=
///       IF(scalar-logic-expr) action-stmt

ExprResult Parser::ParseExpectedConditionExpression(const char *DiagAfter) {
  if (!EatIfPresent(tok::l_paren)) {
    Diag.Report(Tok.getLocation(), diag::err_expected_lparen_after)
        << DiagAfter;
    return ExprError();
  }
  ExprResult Condition = ParseExpectedFollowupExpression("(");
  if(Condition.isInvalid()) return Condition;
  if (!EatIfPresent(tok::r_paren)) {
    Diag.Report(Tok.getLocation(), diag::err_expected_rparen);
    return ExprError();
  }
  return Condition;
}

Parser::StmtResult Parser::ParseIfStmt() {
  auto Loc = Tok.getLocation();
  Lex();

  ExprResult Condition = ParseExpectedConditionExpression("IF");
  if(Condition.isInvalid()) return StmtError();
  if (!EatIfPresent(tok::kw_THEN)){
    // if-stmt
    if(Tok.isAtStartOfStatement()) {
      Diag.Report(Tok.getLocation(), diag::err_expected_executable_stmt);
      return StmtError();
    }
    auto Result = Actions.ActOnIfStmt(Context, Loc, Condition, StmtLabel);
    if(Result.isInvalid()) return Result;
    // NB: Don't give the action stmt my label
    StmtLabel = nullptr;
    auto Action = ParseActionStmt();
    Actions.ActOnEndIfStmt(Context, Loc, nullptr);
    return Action.isInvalid()? StmtError() : Result;
  }

  // if-construct.
  return Actions.ActOnIfStmt(Context, Loc, Condition, StmtLabel);
}

Parser::StmtResult Parser::ParseElseIfStmt() {
  auto Loc = Tok.getLocation();
  Lex();

  ExprResult Condition = ParseExpectedConditionExpression("ELSE IF");
  if(Condition.isInvalid()) return StmtError();
  if (!EatIfPresent(tok::kw_THEN)) {
    Diag.Report(Tok.getLocation(), diag::err_expected_kw)
        << "THEN";
    return StmtError();
  }
  return Actions.ActOnElseIfStmt(Context, Loc, Condition, StmtLabel);
}

Parser::StmtResult Parser::ParseElseStmt() {
  auto Loc = Tok.getLocation();
  Lex();
  return Actions.ActOnElseStmt(Context, Loc, StmtLabel);
}

Parser::StmtResult Parser::ParseEndIfStmt() {
  auto Loc = Tok.getLocation();
  Lex();
  return Actions.ActOnEndIfStmt(Context, Loc, StmtLabel);
}

Parser::StmtResult Parser::ParseDoStmt() {
  auto Loc = Tok.getLocation();
  Lex();

  ExprResult TerminalStmt;
  if(Tok.is(tok::int_literal_constant)) {
    TerminalStmt = ParseStatementLabelReference();
    if(TerminalStmt.isInvalid()) return StmtError();
  }
  auto DoVar = ParseVariableReference();
  if(!DoVar) {
    Diag.Report(Tok.getLocation(),diag::err_expected_do_var);
    return StmtError();
  }
  if(!EatIfPresent(tok::equal)) {
    Diag.Report(Tok.getLocation(),diag::err_expected_equal);
    return StmtError();
  }
  auto E1 = ParseExpectedFollowupExpression("=");
  if(E1.isInvalid()) return StmtError();
  if(!EatIfPresent(tok::comma)) {
    Diag.Report(Tok.getLocation(),diag::err_expected_comma);
    return StmtError();
  }
  auto E2 = ParseExpectedFollowupExpression(",");
  if(E2.isInvalid()) return StmtError();
  ExprResult E3;
  if(EatIfPresent(tok::comma)) {
    E3 = ParseExpectedFollowupExpression(",");
    if(E3.isInvalid()) return StmtError();
  }

  return Actions.ActOnDoStmt(Context, Loc, TerminalStmt,
                             DoVar, E1, E2, E3, StmtLabel);
}

Parser::StmtResult Parser::ParseEndDoStmt() {
  auto Loc = Tok.getLocation();
  Lex();
  return Actions.ActOnEndDoStmt(Context, Loc, StmtLabel);
}

/// ParseContinueStmt
///   [R839]:
///     continue-stmt :=
///       CONTINUE
Parser::StmtResult Parser::ParseContinueStmt() {
  auto Loc = Tok.getLocation();
  Lex();

  return Actions.ActOnContinueStmt(Context, Loc, StmtLabel);
}

/// ParseStopStmt
///   [R840]:
///     stop-stmt :=
///       STOP [ stop-code ]
///   [R841]:
///     stop-code :=
///       scalar-char-constant or
///       digit [ digit [ digit [ digit [ digit ] ] ] ]
Parser::StmtResult Parser::ParseStopStmt() {
  auto Loc = Tok.getLocation();
  Lex();

  //FIXME: parse optional stop-code.
  return Actions.ActOnStopStmt(Context, Loc, ExprResult(), StmtLabel);
}

/// ParseAssignmentStmt
///   [R732]:
///     assignment-stmt :=
///         variable = expr
Parser::StmtResult Parser::ParseAssignmentStmt() {
  ExprResult LHS = ParsePrimaryExpr(true);
  if(LHS.isInvalid()) return StmtError();

  llvm::SMLoc Loc = Tok.getLocation();
  if(!Tok.is(tok::equal)) {
    Diag.Report(Tok.getLocation(),diag::err_expected_equal);
    return StmtError();
  }
  Lex();

  ExprResult RHS = ParseExpectedFollowupExpression("=");
  if(RHS.isInvalid()) return StmtError();
  return Actions.ActOnAssignmentStmt(Context, Loc, LHS, RHS, StmtLabel);
}

/// ParsePrintStatement
///   [R912]:
///     print-stmt :=
///         PRINT format [, output-item-list]
///   [R915]:
///     format :=
///         default-char-expr
///      or label
///      or *
Parser::StmtResult Parser::ParsePrintStmt() {
  SMLoc Loc = Tok.getLocation();
  Lex();

  SMLoc FormatLoc = Tok.getLocation();
  FormatSpec *FS = 0;
  if (EatIfPresent(tok::star)) {
    FS = Actions.ActOnStarFormatSpec(Context, FormatLoc);
  }

  // TODO: Parse the FORMAT default-char-expr & label.

  if (!EatIfPresent(tok::comma)) {
    Diag.ReportError(Tok.getLocation(),
                     "expected ',' after format specifier in PRINT statement");
    return StmtResult(true);
  }

  SmallVector<ExprResult, 4> OutputItemList;
  while (!Tok.isAtStartOfStatement()) {
    OutputItemList.push_back(ParseExpression());
    if (!EatIfPresent(tok::comma))
      break;
  }

  return Actions.ActOnPrintStmt(Context, Loc, FS, OutputItemList, StmtLabel);
}

/// ParseEND_PROGRAMStmt - Parse the END PROGRAM statement.
///
///   [R1103]:
///     end-program-stmt :=
///         END [ PROGRAM [ program-name ] ]
Parser::StmtResult Parser::ParseEND_PROGRAMStmt() {
  llvm::SMLoc Loc = Tok.getLocation();
  if (Tok.isNot(tok::kw_END) && Tok.isNot(tok::kw_ENDPROGRAM)) {
    Diag.Report(Tok.getLocation(),diag::err_expected_stmt)
      << "END PROGRAM";
    return StmtError();
  }
  Lex();

  const IdentifierInfo *IDInfo = 0;
  llvm::SMLoc NameLoc;
  if (Tok.is(tok::identifier) && !Tok.isAtStartOfStatement()) {
    IDInfo = Tok.getIdentifierInfo();
    NameLoc = Tok.getLocation();
    Lex(); // Eat the ending token.
  }

  return Actions.ActOnENDPROGRAM(Context, IDInfo, Loc, NameLoc, StmtLabel);
}

} //namespace flang
