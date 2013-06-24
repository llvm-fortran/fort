//===--- Sema.h - Semantic Analysis & AST Building --------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines the Sema class, which performs semantic analysis and builds
// ASTs.
//
//===----------------------------------------------------------------------===//

#ifndef FLANG_SEMA_SEMA_H__
#define FLANG_SEMA_SEMA_H__

#include "flang/Basic/Token.h"
#include "flang/AST/FormatSpec.h"
#include "flang/AST/Stmt.h"
#include "flang/AST/Type.h"
#include "flang/AST/Expr.h"
#include "flang/Sema/Ownership.h"
#include "flang/Sema/Scope.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/SourceMgr.h"
#include "flang/Basic/LLVM.h"
#include <vector>

namespace flang {

class ASTContext;
class DeclContext;
class DeclSpec;
class DeclarationNameInfo;
class DiagnosticsEngine;
class Expr;
class FormatSpec;
class IdentifierInfo;
class Token;
class VarDecl;

class ExecutableProgramUnitStmts {
public:
  /// \brief A list of executable statements for all the blocks
  std::vector<StmtResult> StmtList;

  /// \brief Represents a statement with body(bodies) like DO or IF
  struct ControlFlowStmt {
    Stmt *Statement;
    size_t BeginOffset;
    /// \brief used only when a statement is a do which terminates
    /// with a labeled statement.
    Expr *ExpectedEndDoLabel;

    ControlFlowStmt()
      : Statement(nullptr),BeginOffset(0),
        ExpectedEndDoLabel(nullptr){
    }
    ControlFlowStmt(CFBlockStmt *S)
      : Statement(S), BeginOffset(0),
        ExpectedEndDoLabel(nullptr) {
    }
    ControlFlowStmt(DoStmt *S, Expr *ExpectedEndDo)
      : Statement(S), BeginOffset(0),
        ExpectedEndDoLabel(ExpectedEndDo) {
    }
    ControlFlowStmt(IfStmt *S)
      : Statement(S), BeginOffset(0) {
    }
    inline bool is(Stmt::StmtTy StmtType) const {
      return Statement->getStatementID() == StmtType;
    }
  };

  /// \brief A stack of current block statements like IF and DO
  SmallVector<ControlFlowStmt, 16> ControlFlowStack;

  ExecutableProgramUnitStmts() {}

  void Reset();

  void Enter(ControlFlowStmt S);
  void LeaveIfThen(ASTContext &C);
  void Leave(ASTContext &C);

  inline const ControlFlowStmt &LastEntered() const {
    return ControlFlowStack.back();
  }
  inline bool HasEntered() const {
    return ControlFlowStack.size() != 0;
  }
  bool HasEntered(Stmt::StmtTy StmtType) const;

  void Append(Stmt *S);
private:
  Stmt *CreateBody(ASTContext &C, const ControlFlowStmt &Last);
};

/// Sema - This implements semantic analysis and AST buiding for Fortran.
class Sema {
  Sema(const Sema&);           // DO NOT IMPLEMENT
  void operator=(const Sema&); // DO NOT IMPLEMENT

  /// \brief A statement label scope for the current program unit.
  StmtLabelScope CurStmtLabelScope;

  /// \brief A class which supports the executable statements in
  /// the current scope.
  ExecutableProgramUnitStmts CurExecutableStmts;

  /// \brief The implicit scope for the current program unit.
  ImplicitTypingScope CurImplicitTypingScope;

  /// \brief The mapping
  intrinsic::FunctionMapping IntrinsicFunctionMapping;

public:
  typedef Expr ExprTy;

  ASTContext &Context;
  DiagnosticsEngine &Diags;

  /// CurContext - This is the current declaration context of parsing.
  DeclContext *CurContext;

  Sema(ASTContext &ctxt, DiagnosticsEngine &Diags);
  ~Sema();

  DeclContext *getContainingDC(DeclContext *DC);

  inline StmtLabelScope& getCurrentStmtLabelScope() {
    return CurStmtLabelScope;
  }

  inline ImplicitTypingScope& getCurrentImplicitTypingScope() {
    return CurImplicitTypingScope;
  }

  inline ExprResult ExprError() const { return ExprResult(true); }
  inline StmtResult StmtError() const { return StmtResult(true); }

  /// Set the current declaration context until it gets popped.
  void PushDeclContext(DeclContext *DC);
  void PopDeclContext();

  void PushExecutableProgramUnit();
  void PopExecutableProgramUnit(SourceLocation Loc);

  void DeclareStatementLabel(Expr *StmtLabel, Stmt *S);
  void CheckStatementLabelEndDo(Expr *StmtLabel, Stmt *S);

  void ActOnTranslationUnit();
  void ActOnEndProgramUnit();

