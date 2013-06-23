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
#include "flang/Parse/ParseDiagnostic.h"
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

  assert(!CurExecutableStmts.StmtList.size());
  assert(!CurExecutableStmts.ControlFlowStack.size());
}

static bool IsValidDoTerminatingStatement(Stmt *S);

// Unterminated labeled do statement
static void ReportUnterminatedLabeledDoStmt(DiagnosticsEngine &Diags,
                                            const ExecutableProgramUnitStmts::ControlFlowStmt &S,
                                            SourceLocation Loc) {
  std::string Str;
  llvm::raw_string_ostream Stream(Str);
  S.ExpectedEndDoLabel->print(Stream);
  Diags.Report(Loc, diag::err_expected_stmt_label_end_do) << Stream.str();
}

// Unterminated if/do statement
static void ReportUnterminatedStmt(DiagnosticsEngine &Diags,
                                   const ExecutableProgramUnitStmts::ControlFlowStmt &S,
                                   SourceLocation Loc,
                                   bool ReportUnterminatedLabeledDo = true) {
  const char * Keyword;
  switch(S.Statement->getStatementID()) {
  case Stmt::If: Keyword = "END IF"; break;
  case Stmt::Do: {
    if(S.ExpectedEndDoLabel) {
      if(ReportUnterminatedLabeledDo)
        ReportUnterminatedLabeledDoStmt(Diags, S, Loc);
      return;
    }
    else Keyword = "END DO";
    break;
  }
  default:
    llvm_unreachable("Invalid stmt");
  }
  Diags.Report(Loc, diag::err_expected_kw) << Keyword;
}

void Sema::PopExecutableProgramUnit(SourceLocation Loc) {
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
          << Stream.str()
          << StmtLabelForwardDecls[I].StmtLabel->getSourceRange();
    }
  }
  // FIXME: TODO warning unused statement labels.
  // Clear the statement labels scope
  CurStmtLabelScope.reset();

  // Report unterminated statements.
  if(CurExecutableStmts.HasEntered()) {
    // If do ends with statement label, the error
    // was already reported as undeclared label use.
    ReportUnterminatedStmt(Diags, CurExecutableStmts.LastEntered(), Loc, false);
  }
  CurExecutableStmts.Reset();
}

void ExecutableProgramUnitStmts::Reset() {
  ControlFlowStack.clear();
  StmtList.clear();
}

void ExecutableProgramUnitStmts::Enter(ControlFlowStmt S) {
  S.BeginOffset = StmtList.size();
  ControlFlowStack.push_back(S);
}

Stmt *ExecutableProgramUnitStmts::CreateBody(ASTContext &C,
                                             const ControlFlowStmt &Last) {
  auto Ref = ArrayRef<StmtResult>(StmtList);
  return BlockStmt::Create(C, Last.Statement->getLocation(),
                           ArrayRef<StmtResult>(Ref.begin() + Last.BeginOffset,
                                                Ref.end()));
}

void ExecutableProgramUnitStmts::LeaveIfThen(ASTContext &C) {
  auto Last = ControlFlowStack.back();
  assert(IfStmt::classof(Last));

  auto Body = CreateBody(C, Last);
  static_cast<IfStmt*>(Last.Statement)->setThenStmt(Body);
  StmtList.erase(StmtList.begin() + Last.BeginOffset, StmtList.end());
}

void ExecutableProgramUnitStmts::Leave(ASTContext &C) {
  assert(ControlFlowStack.size());
  auto Last = ControlFlowStack.pop_back_val();

  /// Create the body
  auto Body = CreateBody(C, Last);
  if(auto Parent = dyn_cast<IfStmt>(Last.Statement)) {
    if(Parent->getThenStmt())
      Parent->setElseStmt(Body);
    else Parent->setThenStmt(Body);
  } else
    static_cast<CFBlockStmt*>(Last.Statement)->setBody(Body);
  StmtList.erase(StmtList.begin() + Last.BeginOffset, StmtList.end());
}

