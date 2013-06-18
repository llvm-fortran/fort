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
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/SMLoc.h"
#include "flang/Basic/LLVM.h"

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

/// Sema - This implements semantic analysis and AST buiding for Fortran.
class Sema {
  Sema(const Sema&);           // DO NOT IMPLEMENT
  void operator=(const Sema&); // DO NOT IMPLEMENT
public:
  typedef Expr ExprTy;

  ASTContext &Context;
  DiagnosticsEngine &Diags;

  /// CurContext - This is the current declaration context of parsing.
  DeclContext *CurContext;

  Sema(ASTContext &ctxt, DiagnosticsEngine &Diags);
  ~Sema();

  DeclContext *getContainingDC(DeclContext *DC);

  inline ExprResult ExprError() const { return ExprResult(true); }
  inline StmtResult StmtError() const { return StmtResult(true); }

  /// Set the current declaration context until it gets popped.
  void PushDeclContext(DeclContext *DC);
  void PopDeclContext();

  void ActOnTranslationUnit();
  void ActOnEndProgramUnit();

  void ActOnMainProgram(const IdentifierInfo *IDInfo, SMLoc NameLoc);
  void ActOnEndMainProgram(const IdentifierInfo *IDInfo, SMLoc NameLoc);

  void ActOnSpecificationPart(ArrayRef<StmtResult> Body);
  VarDecl *GetVariableForSpecification(const IdentifierInfo *IDInfo,
                                       SMLoc ErrorLoc,
                                       const llvm::Twine &ErrorMsg);
  bool ApplySpecification(const DimensionStmt *Stmt);

  QualType ActOnTypeName(ASTContext &C, DeclSpec &DS);
  VarDecl *ActOnKindSelector(ASTContext &C, SMLoc IDLoc,
                             const IdentifierInfo *IDInfo);
  Decl *ActOnEntityDecl(ASTContext &C, DeclSpec &DS, SMLoc IDLoc,
                        const IdentifierInfo *IDInfo);

  Decl *ActOnImplicitEntityDecl(ASTContext &C, SMLoc IDLoc,
                                const IdentifierInfo *IDInfo);

  // FIXME: TODO more features.
  RecordDecl *ActOnDerivedTypeDecl(ASTContext &C, SMLoc Loc,
                                      SMLoc NameLoc, const IdentifierInfo* IDInfo);

  FieldDecl *ActOnDerivedTypeFieldDecl(ASTContext &C, DeclSpec &DS, SMLoc IDLoc,
                                       const IdentifierInfo *IDInfo,
                                       ExprResult Init = ExprResult());

  void ActOnEndDerivedTypeDecl();


  // PROGRAM statement:
  StmtResult ActOnPROGRAM(ASTContext &C, const IdentifierInfo *ProgName,
                          SMLoc Loc, SMLoc NameLoc, Expr *StmtLabel);

  // USE statement:
  StmtResult ActOnUSE(ASTContext &C, UseStmt::ModuleNature MN,
                      const IdentifierInfo *ModName, ExprResult StmtLabel);
  StmtResult ActOnUSE(ASTContext &C, UseStmt::ModuleNature MN,
                      const IdentifierInfo *ModName, bool OnlyList,
                      ArrayRef<UseStmt::RenamePair> RenameNames,
                      ExprResult StmtLabel);

  // IMPORT statement:
  StmtResult ActOnIMPORT(ASTContext &C, SMLoc Loc,
                         ArrayRef<const IdentifierInfo*> ImportNamesList,
                         ExprResult StmtLabel);

  // IMPLICIT statement:
  StmtResult ActOnIMPLICIT(ASTContext &C, SMLoc Loc, DeclSpec &DS,
                           ArrayRef<ImplicitStmt::LetterSpec> LetterSpecs,
                           Expr *StmtLabel);
  StmtResult ActOnIMPLICIT(ASTContext &C, SMLoc Loc, Expr *StmtLabel);

