//===-- ParserFormat.cpp - Fortran FORMAT Parser -------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Fortran FORMAT statement and specification parsing.
//
//===----------------------------------------------------------------------===//

#include "flang/Parse/Parser.h"
#include "flang/Parse/ParseDiagnostic.h"
#include "flang/AST/Expr.h"
#include "flang/Sema/Ownership.h"
#include "flang/Sema/Sema.h"

namespace flang {

/// ParseFORMATStmt - Parse the FORMAT statement.
///
///   [R1001]:
///     format-stmt :=
///         FORMAT format-specification
Parser::StmtResult Parser::ParseFORMATStmt() {
  auto Loc = Tok.getLocation();
  Lex();

  ParseFORMATSpec();

  return StmtError();
}

/// ParseFORMATSpec - Parses the FORMAT specification.
///   [R1002]:
///     format-specification :=
///         ( [ format-items ] )
///      or ( [ format-items, ] unlimited-format-item )
Parser::ExprResult Parser::ParseFORMATSpec() {
  if (!EatIfPresentInSameStmt(tok::l_paren)) {
    Diag.Report(getExpectedLoc(),diag::err_expected_lparen);
    return ExprError();
  }

  auto Items = ParseFORMATItems(true);
  if(Items.isInvalid()) return Items;
  if(!Items.isUsable()) {
    assert(Tok.is(tok::star));
    // FIXME: Act on unlimited
    // Parse unlimited-format-item
    Lex(); // eat '*'
    if (!EatIfPresentInSameStmt(tok::l_paren)) {
      Diag.Report(getExpectedLoc(),diag::err_expected_lparen);
      return ExprError();
    }
    return ParseFORMATItems(false);
  }
  return Items;
}

/// ParseFormatItems - Parses the FORMAT items.
Parser::ExprResult Parser::ParseFORMATItems(bool IsOuter) {
  do {
    if(Tok.isAtStartOfStatement()) {
      Diag.Report(getExpectedLoc(), diag::err_format_expected_desc);
      return ExprError();
    }
    if(IsOuter && Tok.is(tok::star))
      // Go back to the part to parse the unlimited-format-item
      return ExprResult();

    auto Item = ParseFORMATItem();
    if(Item.isInvalid()) {
      return ExprError();
    }

  } while(EatIfPresentInSameStmt(tok::comma));

  if (!EatIfPresentInSameStmt(tok::r_paren)) {
    Diag.Report(getExpectedLoc(),diag::err_expected_rparen)
      << FixItHint(getExpectedLocForFixIt(),")");
    return ExprError();
  }
  return ExprError();
}

/// ParseFORMATItem - Parses the FORMAT item.
Parser::ExprResult Parser::ParseFORMATItem() {
  if(EatIfPresent(tok::l_paren))
    return ParseFORMATItems();

  // char-string-edit-desc
  if(Tok.is(tok::char_literal_constant))
    return ParsePrimaryExpr();

  // R for data-edit-desc or
  // n for X in position-edit-desc
  ExprResult PreInt;
  if(Tok.is(tok::int_literal_constant)) {
    PreInt = ParseFORMATDescInt();
    if(Tok.isAtStartOfStatement()) {
      Diag.Report(getExpectedLoc(), diag::err_format_expected_desc);
      return ExprError();
    }
  }

  if(Tok.is(tok::l_paren)) {
    //FIXME: add the repeat count into account
    return ParseFORMATItems();
  }
  if(Tok.isNot(tok::identifier)) {
    Diag.Report(getExpectedLoc(), diag::err_format_expected_desc);
    return ExprError();
  }

  // data-edit-desc or position-edit-desc
  auto Loc = Tok.getLocation();
  auto Ident = Tok.getIdentifierInfo();
  Lex();
  auto DescIdent = Identifiers.lookupFormatSpec(Ident->getName());
  if(!DescIdent) {
    Diag.Report(Loc, diag::err_format_invalid_desc)
      << Ident;
    return ExprError();
  }

  auto Desc = DescIdent->getTokenID();
  ExprResult W, MD, E;
  switch(Desc) {
  // FIXME: fix '.' lexing/parsing
  // data-edit-desc
  case tok::fs_I: case tok::fs_B:
  case tok::fs_O: case tok::fs_Z:
    W = ParseFORMATDescInt();
    if(W.isInvalid()) break;
    return Actions.ActOnFORMATDataEditDesc(Context, Loc, Desc, PreInt,
                                           W, MD, E);

  case tok::fs_A:
    if(Tok.isNot(tok::comma) && Tok.isNot(tok::r_paren))
      W = ParseFORMATDescInt();
    return Actions.ActOnFORMATDataEditDesc(Context, Loc, Desc, PreInt,
                                           W, MD, E);

  // position-edit-desc
  case tok::fs_T: case tok::fs_TL: case tok::fs_TR:
    W = ParseFORMATDescInt();
    if(W.isInvalid()) break;
    return Actions.ActOnFORMATPositionEditDesc(Context, Loc, Desc, W);
  case tok::fs_X:
    if(!PreInt.isUsable()) {
      Diag.Report(Loc, diag::err_expected_int_literal_constant);
      break;
    }
    return Actions.ActOnFORMATPositionEditDesc(Context, Loc, Desc, PreInt);

  case tok::fs_SS: case tok::fs_SP: case tok::fs_S:
  case tok::fs_BN: case tok::fs_BZ:
  case tok::fs_RU: case tok::fs_RD: case tok::fs_RZ:
  case tok::fs_RN: case tok::fs_RC: case tok::fs_RP:
  case tok::fs_DC: case tok::fs_DP:
    if(PreInt.isUsable()) {
      // FIXME: proper error.
      Diag.Report(Loc, diag::err_expected_int_literal_constant);
    }
    return Actions.ActOnFORMATControlEditDesc(Context, Loc, Desc);

  // FIXME: add the rest..
  default:
    Diag.Report(Loc, diag::err_format_invalid_desc)
      << DescIdent;
    break;
  }
  return ExprError();
}

Parser::ExprResult Parser::ParseFORMATDescInt(const char *DiagAfter) {
  if(Tok.isAtStartOfStatement() || Tok.isNot(tok::int_literal_constant)) {
    if(DiagAfter)
      Diag.Report(getExpectedLoc(), diag::err_expected_int_literal_constant_after)
        << DiagAfter;
    else
      Diag.Report(getExpectedLoc(), diag::err_expected_int_literal_constant);
    return ExprError();
  }
  return ParsePrimaryExpr();
}

} // end namespace flang
