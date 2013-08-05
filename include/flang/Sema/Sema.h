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
#include "flang/AST/IOSpec.h"
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

/// Sema - This implements semantic analysis and AST buiding for Fortran.
class Sema {
  Sema(const Sema&);           // DO NOT IMPLEMENT
  void operator=(const Sema&); // DO NOT IMPLEMENT


  /// \brief A statement label scope for the current program unit.
  StmtLabelScope *CurStmtLabelScope;

  /// \brief A named constructs scope for the current program unit.
  ConstructNameScope *CurNamedConstructs;

  /// \brief A class which supports the executable statements in
  /// the current scope.
  BlockStmtBuilder *CurExecutableStmts;

  /// \brief The implicit scope for the current program unit.
  ImplicitTypingScope *CurImplicitTypingScope;

  /// \brief Represents the do loop variable currently being used.
  SmallVector<const VarExpr*, 8> CurLoopVars;

  /// \brief Marks the variable as used by a loop.
  void AddLoopVar(const VarExpr *Var) {
    CurLoopVars.push_back(Var);
  }

  /// \brief Clears the variable of a used by a loop mark.
  void RemoveLoopVar(const VarExpr *Var) {
    for(auto I = CurLoopVars.begin();I!=CurLoopVars.end();++I) {
      if(*I == Var) {
        CurLoopVars.erase(I);
        return;
      }
    }
  }

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

  ASTContext &getContext() { return Context; }

  LangOptions getLangOpts() const {
    return Context.getLangOpts();
  }

  DeclContext *getContainingDC(DeclContext *DC);

  StmtLabelScope *getCurrentStmtLabelScope() const {
    return CurStmtLabelScope;
  }

  ConstructNameScope *getCurrentConstructNameScope() const {
    return CurNamedConstructs;
  }

  ImplicitTypingScope *getCurrentImplicitTypingScope() const {
    return CurImplicitTypingScope;
  }

  BlockStmtBuilder *getCurrentBody() const {
    return CurExecutableStmts;
  }

  SourceRange getIdentifierRange(SourceLocation Loc, const IdentifierInfo *IDInfo);

  inline ExprResult ExprError() const { return ExprResult(true); }
  inline StmtResult StmtError() const { return StmtResult(true); }

  /// Set the current declaration context until it gets popped.
  void PushDeclContext(DeclContext *DC);
  void PopDeclContext();

  bool IsInsideFunctionOrSubroutine() const;
  FunctionDecl *CurrentContextAsFunction() const;

  void PushExecutableProgramUnit(ExecutableProgramUnitScope &Scope);
  void PopExecutableProgramUnit(SourceLocation Loc);

  void PushProgramUnitScope(ExecutableProgramUnitScope &Scope);
  void PopExecutableProgramUnitScope(SourceLocation Loc);

  void DeclareStatementLabel(Expr *StmtLabel, Stmt *S);
  void CheckStatementLabelEndDo(Expr *StmtLabel, Stmt *S);

  void DeclareConstructName(ConstructName Name, NamedConstructStmt *S);

  /// translation unit actions
  void ActOnTranslationUnit(TranslationUnitScope &Scope);
  void ActOnEndTranslationUnit();

  /// program unit actions
  MainProgramDecl *ActOnMainProgram(ASTContext &C, MainProgramScope &Scope,
                                    const IdentifierInfo *IDInfo, SourceLocation NameLoc);
  void ActOnEndMainProgram(SourceLocation Loc, const IdentifierInfo *IDInfo,
                           SourceLocation NameLoc);

  FunctionDecl *ActOnSubProgram(ASTContext &C, SubProgramScope &Scope,
                                bool IsSubRoutine, SourceLocation IDLoc,
                                const IdentifierInfo *IDInfo, DeclSpec &ReturnTypeDecl);
  VarDecl *ActOnSubProgramArgument(ASTContext &C, SourceLocation IDLoc,
                                   const IdentifierInfo *IDInfo);
  void ActOnSubProgramStarArgument(ASTContext &C, SourceLocation Loc);
  void ActOnSubProgramArgumentList(ASTContext &C, ArrayRef<VarDecl*> Arguments);
  void ActOnEndSubProgram(ASTContext &C, SourceLocation Loc);

  FunctionDecl *ActOnStatementFunction(ASTContext &C,
                                       SourceLocation IDLoc,
                                       const IdentifierInfo *IDInfo);
  VarDecl *ActOnStatementFunctionArgument(ASTContext &C, SourceLocation IDLoc,
                                          const IdentifierInfo *IDInfo);
  void ActOnStatementFunctionBody(SourceLocation Loc, ExprResult Body);
  void ActOnEndStatementFunction(ASTContext &C);