  // DIMENSION statement
  // The source code statement is split into multiple ones in the parsing stage.
  StmtResult ActOnDIMENSION(ASTContext &C, SMLoc Loc,
                            const IdentifierInfo *IDInfo,
                            ArrayRef<std::pair<ExprResult,ExprResult> > Dims,
                            Expr *StmtLabel);

  // PARAMETER statement:
  ParameterStmt::ParamPair ActOnPARAMETERPair(ASTContext &C, SMLoc Loc,
                                              const IdentifierInfo *IDInfo,
                                              ExprResult CE);
  StmtResult ActOnPARAMETER(ASTContext &C, SMLoc Loc,
                            ArrayRef<ParameterStmt::ParamPair> ParamList,
                            Expr *StmtLabel);

  // ASYNCHRONOUS statement:
  StmtResult ActOnASYNCHRONOUS(ASTContext &C, SMLoc Loc,
                               ArrayRef<const IdentifierInfo*> ObjNames,
                               Expr *StmtLabel);

  // END PROGRAM statement:
  StmtResult ActOnENDPROGRAM(ASTContext &C,
                             const IdentifierInfo *ProgName,
                             SMLoc Loc, SMLoc NameLoc,
                             Expr *StmtLabel);

  // EXTERNAL statement:
  StmtResult ActOnEXTERNAL(ASTContext &C, SMLoc Loc,
                           ArrayRef<const IdentifierInfo *> ExternalNames,
                           Expr *StmtLabel);

  // INTRINSIC statement:
  StmtResult ActOnINTRINSIC(ASTContext &C, SMLoc Loc,
                            ArrayRef<const IdentifierInfo *> IntrinsicNames,
                            Expr *StmtLabel);

  StmtResult ActOnAssignmentStmt(ASTContext &C, llvm::SMLoc Loc,
                                 ExprResult LHS,
                                 ExprResult RHS, Expr *StmtLabel);

  QualType ActOnArraySpec(ASTContext &C, QualType ElemTy,
                          ArrayRef<std::pair<ExprResult,ExprResult> > Dims);

  StarFormatSpec *ActOnStarFormatSpec(ASTContext &C, SMLoc Loc);
  DefaultCharFormatSpec *ActOnDefaultCharFormatSpec(ASTContext &C,
                                                    SMLoc Loc,
                                                    ExprResult Fmt);
  LabelFormatSpec *ActOnLabelFormatSpec(ASTContext &C, SMLoc Loc,
                                        ExprResult Label);

  StmtResult ActOnBlock(ASTContext& C,SMLoc Loc,ArrayRef<StmtResult> Body);

  StmtResult ActOnIfStmt(ASTContext& C, SMLoc Loc,
                         ArrayRef<std::pair<ExprResult,StmtResult> > Branches,
                         Expr* StmtLabel);

  StmtResult ActOnContinueStmt(ASTContext &C, SMLoc Loc, Expr *StmtLabel);

  StmtResult ActOnStopStmt(ASTContext &C, SMLoc Loc, ExprResult StopCode, Expr *StmtLabel);

  StmtResult ActOnPrintStmt(ASTContext &C, SMLoc Loc, FormatSpec *FS,
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

  ExprResult ActOnUnaryExpr(ASTContext &C, llvm::SMLoc Loc,
                            UnaryExpr::Operator Op, ExprResult E);

  ExprResult ActOnBinaryExpr(ASTContext &C, llvm::SMLoc Loc,
                             BinaryExpr::Operator Op,
                             ExprResult LHS,ExprResult RHS);

  ExprResult ActOnSubstringExpr(ASTContext &C, llvm::SMLoc Loc, ExprResult Target,
                                ExprResult StartingPoint, ExprResult EndPoint);
  ExprResult ActOnSubscriptExpr(ASTContext &C, llvm::SMLoc Loc, ExprResult Target,
                                llvm::ArrayRef<ExprResult> Subscripts);

};

} // end flang namespace

#endif
