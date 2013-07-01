//===-- Parser.h - Fortran Parser Interface ---------------------*- C++ -*-===//
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

#ifndef FLANG_PARSER_PARSER_H__
#define FLANG_PARSER_PARSER_H__

#include "flang/AST/ASTContext.h" // FIXME: Move to AST construction.
#include "flang/Basic/Diagnostic.h"
#include "flang/Basic/IdentifierTable.h"
#include "flang/Basic/LangOptions.h"
#include "flang/Basic/TokenKinds.h"
#include "flang/Parse/Lexer.h"
#include "flang/Sema/DeclSpec.h"
#include "flang/Sema/Ownership.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/Twine.h"
#include <vector>

namespace llvm {
  class SourceMgr;
} // end namespace llvm

namespace flang {

class Action;
class VarExpr;
class ConstantExpr;
class DeclGroupRef;
class Expr;
class Parser;
class Selector;
class Sema;
class UnitSpec;
class FormatSpec;

/// PrettyStackTraceParserEntry - If a crash happens while the parser is active,
/// an entry is printed for it.
class PrettyStackTraceParserEntry : public llvm::PrettyStackTraceEntry {
  const Parser &FP;
public:
  PrettyStackTraceParserEntry(const Parser &fp) : FP(fp) {}
  virtual void print(llvm::raw_ostream &OS) const;
};

/// Parser - This implements a parser for the Fortran family of languages. After
/// parsing units of the grammar, productions are invoked to handle whatever has
/// been read.
class Parser {
public:
  enum RetTy {
    Success,                //< The construct was parsed successfully
    WrongConstruct,         //< The construct we wanted to parse wasn't present
    Error                   //< There was an error parsing
  };
private:
  Lexer TheLexer;
  LangOptions Features;
  PrettyStackTraceParserEntry CrashInfo;
  llvm::SourceMgr &SrcMgr;
  /// This is a stack of lexing contexts for files lower in the include stack
  std::vector<const char*> LexerBufferContext;

  /// This is the current buffer index we're lexing from as managed by the
  /// SourceMgr object.
  int CurBuffer;

  ASTContext &Context;

  /// Diag - Diagnostics for parsing errors.
  DiagnosticsEngine &Diag;

  /// Actions - These are the callbacks we invoke as we parse various constructs
  /// in the file. 
  Sema &Actions;
#if 0
  /// Actions - These are the callbacks we invoke as we parse various constructs
  /// in the file.
  Action &Actions;
#endif

  /// Tok - The current token we are parsing. All parsing methods assume that
  /// this is valid.
  Token Tok;

  /// NextTok - The next token so that we can do one level of lookahead.
  Token NextTok;

  /// StmtLabel - If set, this is the statement label for the statement.
  Expr *StmtLabel;

  // PrevTokLocation - The location of the token we previously consumed. This
  // token is used for diagnostics where we expected to see a token following
  // another token.
  SourceLocation PrevTokLocation;

  /// Returns the end of location of the previous token.
  SourceLocation PrevTokLocEnd;

  /// DontResolveIdentifiers - if set, the identifier tokens create
  /// an UnresolvedIdentifierExpr, instead of resolving the identifier.
  /// As a of this result Subscript, Substring and function call
  /// expressions aren't parsed.
  bool DontResolveIdentifiers;

  /// DontResolveIdentifiersInSubExpressions - if set,
  /// ParsePrimaryExpression will set the DontResolveIdentifiers
  /// to true when parsing any subexpressions like array subscripts,
  /// etc.
  bool DontResolveIdentifiersInSubExpressions;

  /// LexFORMATTokens - if set,
  /// The lexer will lex the format descriptor tokens instead
  /// of normal tokens.
  bool LexFORMATTokens;

  /// Identifiers - This is mapping/lookup information for all identifiers in
  /// the program, including program keywords.
  mutable IdentifierTable Identifiers;

  /// getIdentifierInfo - Return information about the specified identifier
  /// token.
  IdentifierInfo *getIdentifierInfo(std::string &Name) const {
    return &Identifiers.get(Name);
  }

  /// ConsumeToken - Consume the current 'peek token' and lex the next one. This
  /// returns the location of the consumed token.
  SourceLocation ConsumeToken() {
    PrevTokLocation = Tok.getLocation();
    TheLexer.Lex(Tok);
    return PrevTokLocation;
  }

  /// Returns the maximum location of the current token
  SourceLocation getMaxLocationOfCurrentToken() {
    return SourceLocation::getFromPointer(Tok.getLocation().getPointer() +
                                       Tok.getLength());
  }