  void ActOnSpecificationPart();

  void ActOnFunctionSpecificationPart();

  VarDecl *GetVariableForSpecification(SourceLocation StmtLoc, const IdentifierInfo *IDInfo,
                                       SourceLocation IDLoc,
                                       bool CanBeArgument = true);
  bool ApplySpecification(SourceLocation StmtLoc, const DimensionStmt *Stmt);
  bool ApplySpecification(SourceLocation StmtLoc, const SaveStmt *S);
  bool ApplySpecification(SourceLocation StmtLoc, const SaveStmt *S, VarDecl *VD);

  QualType ActOnTypeName(ASTContext &C, DeclSpec &DS);
  VarDecl *ActOnKindSelector(ASTContext &C, SourceLocation IDLoc,
                             const IdentifierInfo *IDInfo);

  Decl *ActOnEntityDecl(ASTContext &C, const QualType &T, SourceLocation IDLoc,
                        const IdentifierInfo *IDInfo);

  Decl *ActOnEntityDecl(ASTContext &C, DeclSpec &DS, SourceLocation IDLoc,
                        const IdentifierInfo *IDInfo);

  QualType ResolveImplicitType(const IdentifierInfo *IDInfo);

  Decl *ActOnImplicitEntityDecl(ASTContext &C, SourceLocation IDLoc,
                                const IdentifierInfo *IDInfo);

  Decl *ActOnImplicitFunctionDecl(ASTContext &C, SourceLocation IDLoc,
                                  const IdentifierInfo *IDInfo);

  Decl *ActOnPossibleImplicitFunctionDecl(ASTContext &C, SourceLocation IDLoc,
                                          const IdentifierInfo *IDInfo,
                                          Decl *PrevDecl);

  bool ApplyImplicitRulesToArgument(VarDecl *Arg,
                                    SourceRange Range = SourceRange());

  /// Returns a declaration which matches the identifier in this context
  Decl *LookupIdentifier(const IdentifierInfo *IDInfo);

  Decl *ResolveIdentifier(const IdentifierInfo *IDInfo);

  /// \brief Returns a variable declaration if the given identifier resolves
  /// to a variable, or null otherwise. If the identifier isn't resolved
  /// an implicit variable declaration will be created whenever possible.
  VarDecl *ExpectVarRefOrDeclImplicitVar(SourceLocation IDLoc,
                                         const IdentifierInfo *IDInfo);

  /// \brief Returns a variable declaration if the given identifier resolves to
  /// a variable, or null otherwise.
  VarDecl *ExpectVarRef(SourceLocation IDLoc,
                        const IdentifierInfo *IDInfo);

  VarExpr *ConstructRecoveryVariable(ASTContext &C, SourceLocation Loc,
                                     QualType T);

  /// \brief Returns true if the given identifier can be used as the function name
  /// in a statement function declaration. This function resolves the ambiguity
  /// of statement function declarations and array subscript assignments.
  bool IsValidStatementFunctionIdentifier(const IdentifierInfo *IDInfo);

  // FIXME: TODO more features.
  RecordDecl *ActOnDerivedTypeDecl(ASTContext &C, SourceLocation Loc,
                                   SourceLocation NameLoc, const IdentifierInfo* IDInfo);

  FieldDecl *ActOnDerivedTypeFieldDecl(ASTContext &C, DeclSpec &DS, SourceLocation IDLoc,
                                       const IdentifierInfo *IDInfo,
                                       ExprResult Init = ExprResult());

  void ActOnEndDerivedTypeDecl();

