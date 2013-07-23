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
  case tok::kw_DOWHILE:
    return ParseDoWhileStmt();
  case tok::kw_ENDDO:
    return ParseEndDoStmt();
  case tok::kw_CONTINUE:
    return ParseContinueStmt();
  case tok::kw_STOP:
    return ParseStopStmt();
  case tok::kw_PRINT:
    return ParsePrintStmt();
  case tok::kw_WRITE:
    return ParseWriteStmt();
  case tok::kw_FORMAT:
    return ParseFORMATStmt();
  case tok::kw_RETURN:
    return ParseReturnStmt();
  case tok::kw_CALL:
    return ParseCallStmt();
  case tok::kw_READ:
  case tok::kw_OPEN:
  case tok::kw_CLOSE:
  case tok::kw_INQUIRE:
  case tok::kw_BACKSPACE:
  case tok::kw_ENDFILE:
  case tok::kw_REWIND:
    Diag.Report(ConsumeToken(), diag::err_unsupported_stmt);
    return StmtError();


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

Parser::StmtResult Parser::ParseAssignStmt() {
  SourceLocation Loc = ConsumeToken();

  auto Value = ParseStatementLabelReference(false);
  if(Value.isInvalid()) {
    Diag.Report(getExpectedLoc(), diag::err_expected_stmt_label_after)
        << "ASSIGN";
    return StmtError();
  }
  SetNextTokenShouldBeKeyword();
  ConsumeToken();
  if(!ExpectAndConsume(tok::kw_TO, diag::err_expected_kw, "TO"))
    return StmtError();

  auto IDInfo = Tok.getIdentifierInfo();
  auto IDLoc = Tok.getLocation();
  if(!ExpectAndConsume(tok::identifier))
    return StmtError();
  auto VD = Actions.ExpectVarRefOrDeclImplicitVar(IDLoc, IDInfo);
  if(!VD)
    return StmtError();
  auto Var = VarExpr::Create(Context, IDLoc, VD);

  return Actions.ActOnAssignStmt(Context, Loc, Value, Var, StmtLabel);
}