  /// CleanLiteral - Cleans up a literal if it needs cleaning. It removes the
  /// continuation contexts and comments. Cleaning a dirty literal is SLOW!
  void CleanLiteral(Token T, std::string &NameStr);

  bool EnterIncludeFile(const std::string &Filename);
  bool LeaveIncludeFile();

  const Token &PeekAhead() const {
    return NextTok;
  }

  void Lex();
  void ClassifyToken(Token &T);
public:
  typedef OpaquePtr<DeclGroupRef> DeclGroupPtrTy;

  typedef flang::ExprResult ExprResult;
  typedef flang::StmtResult StmtResult;
  typedef flang::FormatItemResult FormatItemResult;

  bool isaIdentifier(const llvm::StringRef &ID) const {
    return Identifiers.isaIdentifier(ID);
  }
  bool isaKeyword(const llvm::StringRef &KW) const {
    return Identifiers.isaKeyword(KW);
  }

  Parser(llvm::SourceMgr &SrcMgr, const LangOptions &Opts,
         DiagnosticsEngine &D, Sema &actions);

  llvm::SourceMgr &getSourceManager() { return SrcMgr; }

  const Token &getCurToken() const { return Tok; }
  const Lexer &getLexer() const { return TheLexer; }
  Lexer &getLexer() { return TheLexer; }

  bool ParseProgramUnits();

  ExprResult ExprError() { return ExprResult(true); }
  StmtResult StmtError() { return StmtResult(true); }

private:

  /// If an error occured because an expected token
  /// isn't there, this returns the location of where
  /// the expected token should be.
  SourceLocation getExpectedLoc() const;

  ///
  inline SourceLocation getExpectedLocForFixIt() const {
    return PrevTokLocEnd;
  }

  // High-level parsing methods.
  bool ParseInclude();
  bool ParseProgramUnit();
  bool ParseMainProgram(std::vector<StmtResult> &Body);
  bool ParseExternalSubprogram(std::vector<StmtResult> &Body);
  bool ParseExternalSubprogram(std::vector<StmtResult> &Body,
                               DeclSpec &ReturnType);
  bool ParseTypedExternalSubprogram(std::vector<StmtResult> &Body);
  bool ParseExecutableSubprogramBody(std::vector<StmtResult> &Body,
                                     tok::TokenKind EndKw);
  bool ParseModule();
  bool ParseBlockData();

  bool ParseSpecificationPart(std::vector<StmtResult> &Body);
  bool ParseImplicitPartList(std::vector<StmtResult> &Body);
  StmtResult ParseImplicitPart();
  bool ParseExecutionPart(std::vector<StmtResult> &Body);

  bool ParseDeclarationConstructList(std::vector<StmtResult> &Body);
  bool ParseDeclarationConstruct(std::vector<StmtResult> &Body);
  bool ParseForAllConstruct();
  StmtResult ParseExecutableConstruct();

  bool ParseTypeDeclarationStmt(SmallVectorImpl<DeclResult> &Decls);
  bool ParseProcedureDeclStmt();
  bool ParseSpecificationStmt(std::vector<StmtResult> &Body);
  StmtResult ParseActionStmt();

  // Designator parsing methods.
  ExprResult ParseDesignator(bool IsLvalue);
  ExprResult ParseArrayElement();
  ExprResult ParseArraySection();
  ExprResult ParseCoindexedNamedObject();
  ExprResult ParseComplexPartDesignator();
  ExprResult ParseStructureComponent();
  ExprResult ParseSubstring(ExprResult Target);
  ExprResult ParseF77Subscript(ExprResult Target);
  ExprResult ParseDataReference();
  ExprResult ParsePartReference();

  // Stmt-level parsing methods.
  StmtResult ParsePROGRAMStmt();
  StmtResult ParseUSEStmt();
  StmtResult ParseIMPORTStmt();
  StmtResult ParseIMPLICITStmt();
  StmtResult ParsePARAMETERStmt();
  StmtResult ParseFORMATStmt();
  StmtResult ParseFORMATSpec(SourceLocation Loc);
  FormatItemResult ParseFORMATItems(bool IsOuter = false);
  FormatItemResult ParseFORMATItem();
  StmtResult ParseENTRYStmt();
  StmtResult ParseEND_PROGRAMStmt();