  StmtResult ActOnCompoundStmt(ASTContext &C, SourceLocation Loc,
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
  StmtResult ActOnDIMENSION(ASTContext &C, SourceLocation Loc, SourceLocation IDLoc,
                            const IdentifierInfo *IDInfo,
                            ArrayRef<ArraySpec*> Dims,
                            Expr *StmtLabel);

  // PARAMETER statement:
  StmtResult ActOnPARAMETER(ASTContext &C, SourceLocation Loc,
                            SourceLocation EqualLoc,
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
                           SourceLocation IDLoc,
                           const IdentifierInfo *IDInfo,
                           Expr *StmtLabel);

  // INTRINSIC statement:
  StmtResult ActOnINTRINSIC(ASTContext &C, SourceLocation Loc,
                            SourceLocation IDLoc,
                            const IdentifierInfo *IDInfo,
                            Expr *StmtLabel);

  // SAVE statement
  StmtResult ActOnSAVE(ASTContext &C, SourceLocation Loc, Expr *StmtLabel);

  StmtResult ActOnSAVE(ASTContext &C, SourceLocation Loc,
                       SourceLocation IDLoc,
                       const IdentifierInfo *IDInfo,
                       Expr *StmtLabel);

  // EQUIVALENCE statement
  StmtResult ActOnEQUIVALENCE(ASTContext &C, SourceLocation Loc,
                              SourceLocation PartLoc,
                              ArrayRef<Expr*> ObjectList,
                              Expr *StmtLabel);

  // DATA statement:
  StmtResult ActOnDATA(ASTContext &C, SourceLocation Loc,
                       ArrayRef<ExprResult> LHS,
                       ArrayRef<ExprResult> RHS,
                       Expr *StmtLabel);

  ExprResult ActOnDATAConstantExpr(ASTContext &C, SourceLocation RepeatLoc,
                                   ExprResult RepeatCount,
                                   ExprResult Value);

  ExprResult ActOnDATAOuterImpliedDoExpr(ASTContext &C,
                                         ExprResult Expression);

  ExprResult ActOnDATAImpliedDoExpr(ASTContext &C, SourceLocation Loc,
                                    SourceLocation IDLoc,
                                    const IdentifierInfo *IDInfo,
                                    ArrayRef<ExprResult> Body,
                                    ExprResult E1, ExprResult E2,
                                    ExprResult E3);

  StmtResult ActOnAssignmentStmt(ASTContext &C, SourceLocation Loc,
                                 ExprResult LHS,
                                 ExprResult RHS, Expr *StmtLabel);

  QualType ActOnArraySpec(ASTContext &C, QualType ElemTy,
                          ArrayRef<ArraySpec *> Dims);

  StarFormatSpec *ActOnStarFormatSpec(ASTContext &C, SourceLocation Loc);
  LabelFormatSpec *ActOnLabelFormatSpec(ASTContext &C, SourceLocation Loc,
                                        ExprResult Label);
  FormatSpec *ActOnExpressionFormatSpec(ASTContext &C, SourceLocation Loc,
                                             Expr *E);

  ExternalStarUnitSpec *ActOnStarUnitSpec(ASTContext &C, SourceLocation Loc,
                                          bool IsLabeled);
  UnitSpec *ActOnUnitSpec(ASTContext &C, ExprResult Value, SourceLocation Loc,
                          bool IsLabeled);

  StmtResult ActOnAssignStmt(ASTContext &C, SourceLocation Loc,
                             ExprResult Value, VarExpr* VarRef,
                             Expr *StmtLabel);

  StmtResult ActOnAssignedGotoStmt(ASTContext &C, SourceLocation Loc,
                                   VarExpr* VarRef, ArrayRef<Expr *> AllowedValues,
                                   Expr *StmtLabel);

  StmtResult ActOnGotoStmt(ASTContext &C, SourceLocation Loc,
                           ExprResult Destination, Expr *StmtLabel);

  StmtResult ActOnComputedGotoStmt(ASTContext &C, SourceLocation Loc,
                                   ArrayRef<Expr*> Targets,
                                   ExprResult Operand, Expr *StmtLabel);

  StmtResult ActOnIfStmt(ASTContext &C, SourceLocation Loc,
                         ExprResult Condition, ConstructName Name,
                         Expr *StmtLabel);
  StmtResult ActOnElseIfStmt(ASTContext &C, SourceLocation Loc,
                             ExprResult Condition, ConstructName Name, Expr *StmtLabel);
  StmtResult ActOnElseStmt(ASTContext &C, SourceLocation Loc,
                           ConstructName Name, Expr *StmtLabel);
  StmtResult ActOnEndIfStmt(ASTContext &C, SourceLocation Loc,
                            ConstructName Name, Expr *StmtLabel);

  StmtResult ActOnDoStmt(ASTContext &C, SourceLocation Loc, SourceLocation EqualLoc,
                         ExprResult TerminatingStmt,
                         VarExpr *DoVar, ExprResult E1, ExprResult E2,
                         ExprResult E3, ConstructName Name,
                         Expr *StmtLabel);

  StmtResult ActOnDoWhileStmt(ASTContext &C, SourceLocation Loc, ExprResult Condition,
                              ConstructName Name, Expr *StmtLabel);

  StmtResult ActOnEndDoStmt(ASTContext &C, SourceLocation Loc,
                            ConstructName Name, Expr *StmtLabel);

  StmtResult ActOnCycleStmt(ASTContext &C, SourceLocation Loc,
                            ConstructName LoopName, Expr *StmtLabel);

  StmtResult ActOnExitStmt(ASTContext &C, SourceLocation Loc,
                           ConstructName LoopName, Expr *StmtLabel);

  StmtResult ActOnContinueStmt(ASTContext &C, SourceLocation Loc, Expr *StmtLabel);

  StmtResult ActOnStopStmt(ASTContext &C, SourceLocation Loc, ExprResult StopCode, Expr *StmtLabel);

  StmtResult ActOnReturnStmt(ASTContext &C, SourceLocation Loc, ExprResult E, Expr *StmtLabel);

  StmtResult ActOnCallStmt(ASTContext &C, SourceLocation Loc, SourceLocation RParenLoc,
                           SourceRange IdRange,
                           const IdentifierInfo *IDInfo,
                           ArrayRef<Expr*> Arguments, Expr *StmtLabel);

  StmtResult ActOnPrintStmt(ASTContext &C, SourceLocation Loc, FormatSpec *FS,
                            ArrayRef<ExprResult> OutputItemList,
                            Expr *StmtLabel);

  StmtResult ActOnWriteStmt(ASTContext &C, SourceLocation Loc,
                            UnitSpec *US, FormatSpec *FS,
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

  ExprResult ActOnComplexConstantExpr(ASTContext &C, SourceLocation Loc,
                                      SourceLocation MaxLoc,
                                      ExprResult RealPart, ExprResult ImPart);

  ExprResult ActOnUnaryExpr(ASTContext &C, SourceLocation Loc,
                            UnaryExpr::Operator Op, ExprResult E);

  ExprResult ActOnBinaryExpr(ASTContext &C, SourceLocation Loc,
                             BinaryExpr::Operator Op,
                             ExprResult LHS,ExprResult RHS);

  ExprResult ActOnSubstringExpr(ASTContext &C, SourceLocation Loc,
                                Expr *Target,
                                Expr *StartingPoint, Expr *EndPoint);

  ExprResult ActOnSubscriptExpr(ASTContext &C, SourceLocation Loc, SourceLocation RParenLoc,
                                Expr* Target, llvm::ArrayRef<Expr*> Subscripts);

  ExprResult ActOnCallExpr(ASTContext &C, SourceLocation Loc, SourceLocation RParenLoc,
                           SourceRange IdRange,
                           FunctionDecl *Function, ArrayRef<Expr*> Arguments);

  ExprResult ActOnIntrinsicFunctionCallExpr(ASTContext &C, SourceLocation Loc,
                                            const IntrinsicFunctionDecl *FunctionDecl,
                                            ArrayRef<Expr*> Arguments);

  // Format
  StmtResult ActOnFORMAT(ASTContext &C, SourceLocation Loc,
                         FormatItemResult Items,
                         FormatItemResult UnlimitedItems,
                         Expr *StmtLabel, bool IsInline = false);

  FormatItemResult ActOnFORMATIntegerDataEditDesc(ASTContext &C, SourceLocation Loc,
                                                  tok::TokenKind Kind,
                                                  IntegerConstantExpr *RepeatCount,
                                                  IntegerConstantExpr *W,
                                                  IntegerConstantExpr *M);

  FormatItemResult ActOnFORMATRealDataEditDesc(ASTContext &C, SourceLocation Loc,
                                               tok::TokenKind Kind,
                                               IntegerConstantExpr *RepeatCount,
                                               IntegerConstantExpr *W,
                                               IntegerConstantExpr *D,
                                               IntegerConstantExpr *E);

  FormatItemResult ActOnFORMATLogicalDataEditDesc(ASTContext &C, SourceLocation Loc,
                                                  tok::TokenKind Kind,
                                                  IntegerConstantExpr *RepeatCount,
                                                  IntegerConstantExpr *W);

  FormatItemResult ActOnFORMATCharacterDataEditDesc(ASTContext &C, SourceLocation Loc,
                                                    tok::TokenKind Kind,
                                                    IntegerConstantExpr *RepeatCount,
                                                    IntegerConstantExpr *W);

  FormatItemResult ActOnFORMATPositionEditDesc(ASTContext &C, SourceLocation Loc,
                                               tok::TokenKind Kind,
                                               IntegerConstantExpr *N);

  FormatItemResult ActOnFORMATControlEditDesc(ASTContext &C, SourceLocation Loc,
                                              tok::TokenKind Kind);

  FormatItemResult ActOnFORMATCharacterStringDesc(ASTContext &C, SourceLocation Loc,
                                                  ExprResult E);

  FormatItemResult ActOnFORMATFormatItemList(ASTContext &C, SourceLocation Loc,
                                             IntegerConstantExpr *RepeatCount,
                                             ArrayRef<FormatItem*> Items);


  /// Returns evaluated integer,
  /// or an ErrorValue if the expression couldn't
  /// be evaluated.
  int64_t EvalAndCheckIntExpr(const Expr *E,
                              int64_t ErrorValue);

  /// Checks if an evaluated integer greater than 0.
  /// Returns EvalResult if EvalResult > 0, or the error
  /// value if EvalResult <= 0
  int64_t CheckIntGT0(const Expr *E, int64_t EvalResult, int64_t ErrorValue = 1);

  /// Returns evaluated kind specification for the builtin types.
  BuiltinType::TypeKind EvalAndCheckTypeKind(QualType T,
                                             const Expr *E);

  /// Returns evaluated length specification
  /// fot the character type.
  unsigned EvalAndCheckCharacterLength(const Expr *E);

  /// Returns true if an expression is constant(i.e. evaluatable)
  bool CheckConstantExpression(const Expr *E);

  /// Returns true if an expression is an integer expression
  bool CheckIntegerExpression(const Expr *E);

  /// Returns true if an expression is an integer expression
  bool StmtRequiresIntegerExpression(SourceLocation Loc, const Expr *E);

  /// Returns true if an expression is a scalar numeric expression
  bool CheckScalarNumericExpression(const Expr *E);

  /// Returns true if a variable reference points to an integer
  /// variable
  bool StmtRequiresIntegerVar(SourceLocation Loc, const VarExpr *E);

  /// Returns true if a variable reference points to an integer
  /// or a real variable
  bool StmtRequiresScalarNumericVar(SourceLocation Loc, const VarExpr *E, unsigned DiagId);

  /// Returns true if an expression is a logical expression
  bool StmtRequiresLogicalExpression(SourceLocation Loc, const Expr *E);

  /// Returns true if an expression is a character expression
  bool CheckCharacterExpression(const Expr *E);

  /// Returns true if two types have the same type class
  /// and kind.
  bool CheckTypesSameKind(QualType A, QualType B) const;

  /// Returns true if the given Type is a scalar(integer,
  /// real, complex) or character
  bool CheckTypeScalarOrCharacter(const Expr *E, QualType T,
                                  bool IsConstant = false);

  /// Checks that all of the expressions have the same type
  /// class and kind.
  void CheckExpressionListSameTypeKind(ArrayRef<Expr*> Expressions);

  /// Returns true if the argument count doesn't match to the function
  /// count
  bool CheckIntrinsicCallArgumentCount(intrinsic::FunctionKind Function,
                                       ArrayRef<Expr*> Args, SourceLocation Loc);

  /// Returns false if the call to a function from a conversion group
  /// is valid.
  bool CheckIntrinsicConversionFunc(intrinsic::FunctionKind Function,
                                    ArrayRef<Expr*> Args,
                                    QualType &ReturnType);

  /// Returns false if the call to a function from the truncation group
  /// is valid.
  bool CheckIntrinsicTruncationFunc(intrinsic::FunctionKind Function,
                                    ArrayRef<Expr*> Args,
                                    QualType &ReturnType);

  /// Returns false if the call to a function from the complex group
  /// is valid.
  bool CheckIntrinsicComplexFunc(intrinsic::FunctionKind Function,
                                 ArrayRef<Expr*> Args,
                                 QualType &ReturnType);

  /// Returns false if the call to a function from the maths group
  /// is valid.
  bool CheckIntrinsicMathsFunc(intrinsic::FunctionKind Function,
                               ArrayRef<Expr*> Args,
                               QualType &ReturnType);

  /// Returns false if the call to a function from the character group
  /// is valid.
  bool CheckIntrinsicCharacterFunc(intrinsic::FunctionKind Function,
                                   ArrayRef<Expr*> Args,
                                   QualType &ReturnType);

  /// Returns false if the argument's type is integer.
  bool CheckIntegerArgument(const Expr *E);

  /// Returns false if the argument's type is real.
  bool CheckRealArgument(const Expr *E);

  /// Returns false if the argument's type is complex.
  bool CheckComplexArgument(const Expr *E);

  /// Returns false if the argument's type is real but isn't double precision.
  bool CheckStrictlyRealArgument(const Expr *E);

  /// Returns false if the argument's type is real and is double precision.
  bool CheckDoublePrecisionRealArgument(const Expr *E);

  /// Returns false if the argument's type is complex and is double complex.
  bool CheckDoubleComplexArgument(const Expr *E);

  /// Returns false if the argument's type is character.
  bool CheckCharacterArgument(const Expr *E);

  /// Returns false if the argument has an integer or a real type.
  bool CheckIntegerOrRealArgument(const Expr *E);

  /// Returns false if the argument has an integer or a real or
  /// a complex argument.
  bool CheckIntegerOrRealOrComplexArgument(const Expr *E);

  /// Returns false if the argument has a real or
  /// a complex argument.
  bool CheckRealOrComplexArgument(const Expr *E);

  bool IsValidFunctionType(QualType Type);

  /// Sets a type for a function
  void SetFunctionType(FunctionDecl *Function, QualType Type,
                       SourceLocation DiagLoc, SourceRange DiagRange);

  /// Returns true if the call expression has the right amount of arguments
  bool CheckCallArgumentCount(FunctionDecl *Function, ArrayRef<Expr*> Arguments,
                              SourceLocation Loc, SourceRange FuncNameRange);

  /// Returns true if the array shape bound is valid
  bool CheckArrayBoundValue(Expr *E);

  /// Returns true if the given array type can be applied to a declaration.
  bool CheckArrayTypeDeclarationCompability(const ArrayType *T, VarDecl *VD);

  /// Returns true if the given character length can be applied to a declaration.
  bool CheckCharacterLengthDeclarationCompability(QualType T, VarDecl *VD);

  /// Performs assignment typechecking.
  ExprResult TypecheckAssignment(QualType LHSTypeof, ExprResult RHS,
                                 SourceLocation Loc = SourceLocation(),
                                 SourceLocation MinLoc = SourceLocation(),
                                 SourceRange ExtraRange = SourceRange());

  /// Returns true if the subscript expression has the
  /// right amount of dimensions.
  bool CheckSubscriptExprDimensionCount(SourceLocation Loc, SourceLocation RParenLoc,
                                        Expr *Target,
                                        ArrayRef<Expr *> Arguments);

  /// Returns true if the items in the array constructor
  /// satisfy all the constraints.
  /// As a bonus it also returns the Element type in ObtainedElementType.
  bool CheckArrayConstructorItems(ArrayRef<Expr*> Items,
                                  QualType &ObtainedElementType);

  /// Returns true if the variable can be assigned to (mutated)
  bool CheckVarIsAssignable(const VarExpr *E);

  /// Returns true if a statement is a valid do terminator
  bool IsValidDoTerminatingStatement(const Stmt *S);

  /// Reports an unterminated construct such as do, if, etc.
  void ReportUnterminatedStmt(const BlockStmtBuilder::Entry &S,
                              SourceLocation Loc,
                              bool ReportUnterminatedLabeledDo = true);

  /// Leaves the last block construct, and performs any clean up
  /// that might be needed.
  void LeaveLastBlock();

  /// Leaves block constructs until a do construct is reached.
  /// NB: the do statements with a termination label such as DO 100 I = ..
  /// are popped.
  Stmt *LeaveBlocksUntilDo(SourceLocation Loc);

  /// Returns true if the current statement is inside a do construct which
  /// is terminated by the given statement label.
  bool IsInLabeledDo(const Expr *StmtLabel);

  /// Leaves block constructs until a label termination do construct is reached.
  DoStmt *LeaveBlocksUntilLabeledDo(SourceLocation Loc, const Expr *StmtLabel);

  /// Leaves block constructs until an if construct is reached.
  IfStmt *LeaveBlocksUntilIf(SourceLocation Loc);

  /// Checks to see if the part of the constructs has a valid construct name.
  void CheckConstructNameMatch(Stmt *Part, ConstructName Name, Stmt *S);

  /// Checks to see if a statement is inside an outer loop or a loop
  /// associated with a given name, and returns this loop. Null
  /// is returned when an error occurs.
  Stmt *CheckWithinLoopRange(const char *StmtString, SourceLocation Loc, ConstructName Name);

};

} // end flang namespace

#endif
