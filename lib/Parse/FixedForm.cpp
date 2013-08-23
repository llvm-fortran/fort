#include "flang/Parse/FixedForm.h"
#include "flang/Parse/Parser.h"

namespace flang {
namespace fixedForm {

KeywordFilter::KeywordFilter(tok::TokenKind K1, tok::TokenKind K2,
                             tok::TokenKind K3) {
  SmallArray[0] = K1; SmallArray[1] = K2;
  SmallArray[2] = K3;
  Keywords = ArrayRef<tok::TokenKind>(SmallArray, K3 == tok::unknown? 2 : 3);
}

KeywordMatcher::KeywordMatcher(ArrayRef<KeywordFilter> Filters) {
  for(auto Filter : Filters) {
    for(auto Keyword : Filter.getKeywords())
      Register(Keyword);
  }
}

void KeywordMatcher::operator=(ArrayRef<KeywordFilter> Filters) {
  for(auto Filter : Filters) {
    for(auto Keyword : Filter.getKeywords())
      Register(Keyword);
  }
}

void KeywordMatcher::Register(tok::TokenKind Keyword) {
  auto Identifier = getTokenName(Keyword);
  std::string Name(Identifier);
  for (size_t I = 0, E = Name.size(); I != E; ++I)
    Name[I] = ::tolower(Name[I]);
  Keywords.insert(Name);
}

bool KeywordMatcher::Matches(StringRef Identifier) const {
  std::string Name(Identifier);
  for (size_t I = 0, E = Name.size(); I != E; ++I)
    Name[I] = ::tolower(Name[I]);
  return Keywords.find(Name) != Keywords.end();
}

static const tok::TokenKind AmbiguousExecKeywords[] = {
  // ASSIGN10TOI
  tok::kw_ASSIGN,
  // DOI=1,10
  tok::kw_DO,
  // ENDDOconstructname
  tok::kw_ENDDO,
  // ELSEconstructname
  tok::kw_ELSE,
  // ENDIFconstructname
  tok::kw_ENDIF,
  // ENDSELECTconstructname
  tok::kw_ENDSELECT,
  // GOTOI
  tok::kw_GOTO,
  // CALLfoo
  tok::kw_CALL,
  // STOP1
  tok::kw_STOP,
  // ENTRYwhat
  tok::kw_ENTRY,
  // RETURN1
  tok::kw_RETURN,
  // CYCLE/EXITconstructname
  tok::kw_CYCLE,
  tok::kw_EXIT,
  // PRINTfmt
  tok::kw_PRINT,
  // READfmt
  tok::kw_READ,

  tok::kw_DATA
};

AmbiguousExecutableStatements::AmbiguousExecutableStatements()
  : KeywordFilter(AmbiguousExecKeywords) {}

static const tok::TokenKind AmbiguousEndKeywords[] = {
  // END<...>name
  tok::kw_END,
  tok::kw_ENDPROGRAM,
  tok::kw_ENDSUBROUTINE,
  tok::kw_ENDFUNCTION
};

AmbiguousEndStatements::AmbiguousEndStatements()
  : KeywordFilter(AmbiguousEndKeywords) {}

static const tok::TokenKind AmbiguousTypeKeywords[] = {
  // INTEGERvar
  tok::kw_INTEGER,
  tok::kw_REAL,
  tok::kw_COMPLEX,
  tok::kw_DOUBLEPRECISION,
  tok::kw_DOUBLECOMPLEX,
  tok::kw_LOGICAL,
  tok::kw_CHARACTER
};

AmbiguousTypeStatements::AmbiguousTypeStatements()
  : KeywordFilter(AmbiguousTypeKeywords) {}

static const tok::TokenKind AmbiguousSpecKeywords[] = {
  // IMPLICITREAL
  tok::kw_IMPLICIT,
  // DIMENSIONI(10)
  tok::kw_DIMENSION,
  // EXTERNALfoo
  tok::kw_EXTERNAL,
  // INTRINSICfoo
  tok::kw_INTRINSIC,
  // COMMONi
  tok::kw_COMMON,
  // DATAa/1/
  tok::kw_DATA,
  // SAVEi
  tok::kw_SAVE
};

AmbiguousSpecificationStatements::AmbiguousSpecificationStatements()
  : KeywordFilter(AmbiguousSpecKeywords) {}

static const tok::TokenKind AmbiguousTopLvlDeclKeywords[] = {
  // PROGRAMname
  tok::kw_PROGRAM,
  tok::kw_SUBROUTINE,
  tok::kw_FUNCTION,
  // RECURSIVEfunctionFOO
  tok::kw_RECURSIVE
};

AmbiguousTopLevelDeclarationStatements::AmbiguousTopLevelDeclarationStatements()
  : KeywordFilter(AmbiguousTopLvlDeclKeywords){}

CommonAmbiguities::CommonAmbiguities() {
  {
    const KeywordFilter Filters[] = {
      AmbiguousTypeStatements(),
      KeywordFilter(tok::kw_FUNCTION, tok::kw_SUBROUTINE)
    };
    MatcherForKeywordsAfterRECURSIVE = llvm::makeArrayRef(Filters);
  }
}

} // end namespace fixedForm

void Parser::RelexAmbiguousIdentifier(const fixedForm::KeywordMatcher &Matcher) {
  assert(Features.FixedForm);
  assert(!Tok.isAtStartOfStatement());
  if(Matcher.Matches(Tok.getIdentifierInfo()->getName()))
    return;
  TheLexer.LexFixedFormIdentifierMatchLongestKeyword(Matcher, Tok);
  ClassifyToken(Tok);
}

Parser::MatchFixedFormIdentAction
Parser::MatchFixedFormIdentifier(Token &T, IdentifierLexingContext Context) {
  // Set the identifier info for this token.
  llvm::SmallVector<llvm::StringRef, 2> Spelling;
  TheLexer.getSpelling(T, Spelling);
  std::string NameStr = T.CleanLiteral(Spelling);

  if(Context.Kind == IdentifierLexingContext::StatementStart) {
    auto KW = Identifiers.lookupKeyword(NameStr);
    if(KW) {
      auto ID = KW->getTokenID();

      /// We need to look for these keywords in the start
      /// of a statement because they can be merged
      /// with other identifiers.
      if(StatementContext->getStatementOrder() != StatementParsingContext::ExecutableConstructs) {
        switch(ID) {
        // INTEGERvar
        case tok::kw_INTEGER:
        case tok::kw_REAL:
        case tok::kw_COMPLEX:
        case tok::kw_DOUBLEPRECISION:
        case tok::kw_DOUBLECOMPLEX:
        case tok::kw_LOGICAL:
        case tok::kw_CHARACTER:
        // IMPLICITREAL
        case tok::kw_IMPLICIT:
        // DIMENSIONI(10)
        case tok::kw_DIMENSION:
        // EXTERNALfoo
        case tok::kw_EXTERNAL:
        // INTRINSICfoo
        case tok::kw_INTRINSIC:
        // COMMONi
        case tok::kw_COMMON:
        // DATAa/1/
        case tok::kw_DATA:
        // SAVEi
        case tok::kw_SAVE:
        // PROGRAMname
        case tok::kw_PROGRAM:
        // ENDPROGRAMname
        case tok::kw_ENDPROGRAM:
        // SUBROUTINEfoo
        case tok::kw_SUBROUTINE:
        case tok::kw_ENDSUBROUTINE:
        // FUNCTIONfoo
        case tok::kw_FUNCTION:
        case tok::kw_ENDFUNCTION:
        case tok::kw_RECURSIVE:
          return RememberIdentAction;
          break;
        default:
          break;
        }
      }

      switch(ID) {
      // ASSIGN10TOI
      case tok::kw_ASSIGN:
      // DOI=1,10
      case tok::kw_DO:
      // ENDDOconstructname
      case tok::kw_ENDDO:
      // ELSEconstructname
      case tok::kw_ELSE:
      // ENDIFconstructname
      case tok::kw_ENDIF:
      // ENDSELECTconstructname
      case tok::kw_ENDSELECT:
      // GOTOI
      case tok::kw_GOTO:
      // CALLfoo
      case tok::kw_CALL:
      // STOP1
      case tok::kw_STOP:
      // ENTRYwhat
      case tok::kw_ENTRY:
      // RETURN1
      case tok::kw_RETURN:
      // CYCLE/EXITconstructname
      case tok::kw_CYCLE:
      case tok::kw_EXIT:
      // PRINTfmt
      case tok::kw_PRINT:
      // READfmt
      case tok::kw_READ:

      case tok::kw_END:
      case tok::kw_ENDPROGRAM:
      case tok::kw_ENDSUBROUTINE:
      case tok::kw_ENDFUNCTION:
        return RememberIdentAction;
        break;
      default:
        break;
      }
      return ResetIdentAction;
    }
  }
  else if(Context.Kind == IdentifierLexingContext::MergedKeyword) {
    auto KW = Identifiers.lookupKeyword(NameStr);
    if(KW && KW->getTokenID() == Context.Keyword)
      return RememberIdentAction;
  }
  return NoIdentAction;
}

Parser::StmtResult Parser::ReparseAmbiguousStatement() {
  StatementTokens.push_back(Tok);
  TheLexer.ReparseStatement(StatementTokens);
  Tok.startToken();
  Lex();
  return ParseAmbiguousAssignmentStmt();
}

StmtResult Parser::ReparseAmbiguousStatementSwitchToExecutablePart() {
  StatementParsingContext StmtContext(*this, StatementParsingContext::ExecutableConstructs);
  return ReparseAmbiguousStatement();
}

} // end namespace flang
