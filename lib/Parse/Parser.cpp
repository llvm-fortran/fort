//===-- Parser.cpp - Fortran Parser Interface -----------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// The Fortran parser interface.
//
//===----------------------------------------------------------------------===//

#include "flang/Parse/Parser.h"
#include "flang/Parse/LexDiagnostic.h"
#include "flang/Parse/ParseDiagnostic.h"
#include "flang/Sema/SemaDiagnostic.h"
#include "flang/AST/Decl.h"
#include "flang/AST/Expr.h"
#include "flang/AST/Stmt.h"
#include "flang/AST/StmtDumper.h"
#include "flang/Basic/TokenKinds.h"
#include "flang/Sema/DeclSpec.h"
#include "flang/Sema/Sema.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallString.h"

namespace flang {

/// print - If a crash happens while the parser is active, print out a line
/// indicating what the current token is.
void PrettyStackTraceParserEntry::print(llvm::raw_ostream &OS) const {
  const Token &Tok = FP.getCurToken();
  if (Tok.is(tok::eof)) {
    OS << "<eof> parser at end of file\n";
    return;
  }

  if (!Tok.getLocation().isValid()) {
    OS << "<unknown> parser at unknown location\n";
    return;
  }

  llvm::SmallVector<llvm::StringRef, 2> Spelling;
  FP.getLexer().getSpelling(Tok, Spelling);
  std::string Name = Tok.CleanLiteral(Spelling);
  FP.getLexer().getSourceManager()
    .PrintMessage(Tok.getLocation(), llvm::SourceMgr::DK_Error,
                  "current parser token '" + Name + "'");
}

//===----------------------------------------------------------------------===//
//                            Fortran Parsing
//===----------------------------------------------------------------------===//

Parser::Parser(llvm::SourceMgr &SM, const LangOptions &Opts, DiagnosticsEngine  &D,
               Sema &actions)
  : TheLexer(SM, Opts, D), Features(Opts), CrashInfo(*this), SrcMgr(SM),
    CurBuffer(0), Context(actions.Context), Diag(D), Actions(actions),
    Identifiers(Opts), DontResolveIdentifiers(false),
    DontResolveIdentifiersInSubExpressions(false),
    LexFORMATTokens(false) {
  getLexer().setBuffer(SrcMgr.getMemoryBuffer(CurBuffer));
  Tok.startToken();
  NextTok.startToken();
}

bool Parser::EnterIncludeFile(const std::string &Filename) {
  std::string IncludedFile;
  int NewBuf = SrcMgr.AddIncludeFile(Filename, getLexer().getLoc(),
                                     IncludedFile);
  if (NewBuf == -1)
    return true;

  CurBuffer = NewBuf;
  LexerBufferContext.push_back(getLexer().getBufferPtr());
  getLexer().setBuffer(SrcMgr.getMemoryBuffer(CurBuffer));
  Diag.getClient()->BeginSourceFile(Features, &TheLexer);
  return false;
}

bool Parser::LeaveIncludeFile() {
  if(CurBuffer == 0) return true;//No files included.
  Diag.getClient()->EndSourceFile();
  --CurBuffer;
  getLexer().setBuffer(SrcMgr.getMemoryBuffer(CurBuffer),
                       LexerBufferContext.back());
  LexerBufferContext.pop_back();
  return false;
}

SourceLocation Parser::getExpectedLoc() const {
  if(Tok.isAtStartOfStatement())
    return PrevTokLocEnd;
  return Tok.getLocation();
}

/// Lex - Get the next token.
void Parser::Lex() {
  PrevTokLocEnd = getMaxLocationOfCurrentToken();
  if(LexFORMATTokens) {
    if (NextTok.isNot(tok::unknown))
      Tok = NextTok;
    else
      TheLexer.LexFORMATToken(Tok);
    NextTok.setKind(tok::unknown);

    if (!Tok.is(tok::eof))
      return;
    else
      LexFORMATTokens = false;
  }

  if (NextTok.isNot(tok::unknown)) {
    Tok = NextTok;
  } else {
    TheLexer.Lex(Tok);
    ClassifyToken(Tok);
  }

  if (Tok.is(tok::eof)){
    if(!LeaveIncludeFile()){
      NextTok.setKind(tok::unknown);
      Lex();
    }
    return;
  }

  TheLexer.Lex(NextTok);
  ClassifyToken(NextTok);

#define MERGE_TOKENS(A, B)                      \
  if (!NextTok.isAtStartOfStatement() && NextTok.is(tok::kw_ ## B)) {              \
    Tok.setKind(tok::kw_ ## A ## B);            \
    break;                                      \
  }                                             \

  // [3.3.1]p4
  switch (Tok.getKind()) {
  default: return;
  case tok::kw_INCLUDE:{
    bool hadErrors = ParseInclude();
    Tok = NextTok;
    NextTok.setKind(tok::unknown);
    if(hadErrors)
      LexToEndOfStatement();
    else Lex();
    return;
  }
  case tok::kw_BLOCK:
    MERGE_TOKENS(BLOCK, DATA);
    return;
  case tok::kw_ELSE:
    MERGE_TOKENS(ELSE, IF);
    MERGE_TOKENS(ELSE, WHERE);
    return;
  case tok::kw_END: {
    MERGE_TOKENS(END, IF);
    MERGE_TOKENS(END, DO);
    MERGE_TOKENS(END, FUNCTION);
    MERGE_TOKENS(END, SUBROUTINE);
    MERGE_TOKENS(END, FORALL);
    MERGE_TOKENS(END, WHERE);
    MERGE_TOKENS(END, ENUM);
    MERGE_TOKENS(END, SELECT);
    MERGE_TOKENS(END, TYPE);
    MERGE_TOKENS(END, MODULE);
    MERGE_TOKENS(END, PROGRAM);
    MERGE_TOKENS(END, ASSOCIATE);
    MERGE_TOKENS(END, FILE);
    MERGE_TOKENS(END, INTERFACE);
    MERGE_TOKENS(END, BLOCKDATA);

    if (NextTok.is(tok::kw_BLOCK)) {
      Tok = NextTok;
      TheLexer.Lex(NextTok);
      ClassifyToken(NextTok);

      if (!NextTok.is(tok::kw_DATA)) {
        Diag.ReportError(NextTok.getLocation(),
                         "expected 'DATA' after 'BLOCK' keyword");
        return;
      }

      Tok.setKind(tok::kw_ENDBLOCKDATA);
      break;
    }

    return;
  }
  case tok::kw_ENDBLOCK:
    MERGE_TOKENS(ENDBLOCK, DATA);
    return;
  case tok::kw_GO:
    MERGE_TOKENS(GO, TO);
    return;
  case tok::kw_SELECT:
    MERGE_TOKENS(SELECT, CASE);
    MERGE_TOKENS(SELECT, TYPE);
    return;
  case tok::kw_IN:
    MERGE_TOKENS(IN, OUT);
    return;
  case tok::kw_DOUBLE:
    MERGE_TOKENS(DOUBLE, PRECISION);
    MERGE_TOKENS(DOUBLE, COMPLEX);
    return;
  }

  if (NextTok.is(tok::eof)) return;

  TheLexer.Lex(NextTok);
  ClassifyToken(NextTok);
}

void Parser::ClassifyToken(Token &T) {
  if (T.isNot(tok::identifier))
    return;

  // Set the identifier info for this token.
  llvm::SmallVector<llvm::StringRef, 2> Spelling;
  TheLexer.getSpelling(T, Spelling);
  std::string NameStr = Tok.CleanLiteral(Spelling);

  // We assume that the "common case" is that if an identifier is also a
  // keyword, it will most likely be used as a keyword. I.e., most programs are
  // sane, and won't use keywords for variable names. We mark it as a keyword
  // for ease in parsing. But it's weak and can change into an identifier or
  // builtin depending upon the context.
  if (IdentifierInfo *KW = Identifiers.lookupKeyword(NameStr)) {
    T.setIdentifierInfo(KW);
    T.setKind(KW->getTokenID());
  } else {
    IdentifierInfo *II = getIdentifierInfo(NameStr);
    T.setIdentifierInfo(II);
    T.setKind(II->getTokenID());
  }
}

/// CleanLiteral - Cleans up a literal if it needs cleaning. It removes the
/// continuation contexts and comments. Cleaning a dirty literal is SLOW!
void Parser::CleanLiteral(Token T, std::string &NameStr) {
  assert(T.isLiteral() && "Trying to clean a non-literal!");
  if (!T.needsCleaning()) {
    // This should be the common case.
    NameStr = llvm::StringRef(T.getLiteralData(),
                              T.getLength()).str();
    return;
  }

  llvm::SmallVector<llvm::StringRef, 2> Spelling;
  TheLexer.getSpelling(T, Spelling);
  NameStr = T.CleanLiteral(Spelling);
}

/// EatIfPresent - Eat the token if it's present. Return 'true' if it was
/// delicious.
bool Parser::EatIfPresent(tok::TokenKind Kind) {
  if (Tok.is(Kind)) {
    Lex();
    return true;
  }

  return false;
}

/// EatIfPresentInSameStmt - Eat the token if it's present and isn't a part of a new statement.
/// Return 'true' if it was delicious.
bool Parser::EatIfPresentInSameStmt(tok::TokenKind Kind) {
  if (!Tok.isAtStartOfStatement() && Tok.is(Kind)) {
    Lex();
    return true;
  }

  return false;
}

/// Expect - Eat the token if it's present. Return 'true' if it was
/// delicious. Reports error if it wasn't.
bool Parser::Expect(tok::TokenKind Kind,const llvm::Twine &Msg){
  if (!EatIfPresent(Kind)) {
    Diag.ReportError(Tok.getLocation(),Msg);
    return false;
  }
  return true;
}

/// LexToEndOfStatement - Lex to the end of a statement. Done in an
/// unrecoverable error situation.
void Parser::LexToEndOfStatement() {
  // Eat the rest of the statment.
  while (!Tok.isAtStartOfStatement())
    Lex();
}

/// ParseInclude - parses the include statement and loads the included file.
bool Parser::ParseInclude() {
  const Token &NextTok = PeekAhead();
  if(!NextTok.is(tok::char_literal_constant)){
    Diag.Report(NextTok.getLocation(),diag::err_pp_expects_filename);
    return true;
  }
  std::string LiteralString;
  CleanLiteral(NextTok,LiteralString);
  //FIXME: need a proper way to get unquoted unescaped string from the character literal.
  LiteralString = std::string(LiteralString,1,LiteralString.length()-2);
  if(!LiteralString.length()) {
    Diag.Report(NextTok.getLocation(),diag::err_pp_empty_filename);
    return true;
  }
  if(EnterIncludeFile(LiteralString) == true){
    Diag.Report(NextTok.getLocation(),diag::err_pp_file_not_found) <<
                LiteralString;
    return true;
  }
  return false;
}

/// ParseStatementLabel - Parse the statement label token. If the current token
/// isn't a statement label, then set the StmtLabelTok's kind to "unknown".
void Parser::ParseStatementLabel() {
  if (Tok.isNot(tok::statement_label)) {
    if (Tok.isAtStartOfStatement())
      StmtLabel = 0;
    return;
  }

  std::string NumStr;
  CleanLiteral(Tok, NumStr);
  StmtLabel = IntegerConstantExpr::Create(Context, Tok.getLocation(),
                                          getMaxLocationOfCurrentToken(),
                                          NumStr);
  Lex();
}

/// ParseStatementLabelReference - Parses a statement label reference token.
ExprResult Parser::ParseStatementLabelReference() {
  if(Tok.isNot(tok::int_literal_constant)) {
    return ExprError();
  }

  std::string NumStr;
  CleanLiteral(Tok, NumStr);
  auto Result = IntegerConstantExpr::Create(Context, Tok.getLocation(),
                                            getMaxLocationOfCurrentToken(),
                                            NumStr);
  Lex();
  return Result;
}

// Assumed syntax rules
//
//   [R101] xyz-list        :=  xyz [, xyz] ...
//   [R102] xyz-name        :=  name
//   [R103] scalar-xyz      :=  xyz
//
//   [C101] (R103) scalar-xyz shall be scalar.

/// ParseProgramUnits - Main entry point to the parser. Parses the current
/// source.
bool Parser::ParseProgramUnits() {
  Actions.ActOnTranslationUnit();

  // Prime the lexer.
  Lex();
  Tok.setFlag(Token::StartOfStatement);

  while (!ParseProgramUnit())
    /* Parse them all */;

  return false;
}

/// ParseProgramUnit - Parse a program unit.
///
///   [R202]:
///     program-unit :=
///         main-program
///      or external-subprogram
///      or module
///      or block-data
bool Parser::ParseProgramUnit() {
  if (Tok.is(tok::eof))
    return true;

  std::vector<StmtResult> Body;

  ParseStatementLabel();
  if (PeekAhead().is(tok::equal))
    return ParseMainProgram(Body);

  // FIXME: These calls should return something proper.
  switch (Tok.getKind()) {
  default:
    ParseMainProgram(Body);
    break;

  case tok::kw_REAL:
  case tok::kw_INTEGER:
  case tok::kw_COMPLEX:
  case tok::kw_CHARACTER:
  case tok::kw_LOGICAL:
  case tok::kw_DOUBLEPRECISION:
  case tok::kw_DOUBLECOMPLEX:
    ParseTypedExternalSubprogram(Body);
    break;
  case tok::kw_FUNCTION:
  case tok::kw_SUBROUTINE:
    ParseExternalSubprogram(Body);
    break;

  case tok::kw_MODULE:
    ParseModule();
    break;

  case tok::kw_BLOCKDATA:
    ParseBlockData();
    break;
  }

  return false;
}

/// ParseMainProgram - Parse the main program.
///
///   [R1101]:
///     main-program :=
///         [program-stmt]
///           [specification-part]
///           [execution-part]
///           [internal-subprogram-part]
///           end-program-stmt
bool Parser::ParseMainProgram(std::vector<StmtResult> &Body) {
  // If the PROGRAM statement didn't have an identifier, pretend like it did for
  // the time being.
  StmtResult ProgStmt;
  if (Tok.is(tok::kw_PROGRAM)) {
    ProgStmt = ParsePROGRAMStmt();
    Body.push_back(ProgStmt);
  }

  // If the PROGRAM statement has an identifier, pass it on to the main program
  // action.
  const IdentifierInfo *IDInfo = 0;
  SourceLocation NameLoc;
  if (ProgStmt.isUsable()) {
    ProgramStmt *PS = ProgStmt.takeAs<ProgramStmt>();
    IDInfo = PS->getProgramName();
    NameLoc = PS->getNameLocation();
    // FIXME: Debugging
    dump(PS);
  }

  Actions.ActOnMainProgram(IDInfo, NameLoc);

  ParseExecutableSubprogramBody(Body, tok::kw_ENDPROGRAM);

  ParseStatementLabel();
  StmtResult EndProgStmt = ParseEND_PROGRAMStmt();
  Body.push_back(EndProgStmt);

  IDInfo = 0;
  NameLoc = SourceLocation();
  auto Loc = SourceLocation();
  if (EndProgStmt.isUsable()) {
    EndProgramStmt *EPS = EndProgStmt.takeAs<EndProgramStmt>();
    Loc = EPS->getLocation();
    IDInfo = EPS->getProgramName();
    NameLoc = EPS->getNameLocation();
  }

  Actions.ActOnEndMainProgram(Loc, IDInfo, NameLoc);
  return EndProgStmt.isInvalid();
}

bool Parser::ParseExecutableSubprogramBody(std::vector<StmtResult> &Body,
                                           tok::TokenKind EndKw) {
  // FIXME: Check for the specific keywords and not just absence of END or
  //        ENDPROGRAM.
  ParseStatementLabel();
  if (Tok.isNot(tok::kw_END) && Tok.isNot(EndKw))
    ParseSpecificationPart(Body);

  // Apply specification statements.
  Actions.ActOnSpecificationPart(Body);

  // FIXME: Check for the specific keywords and not just absence of END or
  //        ENDPROGRAM.
  ParseStatementLabel();
  if (Tok.isNot(tok::kw_END) && Tok.isNot(EndKw))
    ParseExecutionPart(Body);

  // FIXME: Debugging support.
  dump(Body);
}

/// ParseSpecificationPart - Parse the specification part.
///
///   [R204]:
///     specification-part :=
///        [use-stmt] ...
///          [import-stmt] ...
///          [implicit-part] ...
///          [declaration-construct] ...
bool Parser::ParseSpecificationPart(std::vector<StmtResult> &Body) {
  bool HasErrors = false;
  while (Tok.is(tok::kw_USE)) {
    StmtResult S = ParseUSEStmt();
    if (S.isUsable()) {
      Body.push_back(S);
    } else if (S.isInvalid()) {
      LexToEndOfStatement();
      HasErrors = true;
    } else {
      break;
    }

    ParseStatementLabel();
  }

  while (Tok.is(tok::kw_IMPORT)) {
    StmtResult S = ParseIMPORTStmt();
    if (S.isUsable()) {
      Body.push_back(S);
    } else if (S.isInvalid()) {
      LexToEndOfStatement();
      HasErrors = true;
    } else {
      break;
    }

    ParseStatementLabel();
  }

  if (ParseImplicitPartList(Body))
    HasErrors = true;


  if (ParseDeclarationConstructList(Body)) {
    LexToEndOfStatement();
    HasErrors = true;
  }

  return HasErrors;
}

/// ParseExternalSubprogram - Parse an external subprogram.
///
///   [R203]:
///     external-subprogram :=
///         function-subprogram
///      or subroutine-subprogram
///
///   [R1231]:
///     subroutine-subprogram :=
///         subroutine-stmt
///           [specification-part]
///           [execution-part]
///           [internal-subprogram-part]
///           end-subroutine-stmt
///
///   [R1223]:
///     function-subprogram :=
///         function-stmt
///           [specification-part]
///           [execution-part]
///           [internal-subprogram-part]
///           end-function-stmt
bool Parser::ParseExternalSubprogram(std::vector<StmtResult> &Body) {
  DeclSpec ReturnType;
  return ParseExternalSubprogram(Body, ReturnType);
}

bool Parser::ParseTypedExternalSubprogram(std::vector<StmtResult> &Body) {
  DeclSpec ReturnType;
  ParseDeclarationTypeSpec(ReturnType);
  if(Tok.isNot(tok::kw_FUNCTION)) {
    Diag.Report(getExpectedLoc(), diag::err_expected_kw)
      << "FUNCTION";
    return true;
  }
  return ParseExternalSubprogram(Body, ReturnType);
}

bool Parser::ParseExternalSubprogram(std::vector<StmtResult> &Body, DeclSpec &ReturnType) {
  bool IsSubroutine = Tok.is(tok::kw_SUBROUTINE);
  Lex();

  if (Tok.isAtStartOfStatement() ||
      !(Tok.is(tok::identifier) ||
       (Tok.getIdentifierInfo() &&
        isaKeyword(Tok.getIdentifierInfo()->getName()))
      )) {
    Diag.Report(getExpectedLoc(), diag::err_expected_ident);
    return true;
  }

  auto IDLoc = Tok.getLocation();
  auto II = Tok.getIdentifierInfo();
  Lex();

  Actions.ActOnSubProgram(Context, IsSubroutine, IDLoc, II, ReturnType);

  if(EatIfPresentInSameStmt(tok::l_paren)) {

    // argument list
    if(!Tok.isAtStartOfStatement() && Tok.isNot(tok::r_paren)) {
      do {
        if(Tok.isAtStartOfStatement()) break;
        if(IsSubroutine && Tok.is(tok::star)) {
          Actions.ActOnSubProgramStarArgument(Context, Tok.getLocation());
          Lex();
          continue;
        } else if(!(Tok.is(tok::identifier) ||
                  (Tok.getIdentifierInfo() &&
                   isaKeyword(Tok.getIdentifierInfo()->getName()))
                 )) {
          Diag.Report(getExpectedLoc(), diag::err_expected_ident);
          break;
        }

        Actions.ActOnSubProgramArgument(Context, Tok.getLocation(),
                                        Tok.getIdentifierInfo());
        Lex();
      } while(EatIfPresentInSameStmt(tok::comma));
    }

    // closing ')'
    if(!EatIfPresentInSameStmt(tok::r_paren)) {
      Diag.Report(getExpectedLoc(), diag::err_expected_rparen)
        << FixItHint(getExpectedLocForFixIt(),")");
      if(!Tok.isAtStartOfStatement())
        LexToEndOfStatement();
    }
  } else if(!IsSubroutine) {
    Diag.Report(getExpectedLoc(), diag::err_expected_lparen);
    if(!Tok.isAtStartOfStatement())
      LexToEndOfStatement();
  }

  ParseExecutableSubprogramBody(Body, IsSubroutine? tok::kw_ENDSUBROUTINE :
                                                    tok::kw_ENDFUNCTION);

  if(!Tok.isAtStartOfStatement())
    LexToEndOfStatement();
  ParseStatementLabel();
  auto EndLoc = Tok.getLocation();
  if(Tok.is(tok::kw_END) || Tok.is(tok::kw_ENDSUBROUTINE) ||
     Tok.is(tok::kw_ENDFUNCTION))
    Lex();//End..
  StmtLabel = nullptr;
  Actions.ActOnEndSubProgram(Context, EndLoc);

  return false;
}



/// ParseModule - Parse a module.
///
///   [R1104]:
///     module :=
///         module-stmt
///           [specification-part]
///           [module-subprogram-part]
///           end-module-stmt
bool Parser::ParseModule() {
  return false;
}

/// ParseBlockData - Parse block data.
///
///   [R1116]:
///     block-data :=
///         block-data-stmt
///           [specification-part]
///           end-block-data-stmt
bool Parser::ParseBlockData() {
  if (Tok.isNot(tok::kw_BLOCKDATA))
    return true;

  return false;
}

/// ParseImplicitPartList - Parse a (possibly empty) list of implicit part
/// statements.
bool Parser::ParseImplicitPartList(std::vector<StmtResult> &Body) {
  bool HasErrors = false;
  while (true) {
    StmtResult S = ParseImplicitPart();
    if (S.isUsable()) {
      Body.push_back(S);
    } else if (S.isInvalid()) {
      LexToEndOfStatement();
      HasErrors = true;
    } else {
      break;
    }
  }

  return HasErrors;
}

/// ParseImplicitPart - Parse the implicit part.
///
///   [R205]:
///     implicit-part :=
///         [implicit-part-stmt] ...
///           implicit-stmt
StmtResult Parser::ParseImplicitPart() {
  // [R206]:
  //   implicit-part-stmt :=
  //       implicit-stmt
  //    or parameter-stmt
  //    or format-stmt
  //    or entry-stmt [obs]
  ParseStatementLabel();
  StmtResult Result;
  switch (Tok.getKind()) {
  default: break;
  case tok::kw_IMPLICIT:  Result = ParseIMPLICITStmt();  break;
  case tok::kw_PARAMETER: Result = ParsePARAMETERStmt(); break;
  case tok::kw_FORMAT:    Result = ParseFORMATStmt();    break;
  case tok::kw_ENTRY:     Result = ParseENTRYStmt();     break;
  }

  return Result;
}

/// ParseExecutionPart - Parse the execution part.
///
///   [R208]:
///     execution-part :=
///         executable-construct
///           [ execution-part-construct ] ...
bool Parser::ParseExecutionPart(std::vector<StmtResult> &Body) {
  bool HadError = false;
  while (true) {
    StmtResult SR = ParseExecutableConstruct();
    if (SR.isInvalid()) {
      LexToEndOfStatement();
      HadError = true;
    } else if (!SR.isUsable()) {
      break;
    }
    Body.push_back(SR);
  }

  return HadError;
}

/// ParseDeclarationConstructList - Parse a (possibly empty) list of declaration
/// construct statements.
bool Parser::ParseDeclarationConstructList(std::vector<StmtResult> &Body) {
  while (!ParseDeclarationConstruct(Body))
    /* Parse them all */ ;

  return false;
}

/// ParseDeclarationConstruct - Parse a declaration construct.
///
///   [R207]:
///     declaration-construct :=
///         derived-type-def
///      or entry-stmt
///      or enum-def
///      or format-stmt
///      or interface-block
///      or parameter-stmt
///      or procedure-declaration-stmt
///      or specification-stmt
///      or type-declaration-stmt
///      or stmt-function-stmt
bool Parser::ParseDeclarationConstruct(std::vector<StmtResult> &Body) {
  ParseStatementLabel();

  SmallVector<DeclResult, 4> Decls;

  switch (Tok.getKind()) {
  default:
    // FIXME: error handling.
    if(ParseSpecificationStmt(Body)) return true;
    break;
  case tok::kw_TYPE:
  case tok::kw_CLASS: {
    if(!PeekAhead().is(tok::l_paren)){
      //FIXME: error handling?
      ParseDerivedTypeDefinitionStmt();
      break;
    }
  }
  case tok::kw_INTEGER:
  case tok::kw_REAL:
  case tok::kw_COMPLEX:
  case tok::kw_CHARACTER:
  case tok::kw_LOGICAL:
  case tok::kw_DOUBLEPRECISION:
  case tok::kw_DOUBLECOMPLEX: {
    if (ParseTypeDeclarationStmt(Decls)) {
      LexToEndOfStatement();
      // FIXME:
    }
    break;
  }
    // FIXME: And the rest?
  }

  return false;
}

/// ParseForAllConstruct - Parse a forall construct.
///
///   [R752]:
///     forall-construct :=
///         forall-construct-stmt
///           [forall-body-construct] ...
///           end-forall-stmt
bool Parser::ParseForAllConstruct() {
  return false;
}

/// ParseArraySpec - Parse an array specification.
///
///   [R510]:
///     array-spec :=
///         explicit-shape-spec-list
///      or assumed-shape-spec-list
///      or deferred-shape-spec-list
///      or assumed-size-spec
bool Parser::ParseArraySpec(SmallVectorImpl<std::pair<ExprResult,ExprResult> > &Dims) {
  if(!EatIfPresentInSameStmt(tok::l_paren)) {
    Diag.Report(getExpectedLoc(), diag::err_expected_lparen);
    goto error;
  }

  // [R511], [R512], [R513]:
  //   explicit-shape-spec :=
  //       [ lower-bound : ] upper-bound
  //   lower-bound :=
  //       specification-expr
  //   upper-bound :=
  //       specification-expr
  //
  // [R729]:
  //   specification-expr :=
  //       scalar-int-expr
  //
  // [R727]:
  //   int-expr :=
  //       expr
  //
  //   C708: int-expr shall be of type integer.
  do {
    ExprResult E = ParseExpression();
    if (E.isInvalid()) goto error;
    if(EatIfPresentInSameStmt(tok::colon)){
      ExprResult E2 = ParseExpression();
      if(E2.isInvalid()) goto error;
      Dims.push_back(std::make_pair(E,E2));
    }
    else Dims.push_back(std::make_pair(ExprResult(),E));
  } while (EatIfPresentInSameStmt(tok::comma));

  if (!EatIfPresentInSameStmt(tok::r_paren)) {
    Diag.Report(getExpectedLoc(), diag::err_expected_rparen)
      << FixItHint(getExpectedLocForFixIt(),")");
    goto error;
  }

  return false;
 error:
  return true;
}

/// ParsePROGRAMStmt - If there is a PROGRAM statement, parse it.
/// 
///   [11.1] R1102:
///     program-stmt :=
///         PROGRAM program-name
Parser::StmtResult Parser::ParsePROGRAMStmt() {
  // Check to see if we start with a 'PROGRAM' statement.
  const IdentifierInfo *IDInfo = Tok.getIdentifierInfo();
  SourceLocation ProgramLoc = Tok.getLocation();
  if (!isaKeyword(IDInfo->getName()) || Tok.isNot(tok::kw_PROGRAM))
    return Actions.ActOnPROGRAM(Context, 0, ProgramLoc, ProgramLoc,
                                StmtLabel);

  // Parse the program name.
  Lex();
  if (Tok.isNot(tok::identifier) || Tok.isAtStartOfStatement()) {
    Diag.Report(ProgramLoc, diag::err_expected_ident);
    return StmtError();
  }

  SourceLocation NameLoc = Tok.getLocation();
  IDInfo = Tok.getIdentifierInfo();
  Lex(); // Eat program name.
  return Actions.ActOnPROGRAM(Context, IDInfo, ProgramLoc, NameLoc,
                              StmtLabel);
}

/// ParseUSEStmt - Parse the 'USE' statement.
///
///   [R1109]:
///     use-stmt :=
///         USE [ [ , module-nature ] :: ] module-name [ , rename-list ]
///      or USE [ [ , module-nature ] :: ] module-name , ONLY : [ only-list ]
Parser::StmtResult Parser::ParseUSEStmt() {
  // Check if this is an assignment.
  if (PeekAhead().is(tok::equal))
    return StmtResult();

  Lex();

  // module-nature :=
  //     INTRINSIC
  //  or NON INTRINSIC
  UseStmt::ModuleNature MN = UseStmt::None;
  if (EatIfPresent(tok::comma)) {
    if (EatIfPresent(tok::kw_INTRINSIC)) {
      MN = UseStmt::Intrinsic;
    } else if (EatIfPresent(tok::kw_NONINTRINSIC)) {
      MN = UseStmt::NonIntrinsic;
    } else {
      Diag.ReportError(Tok.getLocation(),
                       "expected module nature keyword");
      return StmtResult(true);
    }

    if (!EatIfPresent(tok::coloncolon)) {
      Diag.ReportError(Tok.getLocation(),
                       "expected a '::' after the module nature");
      return StmtResult(true);
    }
  }

  // Eat optional '::'.
  EatIfPresent(tok::coloncolon);

  if (Tok.isNot(tok::identifier)) {
    Diag.ReportError(Tok.getLocation(),
                     "missing module name in USE statement");
    return StmtResult(true);
  }

  const IdentifierInfo *ModuleName = Tok.getIdentifierInfo();
  Lex();

  if (!EatIfPresent(tok::comma)) {
    if (!Tok.isAtStartOfStatement()) {
      Diag.ReportError(Tok.getLocation(),
                       "expected a ',' in USE statement");
      return StmtResult(true);
    }

    return Actions.ActOnUSE(Context, MN, ModuleName, StmtLabel);
  }

  bool OnlyUse = false;
  IdentifierInfo *UseListFirstVar = 0;
  if (Tok.is(tok::kw_ONLY)) {
    UseListFirstVar = Tok.getIdentifierInfo();
    Lex(); // Eat 'ONLY'
    if (!EatIfPresent(tok::colon)) {
      if (Tok.isNot(tok::equalgreater)) {
        Diag.ReportError(Tok.getLocation(),
                         "expected a ':' after the ONLY keyword");
        return StmtResult(true);
      }

      OnlyUse = false;
    } else {
      OnlyUse = true;
    }
  }

  SmallVector<UseStmt::RenamePair, 8> RenameNames;

  if (!OnlyUse && Tok.is(tok::equalgreater)) {
    // They're using 'ONLY' as a non-keyword and renaming it.
    Lex(); // Eat '=>'
    if (Tok.isAtStartOfStatement() || Tok.isNot(tok::identifier)) {
      Diag.ReportError(Tok.getLocation(),
                       "missing rename of variable in USE statement");
      return StmtResult(true);
    }

    RenameNames.push_back(UseStmt::RenamePair(UseListFirstVar,
                                              Tok.getIdentifierInfo()));
    Lex();
    EatIfPresent(tok::comma);
  }

  while (!Tok.isAtStartOfStatement()) {
    const IdentifierInfo *LocalName = Tok.getIdentifierInfo();
    const IdentifierInfo *UseName = 0;
    Lex();

    if (EatIfPresent(tok::equalgreater)) {
      UseName = Tok.getIdentifierInfo();
      Lex();
    }

    RenameNames.push_back(UseStmt::RenamePair(LocalName, UseName));

    if (!EatIfPresent(tok::comma))
      break;
  }

  return Actions.ActOnUSE(Context, MN, ModuleName, OnlyUse, RenameNames,
                          StmtLabel);
}

/// ParseIMPORTStmt - Parse the IMPORT statement.
///
///   [R1209]:
///     import-stmt :=
///         IMPORT [ [ :: ] import-name-list ]
Parser::StmtResult Parser::ParseIMPORTStmt() {
  // Check if this is an assignment.
  if (PeekAhead().is(tok::equal))
    return StmtResult();

  SourceLocation Loc = Tok.getLocation();
  Lex();
  EatIfPresent(tok::coloncolon);

  SmallVector<const IdentifierInfo*, 4> ImportNameList;
  while (!Tok.isAtStartOfStatement()) {
    if (Tok.isNot(tok::identifier)) {
      Diag.ReportError(Tok.getLocation(),
                       "expected import name in IMPORT statement");
      return StmtResult(true);
    }

    ImportNameList.push_back(Tok.getIdentifierInfo());
    Lex();
    if (!EatIfPresent(tok::comma))
      break;
  }

  if (!Tok.isAtStartOfStatement()) {
    Diag.ReportError(Tok.getLocation(),
                     "missing comma before import name in IMPORT statement");
    LexToEndOfStatement();
  }

  return Actions.ActOnIMPORT(Context, Loc, ImportNameList, StmtLabel);
}

/// ParseIMPLICITStmt - Parse the IMPLICIT statement.
///
///   [R560]:
///     implicit-stmt :=
///         IMPLICIT implicit-spec-list
///      or IMPLICIT NONE
Parser::StmtResult Parser::ParseIMPLICITStmt() {
  // Check if this is an assignment.
  if (PeekAhead().is(tok::equal))
    return StmtResult();

  SourceLocation Loc = Tok.getLocation();
  Lex();

  if (EatIfPresentInSameStmt(tok::kw_NONE))
    return Actions.ActOnIMPLICIT(Context, Loc, StmtLabel);

  SmallVector<Stmt*, 8> StmtList;

  do {
    DeclSpec DS;
    if (ParseDeclarationTypeSpec(DS))
      return StmtError();

    if (!EatIfPresentInSameStmt(tok::l_paren)) {
      Diag.Report(getExpectedLoc(),diag::err_expected_lparen);
      return StmtError();
    }

    do {
      const IdentifierInfo *First = Tok.getIdentifierInfo();
      if (Tok.isAtStartOfStatement() ||
          Tok.isNot(tok::identifier) ||
          First->getName().size() > 1) {
        Diag.Report(getExpectedLoc(),diag::err_expected_letter);
        return StmtError();
      }

      Lex();

      const IdentifierInfo *Second = nullptr;
      if (EatIfPresentInSameStmt(tok::minus)) {
        Second = Tok.getIdentifierInfo();
        if (Tok.isAtStartOfStatement() ||
            Tok.isNot(tok::identifier) ||
            Second->getName().size() > 1) {
          Diag.Report(getExpectedLoc(),diag::err_expected_letter);
          return StmtError();
        }

        Lex();
      }

      auto Stmt = Actions.ActOnIMPLICIT(Context, Loc, DS,
                                        std::make_pair(First, Second), nullptr);
      if(Stmt.isUsable())
        StmtList.push_back(Stmt.take());
    } while (EatIfPresentInSameStmt(tok::comma));

    if (!EatIfPresentInSameStmt(tok::r_paren)) {
      Diag.Report(getExpectedLoc(),diag::err_expected_rparen);
      return StmtError();
    }

  } while(EatIfPresentInSameStmt(tok::comma));

  return Actions.ActOnBundledCompoundStmt(Context, Loc, StmtList, StmtLabel);
}

/// ParsePARAMETERStmt - Parse the PARAMETER statement.
///
///   [R548]:
///     parameter-stmt :=
///         PARAMETER ( named-constant-def-list )
Parser::StmtResult Parser::ParsePARAMETERStmt() {
  // Check if this is an assignment.
  if (PeekAhead().is(tok::equal))
    return StmtResult();

  SourceLocation Loc = Tok.getLocation();
  Lex();

  SmallVector<Stmt*, 8> StmtList;

  if (!EatIfPresentInSameStmt(tok::l_paren)) {
    Diag.Report(getExpectedLoc(),diag::err_expected_lparen);
    return StmtError();
  }

  do {
    if (Tok.isAtStartOfStatement() ||
        Tok.isNot(tok::identifier)) {
      Diag.Report(getExpectedLoc(),
                  diag::err_expected_ident);
      return StmtError();
    }

    SourceLocation IDLoc = Tok.getLocation();
    const IdentifierInfo *II = Tok.getIdentifierInfo();
    Lex();

    auto EqualLoc = Tok.getLocation();
    if (!EatIfPresentInSameStmt(tok::equal)) {
      Diag.Report(getExpectedLoc(),diag::err_expected_equal);
      return StmtError();
    }

    ExprResult ConstExpr = ParseExpression();
    if (ConstExpr.isInvalid())
      return StmtError();

    auto Stmt = Actions.ActOnPARAMETER(Context, Loc, EqualLoc,
                                       IDLoc, II,
                                       ConstExpr, nullptr);
    if(Stmt.isUsable())
      StmtList.push_back(Stmt.take());
  } while(EatIfPresentInSameStmt(tok::comma));


  if (!EatIfPresentInSameStmt(tok::r_paren)) {
    if (Tok.isAtStartOfStatement())
      Diag.Report(getExpectedLoc(),diag::err_expected_rparen)
        << FixItHint(getExpectedLocForFixIt(),")");
    else {
      Diag.Report(getExpectedLoc(),diag::err_expected_comma)
        << FixItHint(getExpectedLocForFixIt(),",");
      return StmtError();
    }
  }

  return Actions.ActOnBundledCompoundStmt(Context, Loc, StmtList, StmtLabel);
}

/// ParseENTRYStmt - Parse the ENTRY statement.
///
///   [R1240]:
///     entry-stmt :=
///         ENTRY entry-name [ ( [ dummy-arg-list ] ) [ suffix ] ]
StmtResult Parser::ParseENTRYStmt() {
  return StmtResult();
}

/// ParseProcedureDeclStmt - Parse the procedure declaration statement.
///
///   [12.3.2.3] R1211:
///     procedure-declaration-stmt :=
///         PROCEDURE ([proc-interface]) [ [ , proc-attr-spec ]... :: ] #
///         # proc-decl-list
bool Parser::ParseProcedureDeclStmt() {
  return false;
}

/// ParseSpecificationStmt - Parse the specification statement.
///
///   [R212]:
///     specification-stmt :=
///         access-stmt
///      or allocatable-stmt
///      or asynchronous-stmt
///      or bind-stmt
///      or common-stmt
///      or data-stmt
///      or dimension-stmt
///      or equivalence-stmt
///      or external-stmt
///      or intent-stmt
///      or intrinsic-stmt
///      or namelist-stmt
///      or optional-stmt
///      or pointer-stmt
///      or protected-stmt
///      or save-stmt
///      or target-stmt
///      or value-stmt
///      or volatile-stmt
bool Parser::ParseSpecificationStmt(std::vector<StmtResult> &Body) {
  StmtResult Result;
  switch (Tok.getKind()) {
  default: return true;
  case tok::kw_PUBLIC:
  case tok::kw_PRIVATE:
    Result = ParseACCESSStmt();
    goto notImplemented;
  case tok::kw_ALLOCATABLE:
    Result = ParseALLOCATABLEStmt();
    goto notImplemented;
  case tok::kw_ASYNCHRONOUS:
    Result = ParseASYNCHRONOUSStmt();
    goto notImplemented;
  case tok::kw_BIND:
    Result = ParseBINDStmt();
    goto notImplemented;
  case tok::kw_COMMON:
    Result = ParseCOMMONStmt();
    goto notImplemented;
  case tok::kw_PARAMETER:
    Result = ParsePARAMETERStmt();
    break;
  case tok::kw_DATA:
    Result = ParseDATAStmt();
    break;
  case tok::kw_DIMENSION:
    Result = ParseDIMENSIONStmt();
    break;
  case tok::kw_EQUIVALENCE:
    Result = ParseEQUIVALENCEStmt();
    goto notImplemented;
  case tok::kw_EXTERNAL:
    Result = ParseEXTERNALStmt();
    break;
  case tok::kw_INTENT:
    Result = ParseINTENTStmt();
    goto notImplemented;
  case tok::kw_INTRINSIC:
    Result = ParseINTRINSICStmt();
    break;
  case tok::kw_NAMELIST:
    Result = ParseNAMELISTStmt();
    goto notImplemented;
  case tok::kw_OPTIONAL:
    Result = ParseOPTIONALStmt();
    goto notImplemented;
  case tok::kw_POINTER:
    Result = ParsePOINTERStmt();
    goto notImplemented;
  case tok::kw_PROTECTED:
    Result = ParsePROTECTEDStmt();
    goto notImplemented;
  case tok::kw_SAVE:
    Result = ParseSAVEStmt();
    goto notImplemented;
  case tok::kw_TARGET:
    Result = ParseTARGETStmt();
    goto notImplemented;
  case tok::kw_VALUE:
    Result = ParseVALUEStmt();
    goto notImplemented;
  case tok::kw_VOLATILE:
    Result = ParseVOLATILEStmt();
    goto notImplemented;
  }

  if(Result.isInvalid())
    LexToEndOfStatement();
  if(Result.isUsable())
    Body.push_back(Result);

  return false;
notImplemented:
  Diag.Report(Tok.getLocation(),
              diag::err_unsupported_stmt)
      << SourceRange(Tok.getLocation(),
                     getMaxLocationOfCurrentToken());
  LexToEndOfStatement();
  return false;
}

/// ParseACCESSStmt - Parse the ACCESS statement.
///
///   [R524]:
///     access-stmt :=
///         access-spec [[::] access-id-list]
Parser::StmtResult Parser::ParseACCESSStmt() {
  return StmtResult();
}

/// ParseALLOCATABLEStmt - Parse the ALLOCATABLE statement.
///
///   [R526]:
///     allocatable-stmt :=
///         ALLOCATABLE [::] object-name       #
///         # [ ( deferred-shape-spec-list ) ] #
///         # [ , object-name [ ( deferred-shape-spec-list ) ] ] ...
Parser::StmtResult Parser::ParseALLOCATABLEStmt() {
  return StmtResult();
}

/// ParseASYNCHRONOUSStmt - Parse the ASYNCHRONOUS statement.
///
///   [R528]:
///     asynchronous-stmt :=
///         ASYNCHRONOUS [::] object-name-list
Parser::StmtResult Parser::ParseASYNCHRONOUSStmt() {
  SourceLocation Loc = Tok.getLocation();
  Lex();
  EatIfPresent(tok::coloncolon);

  SmallVector<const IdentifierInfo*, 8> ObjNameList;
  while (!Tok.isAtStartOfStatement()) {
    if (Tok.isNot(tok::identifier)) {
      Diag.ReportError(Tok.getLocation(),
                       "expected an identifier in ASYNCHRONOUS statement");
      return StmtResult(true);
    }

    ObjNameList.push_back(Tok.getIdentifierInfo());
    Lex();
    if (!EatIfPresent(tok::comma)) {
      if (!Tok.isAtStartOfStatement()) {
        Diag.ReportError(Tok.getLocation(),
                         "expected ',' in ASYNCHRONOUS statement");
        continue;
      }
      break;
    }
  }

  return Actions.ActOnASYNCHRONOUS(Context, Loc, ObjNameList, StmtLabel);
}

/// ParseBINDStmt - Parse the BIND statement.
///
///   [5.2.4] R522:
///     bind-stmt :=
///         language-binding-spec [::] bind-entity-list
Parser::StmtResult Parser::ParseBINDStmt() {
  return StmtResult();
}

/// ParseCOMMONStmt - Parse the COMMON statement.
///
///   [5.5.2] R557:
///     common-stmt :=
///         COMMON #
///         # [ / [common-block-name] / ] common-block-object-list #
///         # [ [,] / [common-block-name / #
///         #   common-block-object-list ] ...
Parser::StmtResult Parser::ParseCOMMONStmt() {
  return StmtResult();
}

/// ParseDATAStmt - Parse the DATA statement.
///
///   [R524]:
///     data-stmt :=
///         DATA data-stmt-set [ [,] data-stmt-set ] ...
Parser::StmtResult Parser::ParseDATAStmt() {
  SourceLocation Loc = Tok.getLocation();
  Lex();

  SmallVector<Stmt *,8> StmtList;

  while(true) {
    auto Stmt = ParseDATAStmtPart(Loc);
    if(Stmt.isInvalid()) return StmtError();
    StmtList.push_back(Stmt.take());
    if(Tok.isAtStartOfStatement()) break;
  }

  return Actions.ActOnBundledCompoundStmt(Context, Loc, StmtList, StmtLabel);
}

Parser::StmtResult Parser::ParseDATAStmtPart(SourceLocation Loc) {
  SmallVector<ExprResult, 8> Names;
  SmallVector<ExprResult, 8> Values;

  // nlist
  do {
    if(Tok.isAtStartOfStatement()) {
      Diag.Report(getExpectedLoc(), diag::err_expected_expression);
      return StmtError();
    }

    ExprResult E;
    if(Tok.is(tok::l_paren)) {
      E = ParseDATAStmtImpliedDo();
      if(E.isUsable())
        E = Actions.ActOnDATAOuterImpliedDoExpr(Context, E);
    }
    else
       E = ParsePrimaryExpr();

     if(E.isInvalid())
       return StmtError();
     Names.push_back(E);
  } while(EatIfPresentInSameStmt(tok::comma));

  // clist
  if(!EatIfPresentInSameStmt(tok::slash)) {
    Diag.Report(getExpectedLoc(), diag::err_expected_slash);
    return StmtError();
  }

  do {
    if(Tok.isAtStartOfStatement()) {
      Diag.Report(getExpectedLoc(), diag::err_expected_expression);
      return StmtError();
    }

    ExprResult Repeat;
    SourceLocation RepeatLoc;
    if(Tok.is(tok::int_literal_constant)
       && PeekAhead().is(tok::star)) {
      Repeat = ParsePrimaryExpr();
      RepeatLoc = Tok.getLocation();
      EatIfPresentInSameStmt(tok::star);
      if(Tok.isAtStartOfStatement()) {
        Diag.Report(getExpectedLoc(), diag::err_expected_expression);
        return StmtError();
      }
    }
    auto Value = ParsePrimaryExpr();
    if(Value.isInvalid()) return StmtError();

    Value = Actions.ActOnDATAConstantExpr(Context, RepeatLoc, Repeat, Value);
    if(Value.isUsable())
      Values.push_back(Value);
  } while(EatIfPresentInSameStmt(tok::comma));

  if(!EatIfPresentInSameStmt(tok::slash)) {
    Diag.Report(getExpectedLoc(), diag::err_expected_slash);
    return StmtError();
  }

  return Actions.ActOnDATA(Context, Loc, Names, Values, nullptr);
}

Parser::ExprResult Parser::ParseDATAStmtImpliedDo() {
  auto Loc = Tok.getLocation();
  Lex();

  SmallVector<ExprResult, 8> DList;
  ExprResult E1, E2, E3;

  while(true) {
    ExprResult E;
    if(Tok.isAtStartOfStatement()) {
      Diag.Report(getExpectedLoc(), diag::err_expected_expression);
      return ExprError();
    }

    if(Tok.is(tok::l_paren))
      E = ParseDATAStmtImpliedDo();
    else {
      auto PrevDontResolveIdentifiersInSubExpressions =
             DontResolveIdentifiersInSubExpressions;
      DontResolveIdentifiersInSubExpressions = true;
      E = ParsePrimaryExpr();
      DontResolveIdentifiersInSubExpressions =
        PrevDontResolveIdentifiersInSubExpressions;
    }

    if(E.isInvalid()) return ExprError();
    DList.push_back(E);

    if(EatIfPresentInSameStmt(tok::comma)) {
      if(Tok.isAtStartOfStatement()) {
        Diag.Report(getExpectedLoc(), diag::err_expected_expression);
        return ExprError();
      }
      if(PeekAhead().is(tok::equal)) break;
    } else {
      Diag.Report(getExpectedLoc(), diag::err_expected_comma);
      return ExprError();
    }
  }

  if(!(Tok.is(tok::identifier) ||
     (Tok.getIdentifierInfo() &&
      isaKeyword(Tok.getIdentifierInfo()->getName())))) {
    Diag.Report(getExpectedLoc(), diag::err_expected_ident);
    return ExprError();
  }
  auto IDLoc = Tok.getLocation();
  auto IDInfo = Tok.getIdentifierInfo();
  Lex();

  if(!EatIfPresentInSameStmt(tok::equal)) {
    Diag.Report(getExpectedLoc(), diag::err_expected_equal);
    return ExprError();
  }

  auto PrevDontResolveIdentifiers = DontResolveIdentifiers;
  DontResolveIdentifiers = true;

  E1 = ParseExpectedFollowupExpression("=");
  if(!EatIfPresentInSameStmt(tok::comma)) {
    Diag.Report(getExpectedLoc(), diag::err_expected_comma);
    // NB: don't forget
    DontResolveIdentifiers = PrevDontResolveIdentifiers;
    return ExprError();
  }
  E2 = ParseExpectedFollowupExpression(",");
  if(EatIfPresentInSameStmt(tok::comma)) {
    E3 = ParseExpectedFollowupExpression(",");
  }

  DontResolveIdentifiers = PrevDontResolveIdentifiers;

  if(!EatIfPresentInSameStmt(tok::r_paren)) {
    Diag.Report(getExpectedLoc(), diag::err_expected_rparen);
    return ExprError();
  }

  return Actions.ActOnDATAImpliedDoExpr(Context, Loc, IDLoc, IDInfo,
                                        DList, E1, E2, E3);
}

/// ParseDIMENSIONStmt - Parse the DIMENSION statement.
///
///   [R535]:
///     dimension-stmt :=
///         DIMENSION [::] array-name ( array-spec ) #
///         # [ , array-name ( array-spec ) ] ...
Parser::StmtResult Parser::ParseDIMENSIONStmt() {
  SourceLocation Loc = Tok.getLocation();
  Lex();

  EatIfPresent(tok::coloncolon);

  SmallVector<Stmt*,8> StmtList;
  SmallVector<std::pair<ExprResult, ExprResult>, 4> Dimensions;
  while (true) {
    if (!(Tok.is(tok::identifier) ||
         (Tok.getIdentifierInfo() &&
          isaKeyword(Tok.getIdentifierInfo()->getName()))
        )) {
      Diag.Report(getExpectedLoc(), diag::err_expected_ident);
      return StmtError();
    }

    auto IDLoc = Tok.getLocation();
    auto ID = Tok.getIdentifierInfo();
    Lex();
    if(ParseArraySpec(Dimensions)) return StmtError();

    auto Stmt = Actions.ActOnDIMENSION(Context, Loc, IDLoc, ID,
                                       Dimensions, nullptr);
    if(Stmt.isUsable()) StmtList.push_back(Stmt.take());
    Dimensions.clear();

    if (!EatIfPresentInSameStmt(tok::comma)) {
      if (!Tok.isAtStartOfStatement()) {
        Diag.Report(getExpectedLoc(), diag::err_expected_comma);
        return StmtError();
      }
      break;
    }
  }
  return Actions.ActOnBundledCompoundStmt(Context, Loc, StmtList, StmtLabel);
}

/// ParseEQUIVALENCEStmt - Parse the EQUIVALENCE statement.
///
///   [R554]:
///     equivalence-stmt :=
///         EQUIVALENCE equivalence-set-list
Parser::StmtResult Parser::ParseEQUIVALENCEStmt() {
  return StmtResult();
}

/// ParseEXTERNALStmt - Parse the EXTERNAL statement.
///
///   [R1210]:
///     external-stmt :=
///         EXTERNAL [::] external-name-list
Parser::StmtResult Parser::ParseEXTERNALStmt() {
  return ParseINTRINSICStmt(/*IsActuallyExternal=*/ true);
}

/// ParseINTENTStmt - Parse the INTENT statement.
///
///   [R536]:
///     intent-stmt :=
///         INTENT ( intent-spec ) [::] dummy-arg-name-list
Parser::StmtResult Parser::ParseINTENTStmt() {
  return StmtResult();
}

/// ParseINTRINSICStmt - Parse the INTRINSIC statement.
///
///   [R1216]:
///     intrinsic-stmt :=
///         INTRINSIC [::] intrinsic-procedure-name-list
Parser::StmtResult Parser::ParseINTRINSICStmt(bool IsActuallyExternal) {
  SourceLocation Loc = Tok.getLocation();
  Lex();

  EatIfPresentInSameStmt(tok::coloncolon);

  SmallVector<Stmt *,8> StmtList;

  while (!Tok.isAtStartOfStatement()) {
    if (!(Tok.is(tok::identifier) ||
         (Tok.getIdentifierInfo() &&
          isaKeyword(Tok.getIdentifierInfo()->getName()))
        )) {
      Diag.Report(getExpectedLoc(), diag::err_expected_ident);
      return StmtError();
    }

    auto Stmt = IsActuallyExternal?
                  Actions.ActOnEXTERNAL(Context, Loc, Tok.getLocation(),
                                         Tok.getIdentifierInfo(), nullptr):
                  Actions.ActOnINTRINSIC(Context, Loc, Tok.getLocation(),
                                         Tok.getIdentifierInfo(), nullptr);
    if(Stmt.isUsable())
      StmtList.push_back(Stmt.take());

    Lex();
    if (!EatIfPresentInSameStmt(tok::comma)) {
      if (!Tok.isAtStartOfStatement()) {
        Diag.Report(getExpectedLoc(), diag::err_expected_comma);
        return StmtError();
      }
      break;
    }
  }

  return Actions.ActOnBundledCompoundStmt(Context, Loc, StmtList, StmtLabel);
}

/// ParseNAMELISTStmt - Parse the NAMELIST statement.
///
///   [R552]:
///     namelist-stmt :=
///         NAMELIST #
///         # / namelist-group-name / namelist-group-object-list #
///         # [ [,] / namelist-group-name / #
///         #   namelist-group-object-list ] ...
Parser::StmtResult Parser::ParseNAMELISTStmt() {
  return StmtResult();
}

/// ParseOPTIONALStmt - Parse the OPTIONAL statement.
///
///   [R537]:
///     optional-stmt :=
///         OPTIONAL [::] dummy-arg-name-list
Parser::StmtResult Parser::ParseOPTIONALStmt() {
  return StmtResult();
}

/// ParsePOINTERStmt - Parse the POINTER statement.
///
///   [R540]:
///     pointer-stmt :=
///         POINTER [::] pointer-decl-list
Parser::StmtResult Parser::ParsePOINTERStmt() {
  return StmtResult();
}

/// ParsePROTECTEDStmt - Parse the PROTECTED statement.
///
///   [R542]:
///     protected-stmt :=
///         PROTECTED [::] entity-name-list
Parser::StmtResult Parser::ParsePROTECTEDStmt() {
  return StmtResult();
}

/// ParseSAVEStmt - Parse the SAVE statement.
///
///   [R543]:
///     save-stmt :=
///         SAVE [ [::] saved-entity-list ]
Parser::StmtResult Parser::ParseSAVEStmt() {
  return StmtResult();
}

/// ParseTARGETStmt - Parse the TARGET statement.
///
///   [R546]:
///     target-stmt :=
///         TARGET [::] object-name [ ( array-spec ) ] #
///         # [ , object-name [ ( array-spec ) ] ] ...
Parser::StmtResult Parser::ParseTARGETStmt() {
  return StmtResult();
}

/// ParseVALUEStmt - Parse the VALUE statement.
///
///   [R547]:
///     value-stmt :=
///         VALUE [::] dummy-arg-name-list
Parser::StmtResult Parser::ParseVALUEStmt() {
  return StmtResult();
}

/// ParseVOLATILEStmt - Parse the VOLATILE statement.
///
///   [R548]:
///     volatile-stmt :=
///         VOLATILE [::] object-name-list
Parser::StmtResult Parser::ParseVOLATILEStmt() {
  return StmtResult();
}

/// ParseALLOCATEStmt - Parse the ALLOCATE statement.
///
///   [R623]:
///     allocate-stmt :=
///         ALLOCATE ( [ type-spec :: ] alocation-list [ , alloc-opt-list ] )
Parser::StmtResult Parser::ParseALLOCATEStmt() {
  return StmtResult();
}

/// ParseNULLIFYStmt - Parse the NULLIFY statement.
///
///   [R633]:
///     nullify-stmt :=
///         NULLIFY ( pointer-object-list )
Parser::StmtResult Parser::ParseNULLIFYStmt() {
  return StmtResult();
}

/// ParseDEALLOCATEStmt - Parse the DEALLOCATE statement.
///
///   [R635]:
///     deallocate-stmt :=
///         DEALLOCATE ( allocate-object-list [ , dealloc-op-list ] )
Parser::StmtResult Parser::ParseDEALLOCATEStmt() {
  return StmtResult();
}

/// ParseWHEREStmt - Parse the WHERE statement.
///
///   [R743]:
///     where-stmt :=
///         WHERE ( mask-expr ) where-assignment-stmt
Parser::StmtResult Parser::ParseWHEREStmt() {
  return StmtResult();
}

/// ParseFORALLStmt - Parse the FORALL construct statement.
///
///   [R753]:
///     forall-construct-stmt :=
///         [forall-construct-name :] FORALL forall-header
Parser::StmtResult Parser::ParseFORALLStmt() {
  return StmtResult();
}

/// ParseENDFORALLStmt - Parse the END FORALL construct statement.
/// 
///   [R758]:
///     end-forall-stmt :=
///         END FORALL [forall-construct-name]
Parser::StmtResult Parser::ParseEND_FORALLStmt() {
  return StmtResult();
}

} //namespace flang