bool ExecutableProgramUnitStmts::HasEntered(Stmt::StmtTy StmtType) const {
  for(auto I : ControlFlowStack) {
    if(I.is(StmtType)) return true;
  }
  return false;
}

void ExecutableProgramUnitStmts::Append(Stmt *S) {
  assert(S);
  StmtList.push_back(StmtResult(S));
}

void Sema::DeclareStatementLabel(Expr *StmtLabel, Stmt *S) {
  if(auto Decl = getCurrentStmtLabelScope().Resolve(StmtLabel)) {
    std::string Str;
    llvm::raw_string_ostream Stream(Str);
    StmtLabel->print(Stream);
    Diags.Report(StmtLabel->getLocation(),
                 diag::err_redefinition_of_stmt_label)
        << Stream.str() << StmtLabel->getSourceRange();
    Diags.Report(Decl->getStmtLabel()->getLocation(),
                 diag::note_previous_definition)
        << Decl->getStmtLabel()->getSourceRange();
  }
  else {
    getCurrentStmtLabelScope().Declare(StmtLabel, S);
    /// Check to see if it matches the last do stmt.
    CheckStatementLabelEndDo(StmtLabel, S);
  }
}

void Sema::ActOnTranslationUnit() {
  PushDeclContext(Context.getTranslationUnitDecl());
}

void Sema::ActOnEndProgramUnit() {
  PopDeclContext();
}

void Sema::ActOnMainProgram(const IdentifierInfo *IDInfo, SourceLocation NameLoc) {
  DeclarationName DN(IDInfo);
  DeclarationNameInfo NameInfo(DN, NameLoc);
  PushDeclContext(MainProgramDecl::Create(Context,
                                          Context.getTranslationUnitDecl(),
                                          NameInfo));
  PushExecutableProgramUnit();
}

