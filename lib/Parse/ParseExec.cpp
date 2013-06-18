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
  case tok::kw_CONTINUE:
    return ParseContinueStmt();
  case tok::kw_STOP:
    return ParseStopStmt();
  case tok::kw_PRINT:
    return ParsePrintStmt();

  case tok::kw_END:
    // TODO: All of the end-* stmts.
  case tok::kw_ELSE:
  case tok::kw_ELSEIF:
  case tok::kw_ENDIF:
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
    Diag.Report(Tok.getLocation(), diag::err_expected_stmt_label_after)
        << "GO TO";
    return StmtError();
  }
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
Parser::StmtResult Parser::ParseIfStmt() {
  SMLoc Loc = Tok.getLocation();

  Lex();
  if (!EatIfPresent(tok::l_paren)) {
    Diag.Report(Tok.getLocation(), diag::err_expected_lparen_after)
        << "IF";
    return StmtError();
  }
  ExprResult Condition = ParseExpression();
  if(Condition.isInvalid()) return StmtError();
  if (!EatIfPresent(tok::r_paren)) {
    Diag.Report(Tok.getLocation(), diag::err_expected_rparen);
    return StmtError();
  }
  if (!EatIfPresent(tok::kw_THEN)){
    // if-stmt
    auto Action = ParseActionStmt();
    if(Action.isInvalid()) return Action;

    return Actions.ActOnIfStmt(Context, Loc, Condition, Action,
                               StmtLabel);
  }

  //FIXME: if-construct-name
  // if-then-stmt
  std::vector<std::pair<ExprResult,StmtResult> > Branches;
  if(!Tok.is(tok::kw_ENDIF) && !Tok.is(tok::kw_ELSE)
     && !Tok.is(tok::kw_ELSEIF)) {
    auto Body = ParseBlockStmt();
    if(Body.isInvalid()) return Body;
    Branches.push_back(std::make_pair(Condition,Body));
  }
  else
    Branches.push_back(std::make_pair(Condition,StmtResult()));

  // else-if-stmt
  while(EatIfPresent(tok::kw_ELSEIF)) {
    if (!EatIfPresent(tok::l_paren)) {
      Diag.Report(Tok.getLocation(), diag::err_expected_lparen_after)
          << "ELSE IF";
      return StmtError();
    }
    Condition = ParseExpression();
    if(Condition.isInvalid()) return StmtError();
    if (!EatIfPresent(tok::r_paren)) {
      Diag.Report(Tok.getLocation(), diag::err_expected_rparen);
      return StmtError();
    }
    if (!EatIfPresent(tok::kw_THEN)) {
      Diag.Report(Tok.getLocation(), diag::err_expected_kw)
          << "THEN";
      return StmtError();
    }
    if(!Tok.is(tok::kw_ENDIF) && !Tok.is(tok::kw_ELSE)
       && !Tok.is(tok::kw_ELSEIF)) {
      auto Body = ParseBlockStmt();
      if(Body.isInvalid()) return Body;
      Branches.push_back(std::make_pair(Condition,Body));
    }
    else
      Branches.push_back(std::make_pair(Condition,StmtResult()));
  }

  // else-stmt
  if(EatIfPresent(tok::kw_ELSE)) {
    if(!Tok.is(tok::kw_ENDIF)) {
      auto Body = ParseBlockStmt();
      if(Body.isInvalid()) return Body;
      Branches.push_back(std::make_pair(ExprResult(),Body));
    }
  }

  // end-if-stmt
  if (!EatIfPresent(tok::kw_ENDIF)) {
    Diag.Report(Tok.getLocation(), diag::err_expected_kw)
        << "END IF";
    return StmtError();
  }

  return Actions.ActOnIfStmt(Context, Loc, Branches, StmtLabel);
}

/// ParseContinueStmt
///   [R839]:
///     continue-stmt :=
///       CONTINUE
Parser::StmtResult Parser::ParseContinueStmt() {
  SMLoc Loc = Tok.getLocation();
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
  SMLoc Loc = Tok.getLocation();
  Lex();

  //FIXME: parse optional stop-code.
  return Actions.ActOnStopStmt(Context, Loc, ExprResult(), StmtLabel);
}

/// ParseAssignmentStmt
///   [R732]:
///     assignment-stmt :=
///         variable = expr
Parser::StmtResult Parser::ParseAssignmentStmt() {
  ExprResult LHS = ParseExpression();
  if(LHS.isInvalid()) return StmtError();

  llvm::SMLoc Loc = Tok.getLocation();
  if(!Tok.is(tok::equal)) {
    Diag.ReportError(Tok.getLocation(),"expected '='");
    return StmtError();
  }
  Lex();

  ExprResult RHS = ParseExpression();
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
    Diag.ReportError(Tok.getLocation(),
                     "expected 'END PROGRAM' statement");
    return StmtResult();
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
