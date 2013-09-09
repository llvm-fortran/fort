//===-- ParserDecl.cpp - Fortran Declaration Parser -----------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Fortran declaration parsing.
//
//===----------------------------------------------------------------------===//

#include "flang/Parse/Parser.h"
#include "flang/Parse/ParseDiagnostic.h"
#include "flang/AST/Decl.h"
#include "flang/AST/Expr.h"
#include "flang/Sema/Sema.h"

namespace flang {

bool Parser::ParseTypeDeclarationList(DeclSpec &DS,
                                      SmallVectorImpl<DeclResult> &Decls) {
  while (!Tok.isAtStartOfStatement()) {
    SourceLocation IDLoc = Tok.getLocation();
    const IdentifierInfo *ID = Tok.getIdentifierInfo();
    if (!ID) {
      Diag.Report(getExpectedLoc(), diag::err_expected_ident);
      return true;
    }

    Lex();

    DeclSpec MDS(DS);

    if(Tok.is(tok::l_paren)){
      llvm::SmallVector<ArraySpec*, 4> Dimensions;
      if(ParseArraySpec(Dimensions)) {
        return true;
      }
      //Array declaration.
      if(!MDS.hasAttributeSpec(DeclSpec::AS_dimension))
        MDS.setAttributeSpec(DeclSpec::AS_dimension);
      MDS.setDimensions(Dimensions);
    }
    else

    if(MDS.getTypeSpecType() == TST_character && EatIfPresentInSameStmt(tok::star)) {
      if (MDS.hasLengthSelector())
        Diag.Report(getExpectedLoc(), diag::err_duplicate_len_selector);
      ParseCharacterStarLengthSpec(MDS);
    }

    Decls.push_back(Actions.ActOnEntityDecl(Context, MDS, IDLoc, ID));

    if (!EatIfPresent(tok::comma)) {
      if (!Tok.isAtStartOfStatement()) {
        Diag.Report(getExpectedLoc(), diag::err_expected_comma)
          << FixItHint(getExpectedLocForFixIt(), ",");
        return true;
      }
      break;
    }

    if (Tok.isAtStartOfStatement()) {
      Diag.Report(getExpectedLoc(), diag::err_expected_ident_after)
        << ",";
      return true;
    }
  }

  return false;
}

/// ParseTypeDeclarationStmt - Parse a type-declaration-stmt construct.
///
///   [R501]:
///     type-declaration-stmt :=
///         declaration-type-spec [ [ , attr-spec ] ... :: ] entity-decl-list
bool Parser::ParseTypeDeclarationStmt(SmallVectorImpl<DeclResult> &Decls) {
  DeclSpec DS;
  if (ParseDeclarationTypeSpec(DS))
    return true;

  while (ConsumeIfPresent(tok::comma)) {
    // [R502]:
    //   attr-spec :=
    //       access-spec
    //    or ALLOCATABLE
    //    or ASYNCHRONOUS
    //    or CODIMENSION lbracket coarray-spec rbracket
    //    or CONTIGUOUS
    //    or DIMENSION ( array-spec )
    //    or EXTERNAL
    //    or INTENT ( intent-spec )
    //    or INTRINSIC
    //    or language-binding-spec // TODO!
    //    or OPTIONAL
    //    or PARAMETER
    //    or POINTER
    //    or PROTECTED
    //    or SAVE
    //    or TARGET
    //    or VALUE
    //    or VOLATILE
    auto Kind = Tok.getKind();
    auto Loc = Tok.getLocation();
    if(!ExpectAndConsume(tok::identifier)) {
      goto error;
    }
    switch (Kind) {
    default:
      Diag.ReportError(Loc,
                       "unknown attribute specification");
      goto error;
    case tok::kw_ALLOCATABLE:
      Actions.ActOnAttrSpec(Loc, DS, DeclSpec::AS_allocatable);
      break;
    case tok::kw_ASYNCHRONOUS:
      Actions.ActOnAttrSpec(Loc, DS, DeclSpec::AS_asynchronous);
      break;
    case tok::kw_CODIMENSION:
      Actions.ActOnAttrSpec(Loc, DS, DeclSpec::AS_codimension);
      if (!EatIfPresent(tok::l_square)) {
        Diag.ReportError(Tok.getLocation(),
                         "expected '[' in CODIMENSION attribute");
        goto error;
      }

      // FIXME: Parse the coarray-spec.

      if (!EatIfPresent(tok::r_square)) {
        Diag.ReportError(Tok.getLocation(),
                         "expected ']' in CODIMENSION attribute");
        goto error;
      }

      break;
    case tok::kw_CONTIGUOUS:
      Actions.ActOnAttrSpec(Loc, DS, DeclSpec::AS_contiguous);
      break;
    case tok::kw_DIMENSION:
      if (ParseDimensionAttributeSpec(Loc, DS))
        goto error;
      break;
    case tok::kw_EXTERNAL:
      Actions.ActOnAttrSpec(Loc, DS, DeclSpec::AS_external);
      break;
    case tok::kw_INTENT:
      // FIXME:
      if (!EatIfPresent(tok::l_paren)) {
        Diag.ReportError(Tok.getLocation(),
                         "expected '(' after 'INTENT' keyword");
        goto error;
      }


      switch (Tok.getKind()) {
      default:
        Diag.ReportError(Tok.getLocation(),
                         "invalid INTENT specifier");
        goto error;
      case tok::kw_IN:
        Actions.ActOnIntentSpec(Loc, DS, DeclSpec::IS_in);
        break;
      case tok::kw_OUT:
        Actions.ActOnIntentSpec(Loc, DS, DeclSpec::IS_out);
        break;
      case tok::kw_INOUT:
        Actions.ActOnIntentSpec(Loc, DS, DeclSpec::IS_inout);
        break;
      }
      Lex();

      if (!EatIfPresent(tok::r_paren)) {
        Diag.ReportError(Tok.getLocation(),
                         "expected ')' after INTENT specifier");
        goto error;
      }

      break;
    case tok::kw_INTRINSIC:
      Actions.ActOnAttrSpec(Loc, DS, DeclSpec::AS_intrinsic);
      break;
    case tok::kw_OPTIONAL:
      Actions.ActOnAttrSpec(Loc, DS, DeclSpec::AS_optional);
      break;
    case tok::kw_PARAMETER:
      Actions.ActOnAttrSpec(Loc, DS, DeclSpec::AS_parameter);
      break;
    case tok::kw_POINTER:
      Actions.ActOnAttrSpec(Loc, DS, DeclSpec::AS_pointer);
      break;
    case tok::kw_PROTECTED:
      Actions.ActOnAttrSpec(Loc, DS, DeclSpec::AS_protected);
      break;
    case tok::kw_SAVE:
      Actions.ActOnAttrSpec(Loc, DS, DeclSpec::AS_save);
      break;
    case tok::kw_TARGET:
      Actions.ActOnAttrSpec(Loc, DS, DeclSpec::AS_target);
      break;
    case tok::kw_VALUE:
      Actions.ActOnAttrSpec(Loc, DS, DeclSpec::AS_value);
      break;
    case tok::kw_VOLATILE:
      Actions.ActOnAttrSpec(Loc, DS, DeclSpec::AS_volatile);
      break;

    // Access Control Specifiers
    case tok::kw_PUBLIC:
      Actions.ActOnAccessSpec(Loc, DS, DeclSpec::AC_public);
      break;
    case tok::kw_PRIVATE:
      Actions.ActOnAccessSpec(Loc, DS, DeclSpec::AC_private);
      break;
    }
  }

  ConsumeIfPresent(tok::coloncolon);

  if (Tok.isAtStartOfStatement()) {
    // A type without any identifiers.
    Diag.ReportError(Tok.getLocation(),
                     "expected an identifier in TYPE list");
    goto error;
  }

  if (ParseTypeDeclarationList(DS, Decls))
    goto error;

  return false;
 error:
  return true;
}

bool Parser::ParseDimensionAttributeSpec(SourceLocation Loc, DeclSpec &DS) {
  SmallVector<ArraySpec*, 8> Dimensions;
  if (ParseArraySpec(Dimensions))
    return true;
  if (!Actions.ActOnAttrSpec(Loc, DS, DeclSpec::AS_dimension))
    DS.setDimensions(Dimensions);
  return false;
}

static bool isAttributeSpec(tok::TokenKind Kind) {
  switch (Kind) {
  default:
    return false;
  case tok::kw_ALLOCATABLE:
  case tok::kw_ASYNCHRONOUS:
  case tok::kw_CODIMENSION:
  case tok::kw_CONTIGUOUS:
  case tok::kw_DIMENSION:
  case tok::kw_EXTERNAL:
  case tok::kw_INTENT:
  case tok::kw_INTRINSIC:
  case tok::kw_OPTIONAL:
  case tok::kw_PARAMETER:
  case tok::kw_POINTER:
  case tok::kw_PROTECTED:
  case tok::kw_SAVE:
  case tok::kw_TARGET:
  case tok::kw_VALUE:
  case tok::kw_VOLATILE:
  case tok::kw_PUBLIC:
  case tok::kw_PRIVATE:
    break;
  }
  return true;
}

/// Parse the optional KIND or LEN selector.
/// 
///   [R405]:
///     kind-selector :=
///         ( [ KIND = ] scalar-int-initialization-expr )
///   [R425]:
///     length-selector :=
///         ( [ LEN = ] type-param-value )
ExprResult Parser::ParseSelector(bool IsKindSel) {
  if (EatIfPresent(IsKindSel ? tok::kw_KIND : tok::kw_LEN)) {
    if (!EatIfPresent(tok::equal)) {
      if (Tok.isNot(tok::l_paren))
        return Diag.ReportError(Tok.getLocation(),
                                IsKindSel ? 
                                "invalid kind selector" :
                                "invalid length selector");

      // TODO: We have a "REAL (KIND(10D0)) :: x" situation.
      return false;
    }
  }

  return ParseExpression();
}

bool Parser::ParseCharacterStarLengthSpec(DeclSpec &DS) {
  ExprResult Len;
  if(EatIfPresentInSameStmt(tok::l_paren)) {
    if(EatIfPresentInSameStmt(tok::star)) {
      DS.setStartLengthSelector();
    } else Len = ParseExpectedFollowupExpression("*");
    if(!EatIfPresentInSameStmt(tok::r_paren)) {
      Diag.Report(getExpectedLoc(), diag::err_expected_rparen)
        << FixItHint(getExpectedLocForFixIt(), ")");
      return true;
    }
  }
  else Len = ParseExpectedFollowupExpression("*");
  if(Len.isInvalid()) return true;
  if(Len.isUsable())  DS.setLengthSelector(Len.take());
  return false;
}

/// ParseDerivedTypeSpec - Parse the type declaration specifier.
///
///   [R455]:
///     derived-type-spec :=
///         type-name [ ( type-param-spec-list ) ]
///
///   [R456]:
///     type-param-spec :=
///         [ keyword = ] type-param-value
bool Parser::ParseDerivedTypeSpec(DeclSpec &DS) {
  //  SourceLocation Loc = Tok.getLocation();
  //  const IdentifierInfo *IDInfo = Tok.getIdentifierInfo();
  Lex();

  llvm::SmallVector<ExprResult, 4> ExprVec;
  if (Tok.is(tok::l_paren)) {
    // TODO: Parse "keyword =".
    do {
      Lex();
      ExprResult E = ParseExpression();
      if (E.isInvalid()) goto error;
      ExprVec.push_back(E);
    } while (Tok.is(tok::comma));

    if (!EatIfPresent(tok::r_paren)) {
      Diag.ReportError(Tok.getLocation(),
                       "expected ')' after type parameter specifier list");
      goto error;
    }
  }

  //  DS = new DerivedDeclSpec(new VarExpr(Loc, VD), ExprVec);
  return false;

 error:
  return true;
}

/// ParseTypeOrClassDeclTypeSpec - Parse a TYPE(...) or CLASS(...) declaration
/// type spec.
/// 
///   [R502]:
///     declaration-type-spec :=
///         TYPE ( derived-type-spec )
///      or CLASS ( derived-type-spec )
///      or CLASS ( * )
bool Parser::ParseTypeOrClassDeclTypeSpec(DeclSpec &DS) {
  if (EatIfPresent(tok::kw_TYPE)) {
    if (!EatIfPresent(tok::l_paren))
      return Diag.ReportError(Tok.getLocation(),
                              "expected '(' in type specification");

    if (ParseDerivedTypeSpec(DS))
      return true;

    if (!EatIfPresent(tok::r_paren))
      return Diag.ReportError(Tok.getLocation(),
                              "expected ')' in type specification");

    return Actions.ActOnTypeDeclSpec(&Context);
  }

  // TODO: Handle CLASS.

  return false;
}

/// ParseDeclarationTypeSpec - Parse a declaration type spec construct.
/// 
///   [R502]:
///     declaration-type-spec :=
///         intrinsic-type-spec
///      or TYPE ( derived-type-spec )
///      or CLASS ( derived-type-spec )
///      or CLASS ( * )
bool Parser::ParseDeclarationTypeSpec(DeclSpec &DS, bool AllowSelectors) {
  // [R403]:
  //   intrinsic-type-spec :=
  //       INTEGER [ kind-selector ]
  //    or REAL [ kind-selector ]
  //    or DOUBLE PRECISION
  //    or COMPLEX [ kind-selector ]
  //    or DOUBLE COMPLEX
  //    or CHARACTER [ char-selector ]
  //    or LOGICAL [ kind-selector ]
  switch (Tok.getKind()) {
  default:
    DS.SetTypeSpecType(DeclSpec::TST_unspecified);
    break;
  case tok::kw_INTEGER:
    DS.SetTypeSpecType(DeclSpec::TST_integer);
    break;
  case tok::kw_REAL:
    DS.SetTypeSpecType(DeclSpec::TST_real);
    break;
  case tok::kw_COMPLEX:
    DS.SetTypeSpecType(DeclSpec::TST_complex);
    break;
  case tok::kw_CHARACTER:
    DS.SetTypeSpecType(DeclSpec::TST_character);
    break;
  case tok::kw_LOGICAL:
    DS.SetTypeSpecType(DeclSpec::TST_logical);
    break;
  case tok::kw_DOUBLEPRECISION:
    DS.SetTypeSpecType(DeclSpec::TST_real);
    DS.setDoublePrecision(); // equivalent to Kind = 8
    break;
  case tok::kw_DOUBLECOMPLEX:
    DS.SetTypeSpecType(DeclSpec::TST_complex);
    DS.setDoublePrecision(); // equivalent to Kind = 8
    break;
  }

  if (DS.getTypeSpecType() == DeclSpec::TST_unspecified)
    if (ParseTypeOrClassDeclTypeSpec(DS))
      return true;

  ExprResult Kind;
  ExprResult Len;

  // FIXME: no Kind for double complex and double precision
  switch (DS.getTypeSpecType()) {
  default:
    ConsumeToken();
    if (ConsumeIfPresent(tok::star)) {
      // FIXME: proper obsolete COMPLEX*16 support
      ConsumeAnyToken();
      DS.setDoublePrecision();
    }

    if (!AllowSelectors)
      break;
    if (ConsumeIfPresent(tok::l_paren)) {
      Kind = ParseSelector(true);
      if (Kind.isInvalid())
        return true;

      if(!ExpectAndConsume(tok::r_paren, 0, "", tok::r_paren))
        return true;
    }

    break;
  case DeclSpec::TST_character:
    // [R424]:
    //   char-selector :=
    //       length-selector
    //    or ( LEN = type-param-value , KIND = scalar-int-initialization-expr )
    //    or ( type-param-value , #
    //    #    [ KIND = ] scalar-int-initialization-expr )
    //    or ( KIND = scalar-int-initialization-expr [, LEN = type-param-value])
    //
    // [R425]:
    //   length-selector :=
    //       ( [ LEN = ] type-param-value )
    //    or * char-length [,]
    //
    // [R426]:
    //   char-length :=
    //       ( type-param-value )
    //    or scalar-int-literal-constant
    //
    // [R402]:
    //   type-param-value :=
    //       scalar-int-expr
    //    or *
    //    or :
    ConsumeToken();

    if(EatIfPresentInSameStmt(tok::star)) {
      ParseCharacterStarLengthSpec(DS);
      EatIfPresentInSameStmt(tok::comma);
    } else {
      if (!AllowSelectors)
        break;

      if (Tok.is(tok::l_paren)) {
        Lex(); // Eat '('.

        if (Tok.is(tok::kw_LEN)) {
          Len = ParseSelector(false);
          if (Len.isInvalid())
            return true;
        } else if (Tok.is(tok::kw_KIND)) {
          Kind = ParseSelector(true);
          if (Kind.isInvalid())
            return true;
        } else {
          ExprResult KindExpr = ParseExpression();
          Len = KindExpr;
        }

        if (Tok.is(tok::comma)) {
          Lex(); // Eat ','.

          if (Tok.is(tok::kw_LEN)) {
            if (Len.isInvalid())
              return Diag.ReportError(Tok.getLocation(),
                                      "multiple LEN selectors for this type");
            Len = ParseSelector(false);
            if (Len.isInvalid())
              return true;
          } else if (Tok.is(tok::kw_KIND)) {
            if (Kind.isInvalid())
              return Diag.ReportError(Tok.getLocation(),
                                      "multiple KIND selectors for this type");
            Kind = ParseSelector(true);
            if (Kind.isInvalid())
              return true;
          } else {
            if (Kind.isInvalid())
              return Diag.ReportError(Tok.getLocation(),
                                      "multiple KIND selectors for this type");

            ExprResult KindExpr = ParseExpression();
            Kind = KindExpr;
          }
        }

        if (!EatIfPresent(tok::r_paren))
          return Diag.ReportError(Tok.getLocation(),
                                  "expected ')' after selector");
      }
    }

    break;
  }

  // Set the selectors for declspec.
  if(Kind.isUsable()) DS.setKindSelector(Kind.get());
  if(Len.isUsable())  DS.setLengthSelector(Len.get());
  return false;
}

/// ParseOptionalMatchingIdentifier -
/// Parses the optional definition name after END/END something keyword
/// e.g. END TYPE Foo
bool Parser::ParseOptionalMatchingIdentifier(const IdentifierInfo *OriginalId) {
  if (Tok.is(tok::identifier) && !Tok.isAtStartOfStatement()) {
    const IdentifierInfo * IDInfo = Tok.getIdentifierInfo();
    SourceLocation NameLoc = Tok.getLocation();
    Lex(); // Eat the ending token.

    //FIXME: is this case insensitive???
    //Apply equality constraint
    if(IDInfo->getName() != OriginalId->getName()) {
      llvm::Twine Msg = llvm::Twine("The name '") +
          IDInfo->getName() + "' doesn't match previously used name '" +
          OriginalId->getName();
      Diag.ReportError(NameLoc,Msg);
      return true;
    }
  }
  return false;
}

// FIXME: Fortran 2008 stuff.
/// ParseDerivedTypeDefinitionStmt - Parse a type or a class definition.
///
/// [R422]:
///   derived-type-def :=
///     derived-type-stmt
///     [ private-sequence-stmt ] ...
///     component-def-stmt
///     [ component-def-stmt ] ...
///     end-type-stmt
///
/// [R423]:
///   derived-type-stmt :=
///     TYPE [ [ , access-spec ] :: ] type-name
///
/// [R424]:
///   private-sequence-stmt :=
///     PRIVATE or SEQUENCE
///
/// [R425]:
///   component-def-stmt :=
///     type-spec [ [ component-attr-spec-list ] :: ] component-decl-list
///
/// [R426]:
///   component-attr-spec-list :=
///     POINTER or
///     DIMENSION( component-array-spec )
///
/// [R427]:
///   component-array-spec :=
///     explicit-shape-spec-list or
///     deffered-shape-spec-list
///
/// [R428]:
///   component-decl :=
///     component-name [ ( component-array-spec ) ]
///     [ * char-length ] [ component-initialization ]
///
/// [R429]:
///   component-initialization :=
///     = initialization-expr or
///     => NULL()
///
/// [R430]:
///   end-type-stmt :=
///     END TYPE [ type-name ]
bool Parser::ParseDerivedTypeDefinitionStmt() {
  bool IsClass = Tok.is(tok::kw_CLASS);
  auto Loc = ConsumeToken();

  if(ConsumeIfPresent(tok::comma)) {
    //FIXME: access-spec
  }
  ConsumeIfPresent(tok::coloncolon);

  auto ID = Tok.getIdentifierInfo();
  auto IDLoc = Tok.getLocation();
  if(!ExpectAndConsume(tok::identifier)) {
    SkipUntilNextStatement();
    return true;
  } else
    ExpectStatementEnd();

  Actions.ActOnDerivedTypeDecl(Context, Loc, IDLoc, ID);
  // FIXME: private, sequence

  bool Done = false;
  while(!Done) {
    switch(Tok.getKind()) {
    case tok::kw_TYPE:
    case tok::kw_CLASS:
    case tok::kw_INTEGER:
    case tok::kw_REAL:
    case tok::kw_COMPLEX:
    case tok::kw_CHARACTER:
    case tok::kw_LOGICAL:
    case tok::kw_DOUBLEPRECISION:
    case tok::kw_DOUBLECOMPLEX:
      if(ParseDerivedTypeComponentStmt())
        SkipUntilNextStatement();
      else ExpectStatementEnd();
      break;
    default:
      Done = true;
      break;
    }
  }

  ParseEndTypeStmt();

  Actions.ActOnEndDerivedTypeDecl(Context);
  return false;
error:
  return true;
}

void Parser::ParseEndTypeStmt() {
  if(Tok.isNot(tok::kw_ENDTYPE)) {
    Diag.Report(Tok.getLocation(), diag::err_expected_kw)
      << "end type";
    Diag.Report(cast<NamedDecl>(Actions.CurContext)->getLocation(), diag::note_matching)
      << "type";
    return;
  }

  auto Loc = ConsumeToken();
  if(IsPresent(tok::identifier)) {
    auto ID = Tok.getIdentifierInfo();
    Actions.ActOnENDTYPE(Context, Loc, ConsumeToken(), ID);
  } else
    Actions.ActOnENDTYPE(Context, Loc, Loc, nullptr);
  ExpectStatementEnd();
}

bool Parser::ParseDerivedTypeComponentStmt() {
  // type-spec
  DeclSpec DS;
  llvm::SmallVector<DeclResult, 4> Decls;
  if(ParseDeclarationTypeSpec(DS))
    return true;

  // component-attr-spec
  if(ConsumeIfPresent(tok::comma)) {
    do {
      auto Kind = Tok.getKind();
      auto Loc = Tok.getLocation();
      auto ID = Tok.getIdentifierInfo();
      if(!ExpectAndConsume(tok::identifier))
        return true;
      if(Kind == tok::kw_POINTER)
        Actions.ActOnAttrSpec(Loc, DS, DeclSpec::AS_pointer);
      else if(Kind == tok::kw_DIMENSION) {
        if(ParseDimensionAttributeSpec(Loc, DS))
          return true;
      } else {
        if(isAttributeSpec(Kind))
          Diag.Report(Loc, diag::err_use_of_attr_spec_in_type_decl)
            << ID;
        else
          Diag.Report(Loc, diag::err_expected_attr_spec);
        if(!SkipUntil(tok::coloncolon, true, true))
          return true;
        break;
      }
    } while(ConsumeIfPresent(tok::comma));
    if(!ExpectAndConsume(tok::coloncolon))
      return true;
  } else
    ConsumeIfPresent(tok::coloncolon);

  ParseDerivedTypeComponentDeclarationList(DS, Decls);

  return false;
}

bool Parser::ParseDerivedTypeComponentDeclarationList(DeclSpec &DS,
                              SmallVectorImpl<DeclResult> &Decls) {
  while (!Tok.isAtStartOfStatement()) {
    SourceLocation IDLoc = Tok.getLocation();
    const IdentifierInfo *ID = Tok.getIdentifierInfo();
    if (!ID)
      return Diag.ReportError(IDLoc,
                              "expected an identifier in TYPE list");
    Lex();

    // FIXME: If there's a '(' here, it might be parsing an array decl.

    // FIXME: init expression

    Decls.push_back(Actions.ActOnDerivedTypeFieldDecl(Context, DS, IDLoc, ID));

    SourceLocation CommaLoc = Tok.getLocation();
    if (!EatIfPresent(tok::comma)) {
      if (!Tok.is(tok::kw_ENDTYPE) && !Tok.isAtStartOfStatement())
        return Diag.ReportError(Tok.getLocation(),
                                "expected a ',' in Component declaration list");

      break;
    }

    if (Tok.isAtStartOfStatement())
      return Diag.ReportError(CommaLoc,
                              "expected an identifier after ',' in Component declaration list");
  }

  return false;
}

} //namespace flang