  void ActOnMainProgram(const IdentifierInfo *IDInfo, SourceLocation NameLoc);
  void ActOnEndMainProgram(SourceLocation Loc, const IdentifierInfo *IDInfo, SourceLocation NameLoc);

  void ActOnSpecificationPart(ArrayRef<StmtResult> Body);
  VarDecl *GetVariableForSpecification(const IdentifierInfo *IDInfo,
                                       SourceLocation ErrorLoc,
                                       const llvm::Twine &ErrorMsg);
  bool ApplySpecification(const DimensionStmt *Stmt);

  QualType ActOnTypeName(ASTContext &C, DeclSpec &DS);
  VarDecl *ActOnKindSelector(ASTContext &C, SourceLocation IDLoc,
                             const IdentifierInfo *IDInfo);

  Decl *ActOnEntityDecl(ASTContext &C, const QualType &T, SourceLocation IDLoc,
                        const IdentifierInfo *IDInfo);

  Decl *ActOnEntityDecl(ASTContext &C, DeclSpec &DS, SourceLocation IDLoc,
                        const IdentifierInfo *IDInfo);

  Decl *ActOnImplicitEntityDecl(ASTContext &C, SourceLocation IDLoc,
                                const IdentifierInfo *IDInfo);

  /// Returns a declaration which matches the identifier in this context
  Decl *LookupIdentifier(const IdentifierInfo *IDInfo);

  Decl *ResolveIdentifier(const IdentifierInfo *IDInfo);

  // FIXME: TODO more features.
  RecordDecl *ActOnDerivedTypeDecl(ASTContext &C, SourceLocation Loc,
                                   SourceLocation NameLoc, const IdentifierInfo* IDInfo);

  FieldDecl *ActOnDerivedTypeFieldDecl(ASTContext &C, DeclSpec &DS, SourceLocation IDLoc,
                                       const IdentifierInfo *IDInfo,
                                       ExprResult Init = ExprResult());

  void ActOnEndDerivedTypeDecl();

  StmtResult ActOnBundledCompoundStmt(ASTContext &C, SourceLocation Loc,
                                      ArrayRef<Stmt*> Body, Expr *StmtLabel);

  // PROGRAM statement:
  StmtResult ActOnPROGRAM(ASTContext &C, const IdentifierInfo *ProgName,
                          SourceLocation Loc, SourceLocation NameLoc, Expr *StmtLabel);

  // USE statement:
  StmtResult ActOnUSE(ASTContext &C, UseStmt::ModuleNature MN,
                      const IdentifierInfo *ModName, Expr *StmtLabel);
  StmtResult ActOnUSE(ASTContext &C, UseStmt::ModuleNature MN,
                      const IdentifierInfo *ModName, bool OnlyList,
                      ArrayRef<UseStmt::RenamePair> RenameNames,
                      Expr *StmtLabel);

  // IMPORT statement:
  StmtResult ActOnIMPORT(ASTContext &C, SourceLocation Loc,
                         ArrayRef<const IdentifierInfo*> ImportNamesList,
                         Expr *StmtLabel);

  // IMPLICIT statement:
  StmtResult ActOnIMPLICIT(ASTContext &C, SourceLocation Loc, DeclSpec &DS,
                           ImplicitStmt::LetterSpecTy LetterSpec, Expr *StmtLabel);

  StmtResult ActOnIMPLICIT(ASTContext &C, SourceLocation Loc, Expr *StmtLabel);

  // DIMENSION statement
  // The source code statement is split into multiple ones in the parsing stage.
  StmtResult ActOnDIMENSION(ASTContext &C, SourceLocation Loc,
                            const IdentifierInfo *IDInfo,
                            ArrayRef<std::pair<ExprResult,ExprResult> > Dims,
                            Expr *StmtLabel);

  // PARAMETER statement:
  StmtResult ActOnPARAMETER(ASTContext &C, SourceLocation Loc,
                            SourceLocation IDLoc,
                            const IdentifierInfo *IDInfo,
                            ExprResult Value,
                            Expr *StmtLabel);

  // ASYNCHRONOUS statement:
  StmtResult ActOnASYNCHRONOUS(ASTContext &C, SourceLocation Loc,
                               ArrayRef<const IdentifierInfo*> ObjNames,
                               Expr *StmtLabel);

  // END PROGRAM statement:
  StmtResult ActOnENDPROGRAM(ASTContext &C,
                             const IdentifierInfo *ProgName,
                             SourceLocation Loc, SourceLocation NameLoc,
                             Expr *StmtLabel);

  // EXTERNAL statement:
  StmtResult ActOnEXTERNAL(ASTContext &C, SourceLocation Loc,
                           ArrayRef<const IdentifierInfo *> ExternalNames,
                           Expr *StmtLabel);

