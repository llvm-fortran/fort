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

/// AssignAttrSpec - Helper function that assigns the attribute specification to
/// the list, but reports an error if that attribute was all ready assigned.
bool Parser::AssignAttrSpec(DeclSpec &DS, DeclSpec::AS Val) {
  if (DS.hasAttributeSpec(Val))
    return Diag.ReportError(Tok.getLocation(),
                            "attribute specification defined more than once");
  DS.setAttributeSpec(Val);
  Lex();
  return false;
}

/// AssignAccessSpec - Helper function that assigns the access specification to
/// the DeclSpec, but reports an error if that access spec was all ready
/// assigned.
bool Parser::AssignAccessSpec(DeclSpec &DS, DeclSpec::AC Val) {
  if (DS.hasAccessSpec(Val))
    return Diag.ReportError(Tok.getLocation(),
                            "access specification defined more than once");
  DS.setAccessSpec(Val);
  Lex();
  return false;
}

/// AssignIntentSpec - Helper function that assigns the intent specification to
/// the DeclSpec, but reports an error if that intent spec was all ready
/// assigned.
bool Parser::AssignIntentSpec(DeclSpec &DS, DeclSpec::IS Val) {
  if (DS.hasIntentSpec(Val))
    return Diag.ReportError(Tok.getLocation(),
                            "intent specification defined more than once");
  DS.setIntentSpec(Val);
  Lex();
  return false;
}

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

  llvm::SmallVector<ArraySpec*, 4> Dimensions;
  while (EatIfPresent(tok::comma)) {
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
    switch (Tok.getKind()) {
    default:
      Diag.ReportError(Tok.getLocation(),
                       "unknown attribute specification");
      goto error;
    case tok::kw_ALLOCATABLE:
      if (AssignAttrSpec(DS, DeclSpec::AS_allocatable))
        goto error;
      break;
    case tok::kw_ASYNCHRONOUS:
      if (AssignAttrSpec(DS, DeclSpec::AS_asynchronous))
        goto error;
      break;
    case tok::kw_CODIMENSION:
      if (AssignAttrSpec(DS, DeclSpec::AS_codimension))
        goto error;
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
      if (AssignAttrSpec(DS, DeclSpec::AS_contiguous))
        goto error;
      break;
    case tok::kw_DIMENSION:
      if (AssignAttrSpec(DS, DeclSpec::AS_dimension))
        goto error;
      if (ParseArraySpec(Dimensions))
        goto error;
      DS.setDimensions(Dimensions);
      break;
    case tok::kw_EXTERNAL:
      if (AssignAttrSpec(DS, DeclSpec::AS_external))
        goto error;
      break;
    case tok::kw_INTENT:
      Lex();
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
        if (AssignIntentSpec(DS, DeclSpec::IS_in))
          goto error;
        break;
      case tok::kw_OUT:
        if (AssignIntentSpec(DS, DeclSpec::IS_out))
          goto error;
        break;
      case tok::kw_INOUT:
        if (AssignIntentSpec(DS, DeclSpec::IS_inout))
          goto error;
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
      if (AssignAttrSpec(DS, DeclSpec::AS_intrinsic))
        goto error;
      break;
    case tok::kw_OPTIONAL:
      if (AssignAttrSpec(DS, DeclSpec::AS_optional))
        goto error;
      break;
    case tok::kw_PARAMETER:
      if (AssignAttrSpec(DS, DeclSpec::AS_parameter))
        goto error;
      break;
    case tok::kw_POINTER:
      if (AssignAttrSpec(DS, DeclSpec::AS_pointer))
        goto error;
      break;
    case tok::kw_PROTECTED:
      if (AssignAttrSpec(DS, DeclSpec::AS_protected))
        goto error;
      break;
    case tok::kw_SAVE:
      if (AssignAttrSpec(DS, DeclSpec::AS_save))
        goto error;
      break;
    case tok::kw_TARGET:
      if (AssignAttrSpec(DS, DeclSpec::AS_target))
        goto error;
      break;
    case tok::kw_VALUE:
      if (AssignAttrSpec(DS, DeclSpec::AS_value))
        goto error;
      break;
    case tok::kw_VOLATILE:
      if (AssignAttrSpec(DS, DeclSpec::AS_volatile))
        goto error;
      break;

    // Access Control Specifiers
    case tok::kw_PUBLIC:
      if (AssignAccessSpec(DS, DeclSpec::AC_public))
        goto error;
      break;
    case tok::kw_PRIVATE:
      if (AssignAccessSpec(DS, DeclSpec::AC_private))
        goto error;
      break;
    }
  }

  EatIfPresent(tok::coloncolon);

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
bool Parser::ParseDeclarationTypeSpec(DeclSpec &DS) {
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
    Lex();
    if (EatIfPresentInSameStmt(tok::star)) {
      // FIXME: proper obsolete COMPLEX*16 support
      Lex();
      DS.setDoublePrecision();
    }

    if (Tok.is(tok::l_paren)) {
      if(Tok.isAtStartOfStatement()) return false;
      const Token &NextTok = PeekAhead();
      if (NextTok.isNot(tok::kw_KIND) &&
          NextTok.isNot(tok::kw_LEN))
        return false;
    }

    if (EatIfPresent(tok::l_paren)) {
      Kind = ParseSelector(true);
      if (Kind.isInvalid())
        return true;

      if (Tok.isAtStartOfStatement() || !EatIfPresent(tok::r_paren)) {
        Diag.Report(Tok.getLocation(),diag::err_expected_rparen);
        return true;
      }
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
    Lex();

    if(EatIfPresentInSameStmt(tok::star)) {
      ParseCharacterStarLengthSpec(DS);
      EatIfPresentInSameStmt(tok::comma);
    } else {
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
  SourceLocation Loc, IDLoc;
  const IdentifierInfo *ID;

  Loc = Tok.getLocation();
  if (!EatIfPresent(tok::kw_TYPE)) {
    bool result = EatIfPresent(tok::kw_CLASS);
    assert(result);
    //FIXME: TODO CLASS
    goto error;
  }

  //FIXME: access-spec

  EatIfPresent(tok::coloncolon);
  IDLoc = Tok.getLocation();
  ID = Tok.getIdentifierInfo();
  if (Tok.isAtStartOfStatement() || !ID) {
    Diag.ReportError(Tok.getLocation(),"expected an identifier after 'TYPE'");
    goto error;
  }
  Lex();

  Actions.ActOnDerivedTypeDecl(Context, Loc, IDLoc, ID);

  //FIXME: private-sequence-stmt
  //FIXME: components.
  while(!EatIfPresent(tok::kw_ENDTYPE)){
    ParseDerivedTypeComponent();
  }

  ParseOptionalMatchingIdentifier(ID); // type-name
  Actions.ActOnEndDerivedTypeDecl();

  return false;
error:
  return true;
}

bool Parser::ParseDerivedTypeComponent() {
  DeclSpec DS;
  llvm::SmallVector<DeclResult, 4> Decls;

  ParseDeclarationTypeSpec(DS);
  //FIXME: attributes.
  bool HasColonColon = EatIfPresent(tok::coloncolon);

  return ParseDerivedTypeComponentDeclarationList(DS, Decls);
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