  // Specification statement's contents.
  StmtResult ParseACCESSStmt();
  StmtResult ParseALLOCATABLEStmt();
  StmtResult ParseASYNCHRONOUSStmt();
  StmtResult ParseBINDStmt();
  StmtResult ParseCOMMONStmt();
  StmtResult ParseDATAStmt();
  StmtResult ParseDATAStmtPart(SourceLocation Loc);
  ExprResult ParseDATAStmtImpliedDo();
  StmtResult ParseDIMENSIONStmt();
  StmtResult ParseEQUIVALENCEStmt();
  StmtResult ParseEXTERNALStmt();
  StmtResult ParseINTENTStmt();
  StmtResult ParseINTRINSICStmt(bool IsActuallyExternal = false);
  StmtResult ParseNAMELISTStmt();
  StmtResult ParseOPTIONALStmt();
  StmtResult ParsePOINTERStmt();
  StmtResult ParsePROTECTEDStmt();
  StmtResult ParseSAVEStmt();
  StmtResult ParseTARGETStmt();
  StmtResult ParseVALUEStmt();
  StmtResult ParseVOLATILEStmt();

  // Dynamic association.
  StmtResult ParseALLOCATEStmt();
  StmtResult ParseNULLIFYStmt();
  StmtResult ParseDEALLOCATEStmt();

  StmtResult ParseWHEREStmt();
  StmtResult ParseFORALLStmt();
  StmtResult ParseEND_FORALLStmt();

  // Executable statements
  StmtResult ParseBlockStmt();
  StmtResult ParseAssignStmt();
  StmtResult ParseGotoStmt();

  StmtResult ParseIfStmt();
  StmtResult ParseElseIfStmt();
  StmtResult ParseElseStmt();
  StmtResult ParseEndIfStmt();
  ExprResult ParseExpectedConditionExpression(const char *DiagAfter);

  StmtResult ParseDoStmt();
  StmtResult ParseEndDoStmt();

  StmtResult ParseContinueStmt();
  StmtResult ParseStopStmt();
  StmtResult ParseAssignmentStmt();
  StmtResult ParsePrintStmt();
  StmtResult ParseWriteStmt();
  UnitSpec *ParseUNITSpec(bool IsLabeled);
  FormatSpec *ParseFMTSpec(bool IsLabeled);
  void ParseIOList(SmallVectorImpl<ExprResult> &List);

  // Helper functions.
  ExprResult ParseLevel5Expr();
  ExprResult ParseEquivOperand();
  ExprResult ParseOrOperand();
  ExprResult ParseAndOperand();
  ExprResult ParseLevel4Expr();
  ExprResult ParseLevel3Expr();
  ExprResult ParseLevel2Expr();
  ExprResult ParseAddOperand();
  ExprResult ParseMultOperand();
  ExprResult ParseLevel1Expr();
  ExprResult ParsePrimaryExpr(bool IsLvalue = false);
  ExprResult ParseExpression();
  ExprResult ParseFunctionCallArgumentList(SmallVectorImpl<ExprResult> &Args);

  /// \brief Looks at the next token to see if it's an expression
  /// and calls ParseExpression if it is, or reports an expected expression
  /// error.
  ExprResult ParseExpectedFollowupExpression(const char *DiagAfter = "");

  void ParseStatementLabel();
  ExprResult ParseStatementLabelReference();

  VarExpr *ParseVariableReference();
  VarExpr *ParseIntegerVariableReference();

  // Declaration construct functions
  bool ParseDerivedTypeDefinitionStmt();
  bool ParseDerivedTypeComponent();
  bool ParseDerivedTypeComponentDeclarationList(DeclSpec &DS,
                                SmallVectorImpl<DeclResult> &Decls);

  bool ParseDeclarationTypeSpec(DeclSpec &DS);
  bool ParseTypeOrClassDeclTypeSpec(DeclSpec &DS);
  ExprResult ParseSelector(bool IsKindSel);
  bool ParseDerivedTypeSpec(DeclSpec &DS);
  bool ParseArraySpec(llvm::SmallVectorImpl<std::pair<ExprResult,ExprResult> > &Dims);
  bool ParseTypeDeclarationList(DeclSpec &DS,
                                SmallVectorImpl<DeclResult> &Decls);

  bool AssignAttrSpec(DeclSpec &DS, DeclSpec::AS Val);
  bool AssignAccessSpec(DeclSpec &DS, DeclSpec::AC Val);
  bool AssignIntentSpec(DeclSpec &DS, DeclSpec::IS Val);

  void SetKindSelector(ConstantExpr *E, StringRef Kind);

  // Declaration Helper functions
  bool ParseOptionalMatchingIdentifier(const IdentifierInfo*);

  void LexToEndOfStatement();
  bool EatIfPresent(tok::TokenKind);
  bool EatIfPresentInSameStmt(tok::TokenKind);
  bool Expect(tok::TokenKind,const llvm::Twine&);
};

} // end flang namespace

#endif