void Sema::ActOnEndMainProgram(SourceLocation Loc, const IdentifierInfo *IDInfo, SourceLocation NameLoc) {
  assert(CurContext && "DeclContext imbalance!");

  DeclarationName DN(IDInfo);
  DeclarationNameInfo EndNameInfo(DN, NameLoc);

  StringRef ProgName = cast<MainProgramDecl>(CurContext)->getName();
  if (ProgName.empty()) {
    PopDeclContext();
    PopExecutableProgramUnit(Loc);
    return;
  }

  const IdentifierInfo *ID = EndNameInfo.getName().getAsIdentifierInfo();
  if (ID) {
    if (ProgName != ID->getName()) {
      Diags.Report(NameLoc, diag::err_expect_stmt_name)
        << cast<MainProgramDecl>(CurContext)->getDeclName().getAsIdentifierInfo()
        << "END PROGRAM";
    }
  }

 exit:
  PopDeclContext();
  PopExecutableProgramUnit(Loc);
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

VarDecl *Sema::ActOnKindSelector(ASTContext &C, SourceLocation IDLoc,
                                 const IdentifierInfo *IDInfo) {
  VarDecl *VD = VarDecl::Create(C, CurContext, IDLoc, IDInfo, QualType());
  CurContext->addDecl(VD);

  // Store the Decl in the IdentifierInfo for easy access.
  const_cast<IdentifierInfo*>(IDInfo)->setFETokenInfo(VD);
  return VD;
}

Decl *Sema::ActOnEntityDecl(ASTContext &C, const QualType &T, SourceLocation IDLoc,
                            const IdentifierInfo *IDInfo) {
  if (const VarDecl *Prev = IDInfo->getFETokenInfo<VarDecl>()) {
    if (Prev->getDeclContext() == CurContext) {
      Diags.Report(IDLoc, diag::err_redefinition) << IDInfo;
      Diags.Report(Prev->getLocation(), diag::note_previous_definition);
      return nullptr;
    }
  }

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

Decl *Sema::ActOnEntityDecl(ASTContext &C, DeclSpec &DS, SourceLocation IDLoc,
                            const IdentifierInfo *IDInfo) {
  QualType T = ActOnTypeName(C, DS);
  return ActOnEntityDecl(C, T, IDLoc, IDInfo);
}

Decl *Sema::ActOnImplicitEntityDecl(ASTContext &C, SourceLocation IDLoc,
                                    const IdentifierInfo *IDInfo) {
  auto Result = getCurrentImplicitTypingScope().Resolve(IDInfo);
  if(Result.first == ImplicitTypingScope::NoneRule) {
    Diags.Report(IDLoc, diag::err_undeclared_var_use)
      << IDInfo;
    return nullptr;
  } else if(Result.first == ImplicitTypingScope::DefaultRule) {
    DeclSpec DS;
    char letter = toupper(IDInfo->getNameStart()[0]);

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
  } else {
    return ActOnEntityDecl(C, Result.second, IDLoc, IDInfo);
  }
}

StmtResult Sema::ActOnPROGRAM(ASTContext &C, const IdentifierInfo *ProgName,
                              SourceLocation Loc, SourceLocation NameLoc, Expr *StmtLabel) {
  auto Result = ProgramStmt::Create(C, ProgName, Loc, NameLoc, StmtLabel);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

StmtResult Sema::ActOnUSE(ASTContext &C, UseStmt::ModuleNature MN,
                          const IdentifierInfo *ModName, Expr *StmtLabel) {
  auto Result = UseStmt::Create(C, MN, ModName, StmtLabel);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

StmtResult Sema::ActOnUSE(ASTContext &C, UseStmt::ModuleNature MN,
                          const IdentifierInfo *ModName, bool OnlyList,
                          ArrayRef<UseStmt::RenamePair> RenameNames,
                          Expr *StmtLabel) {
  auto Result = UseStmt::Create(C, MN, ModName, OnlyList, RenameNames, StmtLabel);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

StmtResult Sema::ActOnIMPORT(ASTContext &C, SourceLocation Loc,
                             ArrayRef<const IdentifierInfo*> ImportNamesList,
                             Expr *StmtLabel) {
  auto Result = ImportStmt::Create(C, Loc, ImportNamesList, StmtLabel);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

StmtResult Sema::ActOnIMPLICIT(ASTContext &C, SourceLocation Loc, DeclSpec &DS,
                               ArrayRef<ImplicitStmt::LetterSpec> LetterSpecs,
                               Expr *StmtLabel) {
  QualType Ty = ActOnTypeName(C, DS);
  for(auto Spec : LetterSpecs) {
    if(Spec.second) {
      if(toupper((Spec.second->getNameStart())[0])
         <
         toupper((Spec.first->getNameStart())[0])){
        Diags.Report(Loc, diag::err_implicit_invalid_range)
          << Spec.first << Spec.second;
        continue;
      }
    }
    if(!getCurrentImplicitTypingScope().Apply(Spec,Ty)) {
      if(getCurrentImplicitTypingScope().isNoneInThisScope())
        Diags.Report(Loc, diag::err_use_implicit_stmt_after_none);
      else {
        if(Spec.second)
          Diags.Report(Loc, diag::err_redefinition_of_implicit_stmt_rule_range)
            << Spec.first << Spec.second;
        else
          Diags.Report(Loc,diag::err_redefinition_of_implicit_stmt_rule)
            << Spec.first;
      }
    }
  }

  auto Result = ImplicitStmt::Create(C, Loc, Ty, LetterSpecs, StmtLabel);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

StmtResult Sema::ActOnIMPLICIT(ASTContext &C, SourceLocation Loc, Expr *StmtLabel) {
  // IMPLICIT NONE
  if(!getCurrentImplicitTypingScope().ApplyNone())
    Diags.Report(Loc, diag::err_use_implicit_none_stmt);
  auto Result = ImplicitStmt::Create(C, Loc, StmtLabel);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

ParameterStmt::ParamPair
Sema::ActOnPARAMETERPair(ASTContext &C, SourceLocation Loc, const IdentifierInfo *IDInfo,
                         ExprResult CE) {
  if (const VarDecl *Prev = IDInfo->getFETokenInfo<VarDecl>()) {
    Diags.Report(Loc, diag::err_redefinition) << IDInfo;
    Diags.Report(Prev->getLocation(), diag::note_previous_definition);
    return ParameterStmt::ParamPair(0, ExprResult());
  }

  QualType T = CE.get()->getType();
  VarDecl *VD = VarDecl::Create(C, CurContext, Loc, IDInfo, T);
  CurContext->addDecl(VD);

  // Store the Decl in the IdentifierInfo for easy access.
  const_cast<IdentifierInfo*>(IDInfo)->setFETokenInfo(VD);
  return ParameterStmt::ParamPair(IDInfo, CE);
}

StmtResult Sema::ActOnPARAMETER(ASTContext &C, SourceLocation Loc,
                                ArrayRef<ParameterStmt::ParamPair> ParamList,
                                Expr *StmtLabel) {
  auto Result = ParameterStmt::Create(C, Loc, ParamList, StmtLabel);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

StmtResult Sema::ActOnASYNCHRONOUS(ASTContext &C, SourceLocation Loc,
                                   ArrayRef<const IdentifierInfo *>ObjNames,
                                   Expr *StmtLabel) {
  auto Result = AsynchronousStmt::Create(C, Loc, ObjNames, StmtLabel);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

StmtResult Sema::ActOnDIMENSION(ASTContext &C, SourceLocation Loc,
                               const IdentifierInfo *IDInfo,
                               ArrayRef<std::pair<ExprResult,ExprResult> > Dims,
                               Expr *StmtLabel) {
  auto Result = DimensionStmt::Create(C, Loc, IDInfo, Dims, StmtLabel);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

StmtResult Sema::ActOnENDPROGRAM(ASTContext &C,
                                 const IdentifierInfo *ProgName,
                                 SourceLocation Loc, SourceLocation NameLoc,
                                 Expr *StmtLabel) {
  auto Result = EndProgramStmt::Create(C, ProgName, Loc, NameLoc, StmtLabel);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

StmtResult Sema::ActOnEXTERNAL(ASTContext &C, SourceLocation Loc,
                               ArrayRef<const IdentifierInfo *> ExternalNames,
                               Expr *StmtLabel) {
  auto Result = ExternalStmt::Create(C, Loc, ExternalNames, StmtLabel);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

StmtResult Sema::ActOnINTRINSIC(ASTContext &C, SourceLocation Loc,
                                ArrayRef<const IdentifierInfo *> IntrinsicNames,
                                Expr *StmtLabel) {
  // FIXME: Name Constraints.
  // FIXME: Function declaration.
  auto Result = IntrinsicStmt::Create(C, Loc, IntrinsicNames, StmtLabel);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

StmtResult Sema::ActOnAssignmentStmt(ASTContext &C, SourceLocation Loc,
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
      RHS = ImplicitCastExpr::Create(Context, RHS.get()->getLocation(),
                                     intrinsic::INT, RHS.take());
    else goto typeError;
  } else if(LHSType->isRealType()) {
    if(IsRHSReal) ;
    else if(IsRHSArithmetic)
      RHS = ImplicitCastExpr::Create(Context, RHS.get()->getLocation(),
                                     intrinsic::REAL, RHS.take());
    else goto typeError;
  } else if(LHSType->isDoublePrecisionType()) {
    if(IsRHSDblPrec) ;
    else if(IsRHSArithmetic)
      RHS = ImplicitCastExpr::Create(Context, RHS.get()->getLocation(),
                                     intrinsic::DBLE, RHS.take());
    else goto typeError;
  } else if(LHSType->isComplexType()) {
    if(IsRHSComplex) ;
    else if(IsRHSArithmetic)
      RHS = ImplicitCastExpr::Create(Context, RHS.get()->getLocation(),
                                     intrinsic::CMPLX, RHS.take());
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

  Result = AssignmentStmt::Create(C, Loc, LHS.take(), RHS.take(), StmtLabel);
  CurExecutableStmts.Append(Result);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
typeError:
  std::string TypeStrings[2];
  llvm::raw_string_ostream StreamLHS(TypeStrings[0]),
      StreamRHS(TypeStrings[1]);
  LHS.get()->getType().print(StreamLHS);
  RHS.get()->getType().print(StreamRHS);
  Diags.Report(Loc,diag::err_typecheck_assign_incompatible)
      << StreamLHS.str() << StreamRHS.str()
      << SourceRange(LHS.get()->getLocStart(),
                       RHS.get()->getLocEnd());
  return StmtError();
}

QualType Sema::ActOnArraySpec(ASTContext &C, QualType ElemTy,
                              ArrayRef<std::pair<ExprResult,ExprResult> > Dims) {
  return QualType(ArrayType::Create(C, ElemTy, Dims), 0);
}

StarFormatSpec *Sema::ActOnStarFormatSpec(ASTContext &C, SourceLocation Loc) {
  return StarFormatSpec::Create(C, Loc);
}

DefaultCharFormatSpec *Sema::ActOnDefaultCharFormatSpec(ASTContext &C,
                                                        SourceLocation Loc,
                                                        ExprResult Fmt) {
  return DefaultCharFormatSpec::Create(C, Loc, Fmt);
}

LabelFormatSpec *ActOnLabelFormatSpec(ASTContext &C, SourceLocation Loc,
                                      ExprResult Label) {
  return LabelFormatSpec::Create(C, Loc, Label);
}

StmtResult Sema::ActOnBlock(ASTContext &C, SourceLocation Loc, ArrayRef<StmtResult> Body) {
  return BlockStmt::Create(C, Loc, Body);
}

static void ResolveAssignStmtLabel(const StmtLabelScope::ForwardDecl &Self,
                                   Stmt *Destination) {
  assert(AssignStmt::classof(Self.Statement));
  static_cast<AssignStmt*>(Self.Statement)->setAddress(StmtLabelReference(Destination));
}

StmtResult Sema::ActOnAssignStmt(ASTContext &C, SourceLocation Loc,
                                 ExprResult Value, VarExpr* VarRef,
                                 Expr *StmtLabel) {
  Stmt *Result;
  auto Decl = getCurrentStmtLabelScope().Resolve(Value.get());
  if(!Decl) {
    Result = AssignStmt::Create(C, Loc, StmtLabelReference(),VarRef, StmtLabel);
    getCurrentStmtLabelScope().DeclareForwardReference(
      StmtLabelScope::ForwardDecl(Value.get(), Result,
                                  ResolveAssignStmtLabel));
  } else
    Result = AssignStmt::Create(C, Loc, StmtLabelReference(Decl),
                                VarRef, StmtLabel);

  CurExecutableStmts.Append(Result);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

static void ResolveAssignedGotoStmtLabel(const StmtLabelScope::ForwardDecl &Self,
                                         Stmt *Destination) {
  assert(AssignedGotoStmt::classof(Self.Statement));
  static_cast<AssignedGotoStmt*>(Self.Statement)->
    setAllowedValue(Self.ResolveCallbackData,StmtLabelReference(Destination));
}

StmtResult Sema::ActOnAssignedGotoStmt(ASTContext &C, SourceLocation Loc,
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
        StmtLabelScope::ForwardDecl(AllowedValues[I].get(), Result,
                                             ResolveAssignedGotoStmtLabel, I));
    }
  }

  CurExecutableStmts.Append(Result);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

static void ResolveGotoStmtLabel(const StmtLabelScope::ForwardDecl &Self,
                                 Stmt *Destination) {
  assert(GotoStmt::classof(Self.Statement));
  static_cast<GotoStmt*>(Self.Statement)->setDestination(StmtLabelReference(Destination));
}

StmtResult Sema::ActOnGotoStmt(ASTContext &C, SourceLocation Loc,
                               ExprResult Destination, Expr *StmtLabel) {
  Stmt *Result;
  auto Decl = getCurrentStmtLabelScope().Resolve(Destination.get());
  if(!Decl) {
    Result = GotoStmt::Create(C, Loc, StmtLabelReference(), StmtLabel);
    getCurrentStmtLabelScope().DeclareForwardReference(
      StmtLabelScope::ForwardDecl(Destination.get(), Result,
                                  ResolveGotoStmtLabel));
  } else
    Result = GotoStmt::Create(C, Loc, StmtLabelReference(Decl), StmtLabel);

  CurExecutableStmts.Append(Result);
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
      << Stream.str() << E.get()->getSourceRange();
}

StmtResult Sema::ActOnIfStmt(ASTContext &C, SourceLocation Loc,
                             ExprResult Condition, Expr *StmtLabel) {
  if(!IsLogicalExpression(Condition)) {
    ReportExpectedLogical(Diags, Condition);
    return StmtError();
  }

  auto Result = IfStmt::Create(C, Loc, Condition.take(), StmtLabel);
  CurExecutableStmts.Append(Result);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  CurExecutableStmts.Enter(Result);
  return Result;
}

StmtResult Sema::ActOnElseIfStmt(ASTContext &C, SourceLocation Loc,
                                 ExprResult Condition, Expr *StmtLabel) {
  if(!CurExecutableStmts.HasEntered() ||
     !CurExecutableStmts.LastEntered().is(Stmt::If)) {
    if(CurExecutableStmts.HasEntered(Stmt::If))
      ReportUnterminatedStmt(Diags, CurExecutableStmts.LastEntered(), Loc);
    else
      Diags.Report(Loc, diag::err_stmt_not_in_if) << "ELSE IF";
    return StmtError();
  }

  // typecheck
  if(!IsLogicalExpression(Condition)) {
    ReportExpectedLogical(Diags, Condition);
    return StmtError();
  }

  auto Result = IfStmt::Create(C, Loc, Condition.take(), StmtLabel);
  auto ParentIf = static_cast<IfStmt*>(CurExecutableStmts.LastEntered().Statement);
  CurExecutableStmts.Leave(C);
  ParentIf->setElseStmt(Result);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  CurExecutableStmts.Enter(Result);
  return Result;
}

StmtResult Sema::ActOnElseStmt(ASTContext &C, SourceLocation Loc, Expr *StmtLabel) {
  if(!CurExecutableStmts.HasEntered() ||
     !CurExecutableStmts.LastEntered().is(Stmt::If)) {
    if(CurExecutableStmts.HasEntered(Stmt::If))
      ReportUnterminatedStmt(Diags, CurExecutableStmts.LastEntered(), Loc);
    else
      Diags.Report(Loc, diag::err_stmt_not_in_if) << "ELSE";
    return StmtError();
  }
  auto Result = Stmt::Create(C, Stmt::Else, Loc, StmtLabel);
  CurExecutableStmts.Append(Result);
  CurExecutableStmts.LeaveIfThen(C);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

StmtResult Sema::ActOnEndIfStmt(ASTContext &C, SourceLocation Loc, Expr *StmtLabel) {
  // Report begin .. end mismatch
  if(!CurExecutableStmts.HasEntered() ||
     !CurExecutableStmts.LastEntered().is(Stmt::If)) {
    if(CurExecutableStmts.HasEntered(Stmt::If))
      ReportUnterminatedStmt(Diags, CurExecutableStmts.LastEntered(), Loc);
    else
      Diags.Report(Loc, diag::err_stmt_not_in_if) << "END IF";
    return StmtError();
  }

  auto Result = Stmt::Create(C, Stmt::EndIf, Loc, StmtLabel);
  CurExecutableStmts.Append(Result);
  CurExecutableStmts.Leave(C);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

static void ResolveDoStmtLabel(const StmtLabelScope::ForwardDecl &Self,
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
    << Stream.str() << E->getSourceRange();
  return 1;
}

/// The terminal statement of a DO-loop must not be an unconditional GO TO,
/// assigned GO TO, arithmetic IF, block IF, ELSE IF, ELSE, END IF, RETURN, STOP, END, or DO statement.
/// If the terminal statement of a DO-loop is a logical IF statement,
/// it may contain any executable statement except a DO,
/// block IF, ELSE IF, ELSE, END IF, END, or another logical IF statement.
///
/// FIXME: TODO full
static bool IsValidDoLogicalIfThenStatement(Stmt *S) {
  switch(S->getStatementID()) {
  case Stmt::Do: case Stmt::If:
  case Stmt::Else: case Stmt::EndIf:
    return false;
  default:
    return true;
  }
}

static bool IsValidDoTerminatingStatement(Stmt *S) {
  switch(S->getStatementID()) {
  case Stmt::Goto: case Stmt::AssignedGoto:
  case Stmt::Stop: case Stmt::Do: case Stmt::EndDo:
  case Stmt::Else: case Stmt::EndIf:
    return false;
  case Stmt::If: {
    auto NextStmt = static_cast<IfStmt*>(S)->getThenStmt();
    return NextStmt && IsValidDoLogicalIfThenStatement(NextStmt);
  }
  default:
    return true;
  }
}

static ExprResult ApplyDoConversionIfNeeded(ASTContext &C, ExprResult E, QualType T) {
  if(T->isIntegerType()) {
    if(E.get()->getType()->isIntegerType()) return E;
    else return ImplicitCastExpr::Create(C, E.get()->getLocation(), intrinsic::INT, E.take());
  } else if(T->isRealType()) {
    if(E.get()->getType()->isRealType()) return E;
    else return ImplicitCastExpr::Create(C, E.get()->getLocation(), intrinsic::REAL, E.take());
  } else {
    if(E.get()->getType()->isDoublePrecisionType()) return E;
    else return ImplicitCastExpr::Create(C, E.get()->getLocation(), intrinsic::DBLE, E.take());
  }
}

/// FIXME: TODO Transfer of control into the range of a DO-loop from outside the range is not permitted.
StmtResult Sema::ActOnDoStmt(ASTContext &C, SourceLocation Loc, ExprResult TerminatingStmt,
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
  E1 = ApplyDoConversionIfNeeded(C, E1, DoVar->getType());
  E2 = ApplyDoConversionIfNeeded(C, E2, DoVar->getType());
  if(E3.isUsable())
    E3 = ApplyDoConversionIfNeeded(C, E3, DoVar->getType());

  // Make sure the statement label isn't already declared
  if(TerminatingStmt.get()) {
    if(auto Decl = getCurrentStmtLabelScope().Resolve(TerminatingStmt.get())) {
      std::string String;
      llvm::raw_string_ostream Stream(String);
      TerminatingStmt.get()->print(Stream);
      Diags.Report(TerminatingStmt.get()->getLocation(),
                   diag::err_stmt_label_must_decl_after)
          << Stream.str() << "DO"
          << TerminatingStmt.get()->getSourceRange();
      return StmtError();
    }
  }
  auto Result = DoStmt::Create(C, Loc, StmtLabelReference(),
                               DoVar, E1.take(), E2.take(),
                               E3.take(), StmtLabel);
  CurExecutableStmts.Append(Result);
  if(TerminatingStmt.get())
    getCurrentStmtLabelScope().DeclareForwardReference(
      StmtLabelScope::ForwardDecl(TerminatingStmt.get(), Result,
                                           ResolveDoStmtLabel));
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  if(TerminatingStmt.get())
    CurExecutableStmts.Enter(ExecutableProgramUnitStmts::ControlFlowStmt(
                             Result,TerminatingStmt.get()));
  else CurExecutableStmts.Enter(Result);
  return Result;
}

/// FIXME: Fortran 90+: make multiple do end at one label obsolete
void Sema::CheckStatementLabelEndDo(Expr *StmtLabel, Stmt *S) {
  if(!CurExecutableStmts.HasEntered())
    return;

  auto I = CurExecutableStmts.ControlFlowStack.size();
  size_t LastUnterminated = 0;
  do {
    I--;
    if(!CurExecutableStmts.ControlFlowStack[I].is(Stmt::Do)) {
      if(!LastUnterminated) LastUnterminated = I;
    } else {
      auto ParentDo = static_cast<DoStmt*>(CurExecutableStmts.ControlFlowStack[I].Statement);
      auto ParentDoExpectedLabel = CurExecutableStmts.ControlFlowStack[I].ExpectedEndDoLabel;
      if(!ParentDoExpectedLabel) {
        if(!LastUnterminated) LastUnterminated = I;
      } else {
        if(getCurrentStmtLabelScope().IsSame(ParentDoExpectedLabel, StmtLabel)) {
          // END DO
          getCurrentStmtLabelScope().RemoveForwardReference(ParentDo);
          if(!IsValidDoTerminatingStatement(S)) {
            Diags.Report(S->getLocation(),
                         diag::err_invalid_do_terminating_stmt);
          }
          ParentDo->setTerminatingStmt(StmtLabelReference(S));
          if(!LastUnterminated)
            CurExecutableStmts.Leave(Context);
          else
            ReportUnterminatedStmt(Diags,
                                   CurExecutableStmts.ControlFlowStack[LastUnterminated],
                                   S->getLocation());
        } else if(!LastUnterminated) LastUnterminated = I;
      }
    }
  } while(I>0);
}

StmtResult Sema::ActOnEndDoStmt(ASTContext &C, SourceLocation Loc, Expr *StmtLabel) {
  // Report begin .. end mismatch
  if(!CurExecutableStmts.HasEntered() ||
     !CurExecutableStmts.LastEntered().is(Stmt::Do)) {
    bool HasMatchingDo = false;
    for(auto I : CurExecutableStmts.ControlFlowStack) {
      if(I.is(Stmt::Do) && !I.ExpectedEndDoLabel) {
        HasMatchingDo = true;
        break;
      }
    }
    if(!HasMatchingDo)
      Diags.Report(Loc, diag::err_end_do_without_do);
    else ReportUnterminatedStmt(Diags, CurExecutableStmts.LastEntered(), Loc);
    return StmtError();
  }

  // If last loop was a DO with terminating label, we expect it to finish before this loop
  if(CurExecutableStmts.LastEntered().ExpectedEndDoLabel) {
    ReportUnterminatedLabeledDoStmt(Diags, CurExecutableStmts.LastEntered(), Loc);
    return StmtError();
  }

  auto Result = Stmt::Create(C, Stmt::EndDo, Loc, StmtLabel);
  CurExecutableStmts.Append(Result);
  CurExecutableStmts.Leave(C);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

StmtResult Sema::ActOnContinueStmt(ASTContext &C, SourceLocation Loc, Expr *StmtLabel) {
  auto Result = ContinueStmt::Create(C, Loc, StmtLabel);
  CurExecutableStmts.Append(Result);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

StmtResult Sema::ActOnStopStmt(ASTContext &C, SourceLocation Loc, ExprResult StopCode, Expr *StmtLabel) {
  auto Result = StopStmt::Create(C, Loc, StopCode.take(), StmtLabel);
  CurExecutableStmts.Append(Result);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

StmtResult Sema::ActOnPrintStmt(ASTContext &C, SourceLocation Loc, FormatSpec *FS,
                                ArrayRef<ExprResult> OutputItemList,
                                Expr *StmtLabel) {
  auto Result = PrintStmt::Create(C, Loc, FS, OutputItemList, StmtLabel);
  CurExecutableStmts.Append(Result);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

RecordDecl *Sema::ActOnDerivedTypeDecl(ASTContext &C, SourceLocation Loc,
                                       SourceLocation NameLoc,
                                       const IdentifierInfo* IDInfo) {
  RecordDecl* Record = RecordDecl::Create(C, CurContext, Loc, NameLoc, IDInfo);
  CurContext->addDecl(Record);
  PushDeclContext(Record);
  return Record;
}

FieldDecl *Sema::ActOnDerivedTypeFieldDecl(ASTContext &C, DeclSpec &DS, SourceLocation IDLoc,
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