  // INTRINSIC statement:
  StmtResult ActOnINTRINSIC(ASTContext &C, SourceLocation Loc,
                            SourceLocation IDLoc,
                            const IdentifierInfo *IDInfo,
                            Expr *StmtLabel);

  StmtResult ActOnAssignmentStmt(ASTContext &C, SourceLocation Loc,
                                 ExprResult LHS,
                                 ExprResult RHS, Expr *StmtLabel);

  QualType ActOnArraySpec(ASTContext &C, QualType ElemTy,
                          ArrayRef<std::pair<ExprResult,ExprResult> > Dims);

  StarFormatSpec *ActOnStarFormatSpec(ASTContext &C, SourceLocation Loc);
  DefaultCharFormatSpec *ActOnDefaultCharFormatSpec(ASTContext &C,
                                                    SourceLocation Loc,
                                                    ExprResult Fmt);
  LabelFormatSpec *ActOnLabelFormatSpec(ASTContext &C, SourceLocation Loc,
                                        ExprResult Label);

  StmtResult ActOnBlock(ASTContext &C, SourceLocation Loc, ArrayRef<StmtResult> Body);

  StmtResult ActOnAssignStmt(ASTContext &C, SourceLocation Loc,
                             ExprResult Value, VarExpr* VarRef,
                             Expr *StmtLabel);

  StmtResult ActOnAssignedGotoStmt(ASTContext &C, SourceLocation Loc,
                                   VarExpr* VarRef, ArrayRef<ExprResult> AllowedValues,
                                   Expr *StmtLabel);

  StmtResult ActOnGotoStmt(ASTContext &C, SourceLocation Loc,
                           ExprResult Destination, Expr *StmtLabel);

  StmtResult ActOnIfStmt(ASTContext &C, SourceLocation Loc,
                         ExprResult Condition, Expr *StmtLabel);
  StmtResult ActOnElseIfStmt(ASTContext &C, SourceLocation Loc,
                             ExprResult Condition, Expr *StmtLabel);
  StmtResult ActOnElseStmt(ASTContext &C, SourceLocation Loc, Expr *StmtLabel);
  StmtResult ActOnEndIfStmt(ASTContext &C, SourceLocation Loc, Expr *StmtLabel);

  StmtResult ActOnDoStmt(ASTContext &C, SourceLocation Loc, ExprResult TerminatingStmt,
                         VarExpr *DoVar, ExprResult E1, ExprResult E2,
                         ExprResult E3, Expr *StmtLabel);
  StmtResult ActOnEndDoStmt(ASTContext &C, SourceLocation Loc, Expr *StmtLabel);

  StmtResult ActOnContinueStmt(ASTContext &C, SourceLocation Loc, Expr *StmtLabel);

  StmtResult ActOnStopStmt(ASTContext &C, SourceLocation Loc, ExprResult StopCode, Expr *StmtLabel);

  StmtResult ActOnPrintStmt(ASTContext &C, SourceLocation Loc, FormatSpec *FS,
                            ArrayRef<ExprResult> OutputItemList,
                            Expr *StmtLabel);

  // FIXME: TODO:

  QualType ActOnBuiltinType(ASTContext *Ctx,
                            BuiltinType::TypeSpec TS,
                            Expr *Kind) { return QualType(); }
  QualType ActOnCharacterBuiltinType(ASTContext *Ctx,
                                     Expr *Len,
                                     Expr *Kind) { return QualType(); }
  DeclSpec *ActOnTypeDeclSpec(ASTContext *Ctx) { return 0; }

  ExprResult ActOnDataReference(llvm::ArrayRef<ExprResult> Exprs) {
    return ExprResult();
  }

  ExprResult ActOnUnaryExpr(ASTContext &C, SourceLocation Loc,
                            UnaryExpr::Operator Op, ExprResult E);

  ExprResult ActOnBinaryExpr(ASTContext &C, SourceLocation Loc,
                             BinaryExpr::Operator Op,
                             ExprResult LHS,ExprResult RHS);

  ExprResult ActOnSubstringExpr(ASTContext &C, SourceLocation Loc, ExprResult Target,
                                ExprResult StartingPoint, ExprResult EndPoint);
  ExprResult ActOnSubscriptExpr(ASTContext &C, SourceLocation Loc, ExprResult Target,
                                llvm::ArrayRef<ExprResult> Subscripts);

  ExprResult ActOnIntrinsicFunctionCallExpr(ASTContext &C, SourceLocation Loc,
                                            const IntrinsicFunctionDecl *FunctionDecl,
                                            ArrayRef<ExprResult> Arguments);

};

} // end flang namespace

#endif