Parser::StmtResult Parser::ParseGotoStmt() {
  SourceLocation Loc = ConsumeToken();

  auto Destination = ParseStatementLabelReference();
  if(Destination.isInvalid()) {
    if(!IsPresent(tok::identifier)) {
      Diag.Report(getExpectedLoc(), diag::err_expected_stmt_label_after)
          << "GO TO";
      return StmtError();
    }
    auto IDInfo = Tok.getIdentifierInfo();
    auto IDLoc = ConsumeToken();
    auto VD = Actions.ExpectVarRef(IDLoc, IDInfo);
    if(!VD) return StmtError();
    auto Var = VarExpr::Create(Context, IDLoc, VD);

    // Assigned goto
    SmallVector<Expr*, 4> AllowedValues;
    if(ConsumeIfPresent(tok::l_paren)) {
      do {
        auto E = ParseStatementLabelReference();
        if(E.isInvalid()) {
          Diag.Report(getExpectedLoc(), diag::err_expected_stmt_label);
          SkipUntilNextStatement();
          return Actions.ActOnAssignedGotoStmt(Context, Loc, Var, AllowedValues, StmtLabel);
        }
        AllowedValues.append(1, E.get());
      } while(ConsumeIfPresent(tok::comma));
      ExpectAndConsume(tok::r_paren);
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
  auto Loc = ConsumeToken();

  if (!ExpectAndConsume(tok::l_paren, diag::err_expected_lparen_after, "IF")) {
    SkipUntilNextStatement();
    return StmtError();
  }
  ExprResult Condition = ParseExpectedFollowupExpression("(");
  if(Condition.isInvalid()) return StmtError();
  SetNextTokenShouldBeKeyword();
  if (!ExpectAndConsume(tok::r_paren)) {
    SkipUntilNextStatement();
    return StmtError();
  }
  if (!ConsumeIfPresent(tok::kw_THEN)){
    // if-stmt
    if(Tok.isAtStartOfStatement()) {
      Diag.Report(getExpectedLoc(), diag::err_expected_executable_stmt);
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
  auto Loc = ConsumeToken();

  ExprResult Condition = ParseExpectedConditionExpression("ELSE IF");
  if(Condition.isInvalid()) return StmtError();
  if(!ExpectAndConsume(tok::kw_THEN, diag::err_expected_kw, "THEN"))
    SkipUntilNextStatement();
  return Actions.ActOnElseIfStmt(Context, Loc, Condition, StmtLabel);
}

Parser::StmtResult Parser::ParseElseStmt() {
  auto Loc = ConsumeToken();
  return Actions.ActOnElseStmt(Context, Loc, StmtLabel);
}

Parser::StmtResult Parser::ParseEndIfStmt() {
  auto Loc = ConsumeToken();
  return Actions.ActOnEndIfStmt(Context, Loc, StmtLabel);
}

// FIXME: improve error recovery
Parser::StmtResult Parser::ParseDoStmt() {
  auto Loc = ConsumeToken();

  ExprResult TerminalStmt;
  if(Tok.is(tok::int_literal_constant)) {
    TerminalStmt = ParseStatementLabelReference();
    if(TerminalStmt.isInvalid()) return StmtError();
  }

  // the do var
  auto IDInfo = Tok.getIdentifierInfo();
  auto IDLoc = Tok.getLocation();
  if(!ExpectAndConsume(tok::identifier))
    return StmtError();
  auto VD = Actions.ExpectVarRefOrDeclImplicitVar(IDLoc, IDInfo);
  if(!VD)
    return StmtError();
  auto DoVar = VarExpr::Create(Context, IDLoc, VD);

  if(!ExpectAndConsume(tok::equal))
    return StmtError();
  auto E1 = ParseExpectedFollowupExpression("=");
  if(E1.isInvalid()) return StmtError();
  if(!ExpectAndConsume(tok::comma))
    return StmtError();
  auto E2 = ParseExpectedFollowupExpression(",");
  if(E2.isInvalid()) return StmtError();
  ExprResult E3;
  if(ConsumeIfPresent(tok::comma)) {
    E3 = ParseExpectedFollowupExpression(",");
    if(E3.isInvalid()) return StmtError();
  }

  return Actions.ActOnDoStmt(Context, Loc, TerminalStmt,
                             DoVar, E1, E2, E3, StmtLabel);
}

Parser::StmtResult Parser::ParseDoWhileStmt() {
  auto Loc = ConsumeToken();
  auto Condition = ParseExpectedConditionExpression("WHILE");
  if(Condition.isInvalid()) return StmtError();
  return Actions.ActOnDoWhileStmt(Context, Loc, Condition, StmtLabel);
}

Parser::StmtResult Parser::ParseEndDoStmt() {
  auto Loc = ConsumeToken();
  return Actions.ActOnEndDoStmt(Context, Loc, StmtLabel);
}

/// ParseContinueStmt
///   [R839]:
///     continue-stmt :=
///       CONTINUE
Parser::StmtResult Parser::ParseContinueStmt() {
  auto Loc = ConsumeToken();

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
  auto Loc = ConsumeToken();

  //FIXME: parse optional stop-code.
  return Actions.ActOnStopStmt(Context, Loc, ExprResult(), StmtLabel);
}

Parser::StmtResult Parser::ParseReturnStmt() {
  auto Loc = ConsumeToken();
  ExprResult E;
  if(!Tok.isAtStartOfStatement())
    E = ParseExpression();

  return Actions.ActOnReturnStmt(Context, Loc, E, StmtLabel);
}

Parser::StmtResult Parser::ParseCallStmt() {
  auto Loc = ConsumeToken();
  SourceLocation RParenLoc = getExpectedLoc();

  auto ID = Tok.getIdentifierInfo();
  auto FuncIdRange = SourceRange(Tok.getLocation(),
                                 getMaxLocationOfCurrentToken());
  if(!ExpectAndConsume(tok::identifier))
    return StmtError();

  auto Decl = Actions.ResolveIdentifier(ID);
  auto FD = dyn_cast_or_null<FunctionDecl>(Decl);
  if(!FD) {
    Diag.Report(getExpectedLoc(), diag::err_expected_func_after)
      << "CALL";
    return StmtError();
  }

  SmallVector<Expr*, 8> Arguments;
  if(!Tok.isAtStartOfStatement()) {
    if(ParseFunctionCallArgumentList(Arguments, RParenLoc).isInvalid())
      SkipUntilNextStatement();
  }

  return Actions.ActOnCallStmt(Context, Loc, RParenLoc, FuncIdRange, FD, Arguments, StmtLabel);
}

/// ParseAssignmentStmt
///   [R732]:
///     assignment-stmt :=
///         variable = expr
Parser::StmtResult Parser::ParseAssignmentStmt() {
  ExprResult LHS = ParsePrimaryExpr(true);
  if(LHS.isInvalid()) return StmtError();

  SourceLocation Loc = Tok.getLocation();
  if(!EatIfPresentInSameStmt(tok::equal)) {
    Diag.Report(getExpectedLoc(),diag::err_expected_equal);
    return StmtError();
  }

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
  SourceLocation Loc = ConsumeToken();

  FormatSpec *FS = ParseFMTSpec(false);
  if(!ExpectAndConsume(tok::comma))
    return StmtError();

  SmallVector<ExprResult, 4> OutputItemList;
  ParseIOList(OutputItemList);

  return Actions.ActOnPrintStmt(Context, Loc, FS, OutputItemList, StmtLabel);
}

Parser::StmtResult Parser::ParseWriteStmt() {
  SourceLocation Loc = ConsumeToken();

  // clist
  if(!EatIfPresentInSameStmt(tok::l_paren)) {
    Diag.Report(getExpectedLoc(), diag::err_expected_lparen);
  }

  UnitSpec *US = nullptr;
  FormatSpec *FS = nullptr;

  US = ParseUNITSpec(false);
  if(EatIfPresentInSameStmt(tok::comma)) {
    bool IsFormatLabeled = false;
    if(EatIfPresentInSameStmt(tok::kw_FMT)) {
      if(!EatIfPresentInSameStmt(tok::equal)) {
        Diag.Report(getExpectedLoc(), diag::err_expected_equal);
        return StmtError();
      }
      IsFormatLabeled = true;
    }
    FS = ParseFMTSpec(IsFormatLabeled);

  }

  if(!EatIfPresentInSameStmt(tok::r_paren)) {
    Diag.Report(getExpectedLoc(), diag::err_expected_rparen);
  }

  // iolist
  SmallVector<ExprResult, 4> OutputItemList;
  ParseIOList(OutputItemList);

  return Actions.ActOnWriteStmt(Context, Loc, US, FS, OutputItemList, StmtLabel);
}

UnitSpec *Parser::ParseUNITSpec(bool IsLabeled) {
  auto Loc = Tok.getLocation();
  if(!EatIfPresentInSameStmt(tok::star)) {
    auto E = ParseExpression();
    if(!E.isInvalid())
      return Actions.ActOnUnitSpec(Context, E, Loc, IsLabeled);
  }
  return Actions.ActOnStarUnitSpec(Context, Loc, IsLabeled);
}

FormatSpec *Parser::ParseFMTSpec(bool IsLabeled) {
  auto Loc = Tok.getLocation();
  if(!ConsumeIfPresent(tok::star)) {
    if(Tok.is(tok::int_literal_constant)) {
      auto Destination = ParseStatementLabelReference();
      if(!Destination.isInvalid())
        return Actions.ActOnLabelFormatSpec(Context, Loc, Destination);
    }
    auto E = ParseExpression();
    if(E.isUsable())
      return Actions.ActOnExpressionFormatSpec(Context, Loc, E.get());
    // NB: return empty format string on error.
    return Actions.ActOnExpressionFormatSpec(Context, Loc, CharacterConstantExpr::Create(Context, Loc, Loc, ""));
  }

  return Actions.ActOnStarFormatSpec(Context, Loc);
}

void Parser::ParseIOList(SmallVectorImpl<ExprResult> &List) {
  while(!Tok.isAtStartOfStatement()) {
    auto E = ParseExpression();
    if(E.isUsable()) List.push_back(E);
    if(!EatIfPresentInSameStmt(tok::comma))
      break;
  }
}

/// ParseEND_PROGRAMStmt - Parse the END PROGRAM statement.
///
///   [R1103]:
///     end-program-stmt :=
///         END [ PROGRAM [ program-name ] ]
Parser::StmtResult Parser::ParseEND_PROGRAMStmt() {
  SourceLocation Loc = Tok.getLocation();
  if (Tok.isNot(tok::kw_END) && Tok.isNot(tok::kw_ENDPROGRAM)) {
    Diag.Report(Tok.getLocation(),diag::err_expected_stmt)
      << "END PROGRAM";
    return StmtError();
  }
  Lex();

  const IdentifierInfo *IDInfo = 0;
  SourceLocation NameLoc;
  if (Tok.is(tok::identifier) && !Tok.isAtStartOfStatement()) {
    IDInfo = Tok.getIdentifierInfo();
    NameLoc = Tok.getLocation();
    Lex(); // Eat the ending token.
  }

  return Actions.ActOnENDPROGRAM(Context, IDInfo, Loc, NameLoc, StmtLabel);
}

} //namespace flang
