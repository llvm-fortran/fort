//===--- Sema.cpp - AST Builder and Semantic Analysis Implementation ------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the actions class which performs semantic analysis and
// builds an AST out of a parse stream.
//
//===----------------------------------------------------------------------===//

#include "flang/Sema/Sema.h"
#include "flang/Sema/DeclSpec.h"
#include "flang/Sema/SemaDiagnostic.h"
#include "flang/AST/ASTContext.h"
#include "flang/AST/Decl.h"
#include "flang/AST/Expr.h"
#include "flang/AST/Stmt.h"
#include "flang/Basic/Diagnostic.h"
#include "llvm/Support/raw_ostream.h"
#include <sstream>

namespace flang {

Sema::Sema(ASTContext &ctxt, DiagnosticsEngine &D)
  : Context(ctxt), Diags(D), CurContext(0) {}

Sema::~Sema() {}

// getContainingDC - Determines the context to return to after temporarily
// entering a context.  This depends in an unnecessarily complicated way on the
// exact ordering of callbacks from the parser.
DeclContext *Sema::getContainingDC(DeclContext *DC) {
  return DC->getParent();
}

void Sema::PushDeclContext(DeclContext *DC) {
  assert(getContainingDC(DC) == CurContext &&
      "The next DeclContext should be lexically contained in the current one.");
  CurContext = DC;
}

void Sema::PopDeclContext() {
  assert(CurContext && "DeclContext imbalance!");
  CurContext = getContainingDC(CurContext);
  assert(CurContext && "Popped translation unit!");
}

void Sema::PushExecutableProgramUnit() {
  // Enter new statement label scope
  assert(CurStmtLabelScope.decl_empty());
  assert(CurStmtLabelScope.getForwardDecls().size() == 0);

  // Track the bodies of the executable statements like do and if
  assert(DoStmtList.empty());
}

static bool isValidDoTerminatingStatement(Stmt *S);

void Sema::PopExecutableProgramUnit() {
  // Fix the forward statement label references
  auto StmtLabelForwardDecls = CurStmtLabelScope.getForwardDecls();
  for(size_t I = 0; I < StmtLabelForwardDecls.size(); ++I) {
    if(auto Decl = CurStmtLabelScope.Resolve(StmtLabelForwardDecls[I].StmtLabel))
      StmtLabelForwardDecls[I].ResolveCallback(StmtLabelForwardDecls[I], Decl);
    else {
      std::string Str;
      llvm::raw_string_ostream Stream(Str);
      StmtLabelForwardDecls[I].StmtLabel->print(Stream);
      Diags.Report(StmtLabelForwardDecls[I].StmtLabel->getLocation(),
                   diag::err_undeclared_stmt_label_use)
          << Stream.str();
    }
  }
  // FIXME: TODO warning unused statement labels.
  // Clear the statement labels scope
  CurStmtLabelScope.reset();

  // Resolve the bodies of the do statements
  for(auto I : DoStmtList) {
    if(I->getTerminatingStmt().Statement) {
      // Check the terminating statement constraint
      if(!isValidDoTerminatingStatement(I->getTerminatingStmt().Statement)) {
        Diags.Report(I->getTerminatingStmt().Statement->getLocation(),
                     diag::err_invalid_do_terminating_stmt);
        continue;
      }
      // Create the body of the do statement.

    } // else - error was already reported.
  }
  DoStmtList.clear();
}

void Sema::DeclareStatementLabel(Expr *StmtLabel, Stmt *S) {
  if(auto Decl = getCurrentStmtLabelScope().Resolve(StmtLabel)) {
    std::string Str;
    llvm::raw_string_ostream Stream(Str);
    StmtLabel->print(Stream);
    Diags.Report(StmtLabel->getLocation(),
                       diag::err_redefinition_of_stmt_label)
        << Stream.str();
  }
  else
    getCurrentStmtLabelScope().Declare(StmtLabel, S);
}

void Sema::ActOnTranslationUnit() {
  PushDeclContext(Context.getTranslationUnitDecl());
}

void Sema::ActOnEndProgramUnit() {
  PopDeclContext();
}

void Sema::ActOnMainProgram(const IdentifierInfo *IDInfo, SMLoc NameLoc) {
  DeclarationName DN(IDInfo);
  DeclarationNameInfo NameInfo(DN, NameLoc);
  PushDeclContext(MainProgramDecl::Create(Context,
                                          Context.getTranslationUnitDecl(),
                                          NameInfo));
  PushExecutableProgramUnit();
}

void Sema::ActOnEndMainProgram(const IdentifierInfo *IDInfo, SMLoc NameLoc) {
  assert(CurContext && "DeclContext imbalance!");

  DeclarationName DN(IDInfo);
  DeclarationNameInfo EndNameInfo(DN, NameLoc);

  StringRef ProgName = cast<MainProgramDecl>(CurContext)->getName();
  if (ProgName.empty()) {
    PopDeclContext();
    return;
  }

  const IdentifierInfo *ID = EndNameInfo.getName().getAsIdentifierInfo();
  if (!ID) goto exit;

  if (ProgName != ID->getName())
    Diags.ReportError(EndNameInfo.getLoc(),
                      llvm::Twine("expected label '") +
                      ProgName + "' for END PROGRAM statement");
 exit:
  PopDeclContext();
  PopExecutableProgramUnit();
}

/// \brief Convert the specified DeclSpec to the appropriate type object.
QualType Sema::ActOnTypeName(ASTContext &C, DeclSpec &DS) {
  QualType Result;
  switch (DS.getTypeSpecType()) {
  case DeclSpec::TST_integer:
    Result = C.IntegerTy;
    break;
  case DeclSpec::TST_unspecified: // FIXME: Correct?
  case DeclSpec::TST_real:
    Result = C.RealTy;
    break;
  case DeclSpec::TST_doubleprecision:
    Result = C.DoublePrecisionTy;
    break;
  case DeclSpec::TST_character:
    Result = C.CharacterTy;
    break;
  case DeclSpec::TST_logical:
    Result = C.LogicalTy;
    break;
  case DeclSpec::TST_complex:
    Result = C.ComplexTy;
    break;
  case DeclSpec::TST_struct:
    // FIXME: Finish this.
    break;
  }

  if (!DS.hasAttributes())
    return Result;

  const Type *TypeNode = Result.getTypePtr();
  Qualifiers Quals = Qualifiers::fromOpaqueValue(DS.getAttributeSpecs());
  Quals.setIntentAttr(DS.getIntentSpec());
  Quals.setAccessAttr(DS.getAccessSpec());
  QualType EQs =  C.getExtQualType(TypeNode, Quals, DS.getKindSelector(),
                                   DS.getLengthSelector());
  if (!Quals.hasAttributeSpec(Qualifiers::AS_dimension))
    return EQs;

  return ActOnArraySpec(C, EQs, DS.getDimensions());
}

VarDecl *Sema::ActOnKindSelector(ASTContext &C, SMLoc IDLoc,
                                 const IdentifierInfo *IDInfo) {
  VarDecl *VD = VarDecl::Create(C, CurContext, IDLoc, IDInfo, QualType());
  CurContext->addDecl(VD);

  // Store the Decl in the IdentifierInfo for easy access.
  const_cast<IdentifierInfo*>(IDInfo)->setFETokenInfo(VD);
  return VD;
}

Decl *Sema::ActOnEntityDecl(ASTContext &C, DeclSpec &DS, llvm::SMLoc IDLoc,
                            const IdentifierInfo *IDInfo) {
  if (const VarDecl *Prev = IDInfo->getFETokenInfo<VarDecl>()) {
    if (Prev->getDeclContext() == CurContext) {
      Diags.ReportError(IDLoc,
                        llvm::Twine("variable '") + IDInfo->getName() +
                        "' already declared");
      Diags.getClient()->HandleDiagnostic(DiagnosticsEngine::Note, Prev->getLocation(),
                                          "previous declaration");
      return 0;
    }
  }

  QualType T = ActOnTypeName(C, DS);
  VarDecl *VD = VarDecl::Create(C, CurContext, IDLoc, IDInfo, T);
  CurContext->addDecl(VD);

  // Store the Decl in the IdentifierInfo for easy access.
  const_cast<IdentifierInfo*>(IDInfo)->setFETokenInfo(VD);

  // FIXME: For debugging:
  llvm::outs() << "(declaration\n  '";
  VD->print(llvm::outs());
  llvm::outs() << "')\n";

  return VD;
}

///
Decl *Sema::ActOnImplicitEntityDecl(ASTContext &C, SMLoc IDLoc,
                                    const IdentifierInfo *IDInfo) {
  DeclSpec DS;
  char letter = toupper(IDInfo->getNameStart()[0]);

  // FIXME: This needs to look at the IMPLICIT statements, if any.

  // IMPLICIT statement:
  // `If a mapping is not specified for a letter, the default for a
  //  program unit or an interface body is default integer if the
  //  letter is I, K, ..., or N and default real otherwise`
  if(letter >= 'I' && letter <= 'N')
    DS.SetTypeSpecType(DeclSpec::TST_integer);
  else DS.SetTypeSpecType(DeclSpec::TST_real);

  // FIXME: default for an internal or module procedure is the mapping in
  // the host scoping unit.

  return ActOnEntityDecl(C, DS, IDLoc, IDInfo);
}

StmtResult Sema::ActOnPROGRAM(ASTContext &C, const IdentifierInfo *ProgName,
                              SMLoc Loc, SMLoc NameLoc, Expr *StmtLabel) {
  auto Result = ProgramStmt::Create(C, ProgName, Loc, NameLoc, StmtLabel);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

StmtResult Sema::ActOnUSE(ASTContext &C, UseStmt::ModuleNature MN,
                          const IdentifierInfo *ModName, ExprResult StmtLabel) {
  auto Result = UseStmt::Create(C, MN, ModName, StmtLabel);
  if(StmtLabel.get()) DeclareStatementLabel(StmtLabel.get(), Result);
  return Result;
}

StmtResult Sema::ActOnUSE(ASTContext &C, UseStmt::ModuleNature MN,
                          const IdentifierInfo *ModName, bool OnlyList,
                          ArrayRef<UseStmt::RenamePair> RenameNames,
                          ExprResult StmtLabel) {
  auto Result = UseStmt::Create(C, MN, ModName, OnlyList, RenameNames, StmtLabel);
  if(StmtLabel.get()) DeclareStatementLabel(StmtLabel.get(), Result);
  return Result;
}

StmtResult Sema::ActOnIMPORT(ASTContext &C, SMLoc Loc,
                             ArrayRef<const IdentifierInfo*> ImportNamesList,
                             ExprResult StmtLabel) {
  auto Result = ImportStmt::Create(C, Loc, ImportNamesList, StmtLabel);
  if(StmtLabel.get()) DeclareStatementLabel(StmtLabel.get(), Result);
  return Result;
}

StmtResult Sema::ActOnIMPLICIT(ASTContext &C, SMLoc Loc, DeclSpec &DS,
                               ArrayRef<ImplicitStmt::LetterSpec> LetterSpecs,
                               Expr *StmtLabel) {
  QualType Ty = ActOnTypeName(C, DS);
  auto Result = ImplicitStmt::Create(C, Loc, Ty, LetterSpecs, StmtLabel);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

StmtResult Sema::ActOnIMPLICIT(ASTContext &C, SMLoc Loc, Expr *StmtLabel) {
  // IMPLICIT NONE
  auto Result = ImplicitStmt::Create(C, Loc, StmtLabel);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

ParameterStmt::ParamPair
Sema::ActOnPARAMETERPair(ASTContext &C, SMLoc Loc, const IdentifierInfo *IDInfo,
                         ExprResult CE) {
  if (const VarDecl *Prev = IDInfo->getFETokenInfo<VarDecl>()) {
    Diags.ReportError(Loc,
                      llvm::Twine("variable '") + IDInfo->getName() +
                      "' already defined");
    Diags.getClient()->HandleDiagnostic(DiagnosticsEngine::Note, Prev->getLocation(),
                                        "previous definition");
    return ParameterStmt::ParamPair(0, ExprResult());
  }

  QualType T = CE.get()->getType();
  VarDecl *VD = VarDecl::Create(C, CurContext, Loc, IDInfo, T);
  CurContext->addDecl(VD);

  // Store the Decl in the IdentifierInfo for easy access.
  const_cast<IdentifierInfo*>(IDInfo)->setFETokenInfo(VD);
  return ParameterStmt::ParamPair(IDInfo, CE);
}

StmtResult Sema::ActOnPARAMETER(ASTContext &C, SMLoc Loc,
                                ArrayRef<ParameterStmt::ParamPair> ParamList,
                                Expr *StmtLabel) {
  auto Result = ParameterStmt::Create(C, Loc, ParamList, StmtLabel);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

StmtResult Sema::ActOnASYNCHRONOUS(ASTContext &C, SMLoc Loc,
                                   ArrayRef<const IdentifierInfo *>ObjNames,
                                   Expr *StmtLabel) {
  auto Result = AsynchronousStmt::Create(C, Loc, ObjNames, StmtLabel);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

StmtResult Sema::ActOnDIMENSION(ASTContext &C, SMLoc Loc,
                               const IdentifierInfo *IDInfo,
                               ArrayRef<std::pair<ExprResult,ExprResult> > Dims,
                               Expr *StmtLabel) {
  auto Result = DimensionStmt::Create(C, Loc, IDInfo, Dims, StmtLabel);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

StmtResult Sema::ActOnENDPROGRAM(ASTContext &C,
                                 const IdentifierInfo *ProgName,
                                 llvm::SMLoc Loc, llvm::SMLoc NameLoc,
                                 Expr *StmtLabel) {
  auto Result = EndProgramStmt::Create(C, ProgName, Loc, NameLoc, StmtLabel);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

StmtResult Sema::ActOnEXTERNAL(ASTContext &C, SMLoc Loc,
                               ArrayRef<const IdentifierInfo *> ExternalNames,
                               Expr *StmtLabel) {
  auto Result = ExternalStmt::Create(C, Loc, ExternalNames, StmtLabel);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

StmtResult Sema::ActOnINTRINSIC(ASTContext &C, SMLoc Loc,
                                ArrayRef<const IdentifierInfo *> IntrinsicNames,
                                Expr *StmtLabel) {
  // FIXME: Name Constraints.
  // FIXME: Function declaration.
  auto Result = IntrinsicStmt::Create(C, Loc, IntrinsicNames, StmtLabel);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

StmtResult Sema::ActOnAssignmentStmt(ASTContext &C, llvm::SMLoc Loc,
                                     ExprResult LHS,
                                     ExprResult RHS, Expr *StmtLabel) {
  Stmt *Result = 0;
  const Type *LHSType = LHS.get()->getType().getTypePtr();
  const Type *RHSType = RHS.get()->getType().getTypePtr();

  // Arithmetic assigment
  bool IsRHSInteger = RHSType->isIntegerType();
  bool IsRHSReal = RHSType->isRealType();
  bool IsRHSDblPrec = RHSType->isDoublePrecisionType();
  bool IsRHSComplex = RHSType->isComplexType();
  bool IsRHSArithmetic = IsRHSInteger || IsRHSReal ||
                         IsRHSDblPrec || IsRHSComplex;

  if(LHSType->isIntegerType()) {
    if(IsRHSInteger) ;
    else if(IsRHSArithmetic)
      RHS = ConversionExpr::Create(Context, RHS.get()->getLocation(),
                                   ConversionExpr::INT,RHS);
    else goto typeError;
  } else if(LHSType->isRealType()) {
    if(IsRHSReal) ;
    else if(IsRHSArithmetic)
      RHS = ConversionExpr::Create(Context, RHS.get()->getLocation(),
                                   ConversionExpr::REAL,RHS);
    else goto typeError;
  } else if(LHSType->isDoublePrecisionType()) {
    if(IsRHSDblPrec) ;
    else if(IsRHSArithmetic)
      RHS = ConversionExpr::Create(Context, RHS.get()->getLocation(),
                                   ConversionExpr::DBLE,RHS);
    else goto typeError;
  } else if(LHSType->isComplexType()) {
    if(IsRHSComplex) ;
    else if(IsRHSArithmetic)
      RHS = ConversionExpr::Create(Context, RHS.get()->getLocation(),
                                   ConversionExpr::CMPLX,RHS);
    else goto typeError;
  }

  // Logical assignment
  else if(LHSType->isLogicalType()) {
    if(!RHSType->isLogicalType()) goto typeError;
  }

  // Character assignment
  else if(LHSType->isCharacterType()) {
    if(!RHSType->isCharacterType()) goto typeError;
  }

  // Invalid assignment
  else goto typeError;

  Result = AssignmentStmt::Create(C, LHS, RHS, StmtLabel);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
typeError:
  std::string TypeStrings[2];
  llvm::raw_string_ostream StreamLHS(TypeStrings[0]),
      StreamRHS(TypeStrings[1]);
  LHS.get()->getType().print(StreamLHS);
  RHS.get()->getType().print(StreamRHS);
  Diags.Report(Loc,diag::err_typecheck_assign_incompatible)
      << StreamLHS.str() << StreamRHS.str();
  return StmtError();
}

QualType Sema::ActOnArraySpec(ASTContext &C, QualType ElemTy,
                              ArrayRef<std::pair<ExprResult,ExprResult> > Dims) {
  return QualType(ArrayType::Create(C, ElemTy, Dims), 0);
}

StarFormatSpec *Sema::ActOnStarFormatSpec(ASTContext &C, SMLoc Loc) {
  return StarFormatSpec::Create(C, Loc);
}

DefaultCharFormatSpec *Sema::ActOnDefaultCharFormatSpec(ASTContext &C,
                                                        SMLoc Loc,
                                                        ExprResult Fmt) {
  return DefaultCharFormatSpec::Create(C, Loc, Fmt);
}

LabelFormatSpec *ActOnLabelFormatSpec(ASTContext &C, SMLoc Loc,
                                      ExprResult Label) {
  return LabelFormatSpec::Create(C, Loc, Label);
}

StmtResult Sema::ActOnBlock(ASTContext &C, SMLoc Loc, ArrayRef<StmtResult> Body) {
  return BlockStmt::Create(C, Loc, Body);
}

static void ResolveAssignStmtLabel(const StmtLabelScope::StmtLabelForwardDecl &Self,
                                   Stmt *Destination) {
  assert(AssignStmt::classof(Self.Statement));
  static_cast<AssignStmt*>(Self.Statement)->setAddress(StmtLabelReference(Destination));
}

StmtResult Sema::ActOnAssignStmt(ASTContext &C, SMLoc Loc,
                                 ExprResult Value, VarExpr* VarRef,
                                 Expr *StmtLabel) {
  Stmt *Result;
  auto Decl = getCurrentStmtLabelScope().Resolve(Value.get());
  if(!Decl) {
    Result = AssignStmt::Create(C, Loc, StmtLabelReference(),VarRef, StmtLabel);
    getCurrentStmtLabelScope().DeclareForwardReference(
      StmtLabelScope::StmtLabelForwardDecl(Value.get(), Result,
                                           ResolveAssignStmtLabel));
  } else
    Result = AssignStmt::Create(C, Loc, StmtLabelReference(Decl),
                                VarRef, StmtLabel);

  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

static void ResolveAssignedGotoStmtLabel(const StmtLabelScope::StmtLabelForwardDecl &Self,
                                         Stmt *Destination) {
  assert(AssignedGotoStmt::classof(Self.Statement));
  static_cast<AssignedGotoStmt*>(Self.Statement)->
    setAllowedValue(Self.ResolveCallbackData,StmtLabelReference(Destination));
}

StmtResult Sema::ActOnAssignedGotoStmt(ASTContext &C, SMLoc Loc,
                                       VarExpr* VarRef,
                                       ArrayRef<ExprResult> AllowedValues,
                                       Expr *StmtLabel) {
  SmallVector<StmtLabelReference, 4> AllowedLabels(AllowedValues.size());
  for(size_t I = 0; I < AllowedValues.size(); ++I) {
    auto Decl = getCurrentStmtLabelScope().Resolve(AllowedValues[I].get());
    AllowedLabels[I] = Decl? StmtLabelReference(Decl): StmtLabelReference();
  }
  auto Result = AssignedGotoStmt::Create(C, Loc, VarRef, AllowedLabels, StmtLabel);

  for(size_t I = 0; I < AllowedValues.size(); ++I) {
    if(!AllowedLabels[I].Statement) {
      getCurrentStmtLabelScope().DeclareForwardReference(
        StmtLabelScope::StmtLabelForwardDecl(AllowedValues[I].get(), Result,
                                             ResolveAssignedGotoStmtLabel, I));
    }
  }

  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

static void ResolveGotoStmtLabel(const StmtLabelScope::StmtLabelForwardDecl &Self,
                                 Stmt *Destination) {
  assert(GotoStmt::classof(Self.Statement));
  static_cast<GotoStmt*>(Self.Statement)->setDestination(StmtLabelReference(Destination));
}

StmtResult Sema::ActOnGotoStmt(ASTContext &C, SMLoc Loc,
                               ExprResult Destination, Expr *StmtLabel) {
  Stmt *Result;
  auto Decl = getCurrentStmtLabelScope().Resolve(Destination.get());
  if(!Decl) {
    Result = GotoStmt::Create(C, Loc, StmtLabelReference(), StmtLabel);
    getCurrentStmtLabelScope().DeclareForwardReference(
      StmtLabelScope::StmtLabelForwardDecl(Destination.get(), Result,
                                           ResolveGotoStmtLabel));
  } else
    Result = GotoStmt::Create(C, Loc, StmtLabelReference(Decl), StmtLabel);

  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

static inline bool IsLogicalExpression(ExprResult E) {
  return E.get()->getType()->isLogicalType();
}

static void ReportExpectedLogical(DiagnosticsEngine &Diag, ExprResult E) {
  std::string TypeString;
  llvm::raw_string_ostream Stream(TypeString);
  E.get()->getType().print(Stream);
  Diag.Report(E.get()->getLocation(), diag::err_typecheck_expected_logical_expr)
      << Stream.str();
}

StmtResult Sema::ActOnIfStmt(ASTContext &C, SMLoc Loc,
                       ExprResult Condition, StmtResult Body,
                       Expr *StmtLabel) {
  // FIXME: Constraint: The action-stmt in the if-stmt shall not be an
  // if-stmt, end-program-stmt, end-function-stmt, or end-subroutine-stmt.

  if(!IsLogicalExpression(Condition)) {
    ReportExpectedLogical(Diags, Condition);
    return StmtError();
  }
  auto Result = IfStmt::Create(C, Loc, std::make_pair(Condition, Body), StmtLabel);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

StmtResult Sema::ActOnIfStmt(ASTContext &C, SMLoc Loc,
                       ArrayRef<std::pair<ExprResult,StmtResult> > Branches,
                       Expr *StmtLabel) {
  for(size_t I = 0; I < Branches.size(); ++I) {
    if(!Branches[I].first.get()) {
      assert(I == Branches.size() - 1);
      break;
    }
    if(!IsLogicalExpression(Branches[I].first)) {
      ReportExpectedLogical(Diags, Branches[I].first);
      return StmtError();
    }
  }
  auto Result = IfStmt::Create(C, Loc, Branches, StmtLabel);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

static void ResolveDoStmtLabel(const StmtLabelScope::StmtLabelForwardDecl &Self,
                                 Stmt *Destination) {
  assert(DoStmt::classof(Self.Statement));
  static_cast<DoStmt*>(Self.Statement)->setTerminatingStmt(StmtLabelReference(Destination));
}

static int ExpectRealOrIntegerOrDoublePrec(DiagnosticsEngine &Diags, const Expr *E,
                                           unsigned DiagType = diag::err_typecheck_expected_do_expr) {
  auto T = E->getType();
  if(T->isIntegerType() || T->isRealType() || T->isDoublePrecisionType()) return 0;
  std::string TypeString;
  llvm::raw_string_ostream Stream(TypeString);
  E->getType().print(Stream);
  Diags.Report(E->getLocation(),DiagType)
    << Stream.str();
  return 1;
}

/// The terminal statement of a DO-loop must not be an unconditional GO TO,
/// assigned GO TO, arithmetic IF, block IF, ELSE IF, ELSE, END IF, RETURN, STOP, END, or DO statement.
/// If the terminal statement of a DO-loop is a logical IF statement,
/// it may contain any executable statement except a DO,
/// block IF, ELSE IF, ELSE, END IF, END, or another logical IF statement.
///
/// FIXME: TODO full
static bool isValidDoTerminatingStatement(Stmt *S) {
  switch(S->getStatementID()) {
  case Stmt::Goto: case Stmt::AssignedGoto:
  case Stmt::Stop: case Stmt::Do:
    return false;
  default:
    return true;
  }
}

/// FIXME: TODO DO body.
StmtResult Sema::ActOnDoStmt(ASTContext &C, SMLoc Loc, ExprResult TerminatingStmt,
                             VarExpr *DoVar, ExprResult E1, ExprResult E2,
                             ExprResult E3, Expr *StmtLabel) {
  // typecheck
  auto HasErrors = 0;
  HasErrors |= ExpectRealOrIntegerOrDoublePrec(Diags, DoVar, diag::err_typecheck_expected_do_var);
  HasErrors |= ExpectRealOrIntegerOrDoublePrec(Diags, E1.get());
  HasErrors |= ExpectRealOrIntegerOrDoublePrec(Diags, E2.get());
  if(E3.isUsable())
    HasErrors |= ExpectRealOrIntegerOrDoublePrec(Diags, E3.get());
  if(HasErrors) return StmtError();

  // Make sure the statement label isn't already declared
  if(auto Decl = getCurrentStmtLabelScope().Resolve(TerminatingStmt.get())) {
    std::string String;
    llvm::raw_string_ostream Stream(String);
    TerminatingStmt.get()->print(Stream);
    Diags.Report(TerminatingStmt.get()->getLocation(),
                 diag::err_stmt_label_must_decl_after)
        << Stream.str() << "DO";
    return StmtError();
  }
  auto Result = DoStmt::Create(C, Loc, StmtLabelReference(),
                               DoVar, E1, E2, E3, StmtLabel);
  getCurrentStmtLabelScope().DeclareForwardReference(
    StmtLabelScope::StmtLabelForwardDecl(TerminatingStmt.get(), Result,
                                         ResolveDoStmtLabel));

  DoStmtList.append(1, Result);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

StmtResult Sema::ActOnContinueStmt(ASTContext &C, SMLoc Loc, Expr *StmtLabel) {
  auto Result = ContinueStmt::Create(C, Loc, StmtLabel);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

StmtResult Sema::ActOnStopStmt(ASTContext &C, SMLoc Loc, ExprResult StopCode, Expr *StmtLabel) {
  auto Result = StopStmt::Create(C, Loc, StopCode, StmtLabel);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

StmtResult Sema::ActOnPrintStmt(ASTContext &C, SMLoc Loc, FormatSpec *FS,
                                ArrayRef<ExprResult> OutputItemList,
                                Expr *StmtLabel) {
  auto Result = PrintStmt::Create(C, Loc, FS, OutputItemList, StmtLabel);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

RecordDecl *Sema::ActOnDerivedTypeDecl(ASTContext &C, SMLoc Loc,
                                          SMLoc NameLoc,
                                          const IdentifierInfo* IDInfo) {
  RecordDecl* Record = RecordDecl::Create(C, CurContext, Loc, NameLoc, IDInfo);
  CurContext->addDecl(Record);
  PushDeclContext(Record);
  return Record;
}

FieldDecl *Sema::ActOnDerivedTypeFieldDecl(ASTContext &C, DeclSpec &DS, SMLoc IDLoc,
                                     const IdentifierInfo *IDInfo,
                                     ExprResult Init) {
  //FIXME: TODO same field name check
  //FIXME: TODO init expression

  QualType T = ActOnTypeName(C, DS);
  FieldDecl* Field = FieldDecl::Create(C, CurContext, IDLoc, IDInfo, T);
  CurContext->addDecl(Field);

  return Field;
}

void Sema::ActOnEndDerivedTypeDecl() {
  PopDeclContext();
}

} //namespace flang
