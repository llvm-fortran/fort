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
  LexFORMATTokens = true;
  Lex();


  ParseFORMATSpec();
  LexFORMATTokens = false;

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
    //if(Item.isInvalid()) {
    //  return ExprError();
    //}

  } while(EatIfPresentInSameStmt(tok::comma));

  if (!EatIfPresentInSameStmt(tok::r_paren)) {
    Diag.Report(getExpectedLoc(),diag::err_expected_rparen)
      << FixItHint(getExpectedLocForFixIt(),")");
    return ExprError();
  }
  return ExprError();
}


/// A helper class to parse FORMAT descriptor
class FormatDescriptorParser : public FormatDescriptorLexer {
  ASTContext &Context;
  DiagnosticsEngine &Diag;

public:
  FormatDescriptorParser(ASTContext &C, DiagnosticsEngine &D,
           const Lexer &Lex, const Token &FD)
   : Context(C), Diag(D), FormatDescriptorLexer(Lex, FD) {
  }

  Parser::ExprResult ParseIntExpr(const char *DiagAfter = nullptr) {
    llvm::StringRef Str;
    auto Loc = getCurrentLoc();
    if(!LexIntIfPresent(Str)){
      if(DiagAfter)
        Diag.Report(Loc, diag::err_expected_int_literal_constant_after)
          << DiagAfter;
      else
        Diag.Report(Loc, diag::err_expected_int_literal_constant);
      return ExprResult(true);
    }
    return IntegerConstantExpr::Create(Context, Loc,
                                       getCurrentLoc(), Str);
  }

  void MustBeDone() {
    if(!IsDone()) {
      auto Loc = getCurrentLoc();
      while(!IsDone()) ++Offset;
      Diag.Report(Loc, diag::err_format_desc_with_unparsed_end)
        << SourceRange(Loc, getCurrentLoc());
    }
  }
};

/// ParseFORMATItem - Parses the FORMAT item.
Parser::ExprResult Parser::ParseFORMATItem() {
  if(EatIfPresent(tok::l_paren))
    return ParseFORMATItems();

  // char-string-edit-desc
  if(Tok.is(tok::char_literal_constant))
    return ParsePrimaryExpr();

  if(Tok.is(tok::l_paren)) {
    //FIXME: add the repeat count into account
    return ParseFORMATItems();
  }
  if(Tok.is(tok::slash) || Tok.is(tok::colon)) {
    // FIXME: allow preint repeat count before slash.
    auto Loc = Tok.getLocation();
    auto Desc = Tok.getKind();
    Lex();
    return Actions.ActOnFORMATControlEditDesc(Context, Loc, Desc);
  }
  if(Tok.isNot(tok::format_descriptor)) {
    Diag.Report(getExpectedLoc(), diag::err_format_expected_desc);
    return ExprError();
  }

  // the identifier token could be something like I2,
  // so we need to split it into parts
  FormatDescriptorParser FDParser(Context, Diag,
                                  TheLexer, Tok);

  auto Loc = Tok.getLocation();
  Lex();

  // possible pre integer
  // R for data-edit-desc or
  // n for X in position-edit-desc
  ExprResult PreInt;
  if(FDParser.IsIntPresent())
    PreInt = FDParser.ParseIntExpr();

  // descriptor identifier.
  llvm::StringRef DescriptorStr;
  auto DescriptorStrLoc = FDParser.getCurrentLoc();
  if(!FDParser.LexIdentIfPresent(DescriptorStr)) {
    Diag.Report(DescriptorStrLoc, diag::err_format_expected_desc);
    return ExprError();
  }
  auto DescIdent = Identifiers.lookupFormatSpec(DescriptorStr);
  if(!DescIdent) {
    Diag.Report(DescriptorStrLoc, diag::err_format_invalid_desc)
      << DescriptorStr
      << SourceRange(DescriptorStrLoc,FDParser.getCurrentLoc());
    return ExprError();
  }

  // data-edit-desc or control-edit-desc
  auto Desc = DescIdent->getTokenID();
  ExprResult W, MD, E;
  switch(Desc) {
  // data-edit-desc
  case tok::fs_I: case tok::fs_B:
  case tok::fs_O: case tok::fs_Z:
    W = FDParser.ParseIntExpr(DescriptorStr.data());
    if(W.isInvalid()) break;
    if(FDParser.LexCharIfPresent('.')) {
      MD = FDParser.ParseIntExpr(".");
      if(MD.isInvalid()) break;
    }
    FDParser.MustBeDone();
    return Actions.ActOnFORMATDataEditDesc(Context, Loc, Desc, PreInt,
                                           W, MD, E);

  case tok::fs_F:
  case tok::fs_E: case tok::fs_EN: case tok::fs_ES: case tok::fs_G:
    W = FDParser.ParseIntExpr(DescriptorStr.data());
    if(!FDParser.LexCharIfPresent('.')) {
      if(Desc == tok::fs_G) {
        FDParser.MustBeDone();
        return Actions.ActOnFORMATDataEditDesc(Context, Loc, Desc, PreInt,
                                               W, MD, E);
      }
      Diag.Report(FDParser.getCurrentLoc(), diag::err_expected_dot);
      break;
    }
    MD = FDParser.ParseIntExpr(".");
    if(Desc != tok::fs_F &&
       (FDParser.LexCharIfPresent('E') ||
        FDParser.LexCharIfPresent('e'))) {
      E = FDParser.ParseIntExpr("E");
    }
    FDParser.MustBeDone();
    return Actions.ActOnFORMATDataEditDesc(Context, Loc, Desc, PreInt,
                                           W, MD, E);


  case tok::fs_A:
    if(!FDParser.IsDone())
      W = FDParser.ParseIntExpr();
    FDParser.MustBeDone();
    return Actions.ActOnFORMATDataEditDesc(Context, Loc, Desc, PreInt,
                                           W, MD, E);

  // position-edit-desc
  case tok::fs_T: case tok::fs_TL: case tok::fs_TR:
    W = FDParser.ParseIntExpr(DescriptorStr.data());
    if(W.isInvalid()) break;
    FDParser.MustBeDone();
    return Actions.ActOnFORMATPositionEditDesc(Context, Loc, Desc, W);

  case tok::fs_X:
    if(!PreInt.isUsable()) {
      Diag.Report(DescriptorStrLoc, diag::err_expected_int_literal_constant_before)
        << "X";
      break;
    }
    FDParser.MustBeDone();
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
    FDParser.MustBeDone();
    return Actions.ActOnFORMATControlEditDesc(Context, Loc, Desc);

  // FIXME: add the rest..
  default:
    Diag.Report(DescriptorStrLoc, diag::err_format_invalid_desc)
      << DescriptorStr;
    break;
  }
  return ExprError();
}

} // end namespace flang
