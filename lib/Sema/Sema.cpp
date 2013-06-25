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
  : Context(ctxt), Diags(D), CurContext(0), IntrinsicFunctionMapping(LangOptions()) {
}

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
  if (auto Prev = LookupIdentifier(IDInfo)) {
    Diags.Report(IDLoc, diag::err_redefinition) << IDInfo;
    Diags.Report(Prev->getLocation(), diag::note_previous_definition);
    return nullptr;
  }

  VarDecl *VD = VarDecl::Create(C, CurContext, IDLoc, IDInfo, T);
  CurContext->addDecl(VD);

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

Decl *Sema::LookupIdentifier(const IdentifierInfo *IDInfo) {
  auto Result = CurContext->lookup(IDInfo);
  if(Result.first >= Result.second) return nullptr;
  assert(Result.first + 1 >= Result.second);
  return *Result.first;
}

Decl *Sema::ResolveIdentifier(const IdentifierInfo *IDInfo) {
  return LookupIdentifier(IDInfo);
}

StmtResult Sema::ActOnBundledCompoundStmt(ASTContext &C, SourceLocation Loc,
                                          ArrayRef<Stmt*> Body, Expr *StmtLabel) {
  if(Body.size() == 1) {
    auto Result = Body[0];
    if(StmtLabel) {
      DeclareStatementLabel(StmtLabel, Result);
      Result->setStmtLabel(StmtLabel);
    }
    return Result;
  }
  auto Result = BundledCompoundStmt::Create(C, Loc, Body, StmtLabel);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
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
                               ImplicitStmt::LetterSpecTy LetterSpec,
                               Expr *StmtLabel) {
  QualType Ty = ActOnTypeName(C, DS);

  // check a <= b
  if(LetterSpec.second) {
    if(toupper((LetterSpec.second->getNameStart())[0])
       <
       toupper((LetterSpec.first->getNameStart())[0])) {
      Diags.Report(Loc, diag::err_implicit_invalid_range)
        << LetterSpec.first << LetterSpec.second;
      return StmtError();
    }
  }

  // apply the rule
  if(!getCurrentImplicitTypingScope().Apply(LetterSpec,Ty)) {
    if(getCurrentImplicitTypingScope().isNoneInThisScope())
      Diags.Report(Loc, diag::err_use_implicit_stmt_after_none);
    else {
      if(LetterSpec.second)
        Diags.Report(Loc, diag::err_redefinition_of_implicit_stmt_rule_range)
          << LetterSpec.first << LetterSpec.second;
      else
        Diags.Report(Loc,diag::err_redefinition_of_implicit_stmt_rule)
          << LetterSpec.first;

    }
    return StmtError();
  }

  auto Result = ImplicitStmt::Create(C, Loc, Ty, LetterSpec, StmtLabel);
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

class NonConstantExprASTSearch {
public:
  SmallVector<Expr *,8> Results;

  void visit(Expr *E) {
    if(ConstantExpr::classof(E)) ;
    else if(UnaryExpr *Unary = dyn_cast<UnaryExpr>(E)) {
      visit(Unary->getExpression());
    } else if(BinaryExpr *Binary = dyn_cast<BinaryExpr>(E)) {
      visit(Binary->getLHS());
      visit(Binary->getRHS());
    } else if(ImplicitCastExpr *Cast = dyn_cast<ImplicitCastExpr>(E)) {
      visit(Cast->getExpression());
    } else if(VarExpr *Var = dyn_cast<VarExpr>(E)) {
      if(!Var->getVarDecl()->isParameter())
        Results.push_back(E);
    }
    else Results.push_back(E);
  }
};

StmtResult Sema::ActOnPARAMETER(ASTContext &C, SourceLocation Loc,
                                SourceLocation IDLoc,
                                const IdentifierInfo *IDInfo, ExprResult Value,
                                Expr *StmtLabel) {
  VarDecl *VD = nullptr;

  if (auto Prev = LookupIdentifier(IDInfo)) {
    VD = dyn_cast<VarDecl>(Prev);
    if(!VD || VD->isParameter()) {
      Diags.Report(IDLoc, diag::err_redefinition) << IDInfo;
      Diags.Report(Prev->getLocation(), diag::note_previous_definition);
      return StmtError();
    }
  }

  // Make sure the value is a constant expression.
  NonConstantExprASTSearch Visitor;
  Visitor.visit(Value.get());
  if(!Visitor.Results.empty()) {
    Diags.Report(IDLoc, diag::err_parameter_requires_const_init)
        << IDInfo << Value.get()->getSourceRange();
    for(auto E : Visitor.Results) {
      Diags.Report(E->getLocation(), diag::note_parameter_value_invalid_expr)
          << E->getSourceRange();
    }
    return StmtError();
  }

  if(VD) {
    auto MyT = VD->getType();
    // FIXME: TODO Implicit cast.
    VD->MutateIntoParameter(Value.take());
  } else {
    QualType T = Value.get()->getType();
    VD = VarDecl::Create(C, CurContext, IDLoc, IDInfo, T);
    VD->MutateIntoParameter(Value.take());
    CurContext->addDecl(VD);
  }

  auto Result = DeclStmt::Create(C, Loc, VD, StmtLabel);
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
                                SourceLocation IDLoc,
                                const IdentifierInfo *IDInfo,
                                Expr *StmtLabel) {
  auto FuncResult = IntrinsicFunctionMapping.Resolve(IDInfo);
  if(FuncResult.IsInvalid) {
    Diags.Report(IDLoc, diag::err_intrinsic_invalid_func)
      << IDInfo
      << SourceRange(IDLoc,
                     SourceLocation::getFromPointer(
                       IDLoc.getPointer() + IDInfo->getLength()));
    return StmtError();
  }

  if (auto Prev = LookupIdentifier(IDInfo)) {
    Diags.Report(IDLoc, diag::err_redefinition) << IDInfo;
    Diags.Report(Prev->getLocation(), diag::note_previous_definition);
    return StmtError();
  }

  auto Decl = IntrinsicFunctionDecl::Create(C, CurContext, IDLoc, IDInfo,
                                            C.IntegerTy, FuncResult.Function);
  CurContext->addDecl(Decl);

  auto Result = DeclStmt::Create(C, Loc, Decl, StmtLabel);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

/// FIXME: add sema
StmtResult Sema::ActOnDATA(ASTContext &C, SourceLocation Loc,
                           ArrayRef<ExprResult> LHS,
                           ArrayRef<ExprResult> RHS,
                           Expr *StmtLabel) {
  SmallVector<Expr*, 8> Names (LHS.size());
  SmallVector<Expr*, 8> Values(RHS.size());
  for(size_t I = 0; I < LHS.size(); ++I)
    Names[I] = LHS[I].take();
  for(size_t I = 0; I < RHS.size(); ++I)
    Values[I] = RHS[I].take();

  return DataStmt::Create(C, Loc, Names, Values, StmtLabel);
}

ExprResult Sema::ActOnDATAConstantExpr(ASTContext &C,
                                       SourceLocation RepeatLoc,
                                       ExprResult RepeatCount,
                                       ExprResult Value) {
  IntegerConstantExpr *RepeatExpr = nullptr;
  bool HasErrors = false;

  if(RepeatCount.isUsable()) {
    RepeatExpr = dyn_cast<IntegerConstantExpr>(RepeatCount.get());
    if(!RepeatExpr ||
       RepeatExpr->getValue().isNegative() ||
       !RepeatExpr->getValue()) {
      Diags.Report(RepeatCount.get()->getLocation(),
                   diag::err_expected_integer_gt_0)
        << RepeatCount.get()->getSourceRange();
      HasErrors = true;
    }
  }

  auto Constant = dyn_cast<ConstantExpr>(Value.get());
  if(!Constant) {
    /// the value can also be a constant variable
    VarExpr *Var = dyn_cast<VarExpr>(Value.get());
    if(!(Var && Var->getVarDecl()->isParameter())) {
      Diags.Report(Value.get()->getLocation(),
                   diag::err_expected_constant_expr)
        << Value.get()->getSourceRange();
      HasErrors = true;
    }
  }

  if(HasErrors) return ExprError();
  return RepeatExpr? RepeatedConstantExpr::Create(C, RepeatLoc,
                                                  RepeatExpr, Value.take())
                   : Value;
}

/// FIXME: allow outer scope integer constants.
/// FIXME: walk constant expressions like 1+1.
ExprResult Sema::ActOnDATAOuterImpliedDoExpr(ASTContext &C,
                                             ExprResult Expression) {
  /// Resolves the implied do AST.
  class ImpliedDoResolver {
  public:
    ASTContext &C;
    DiagnosticsEngine &Diags;
    Sema &S;
    InnerScope *CurScope;
    bool HasErrors;

    ImpliedDoResolver(ASTContext &Context,
                      DiagnosticsEngine &Diag, Sema &Sem)
      : C(Context), Diags(Diag), S(Sem), CurScope(nullptr),
        HasErrors(false) {
    }

    Expr* visit(Expr *E) {
      if(auto ImpliedDo = dyn_cast<ImpliedDoExpr>(E))
        return visit(ImpliedDo);
      else if(auto ArrayElement = dyn_cast<ArrayElementExpr>(E))
        return visit(ArrayElement);
      else {
        Diags.Report(E->getLocation(), diag::err_implied_do_expect_expr)
          << E->getSourceRange();
        HasErrors = true;
      }
      return E;
    }

    Expr *visitLeaf(Expr *E) {
      if(auto Unresolved = dyn_cast<UnresolvedIdentifierExpr>(E)) {
        auto IDInfo = Unresolved->getIdentifier();
        if(auto Declaration = CurScope->Resolve(IDInfo)) {
          auto VD = dyn_cast<VarDecl>(Declaration);
          assert(VD);
          return VarExpr::Create(C, Unresolved->getLocation(), VD);
        } else {
          /*FIXME: outer scope constants.
           *if(auto OuterDeclaration = S.ResolveIdentifier(IDInfo)) {
            if(auto VD = dyn_cast<VarDecl>(OuterDeclaration)) {
              // a constant variable
              if(VD->isParameter())
                return VarExpr::Create(C, )
            }
          }*/
          Diags.Report(E->getLocation(), diag::err_undeclared_var_use)
            << IDInfo << E->getSourceRange();
          HasErrors = true;
        }
      }
      else if(auto Unary = dyn_cast<UnaryExpr>(E)) {
        return UnaryExpr::Create(C, Unary->getLocation(),
                                 Unary->getOperator(),
                                 visitLeaf(Unary->getExpression()));
      }
      else if(auto Binary = dyn_cast<BinaryExpr>(E)) {
        return BinaryExpr::Create(C, Binary->getLocation(),
                                  Binary->getOperator(),
                                  visitLeaf(Binary->getLHS()),
                                  visitLeaf(Binary->getRHS()));
      }
      else if(!IntegerConstantExpr::classof(E)) {
        Diags.Report(E->getLocation(),diag::err_implied_do_expect_leaf_expr )
          << E->getSourceRange();
        HasErrors = true;
      }
      return E;
    }

    Expr *visit(ArrayElementExpr *E) {
      auto Subscripts = E->getArguments();
      SmallVector<Expr*, 8> ResolvedSubscripts(Subscripts.size());
      for(size_t I = 0; I < Subscripts.size(); ++I)
        ResolvedSubscripts[I] = visitLeaf(Subscripts[I]);
      return ArrayElementExpr::Create(C, E->getLocation(),
                                      E->getTarget(), ResolvedSubscripts);
    }

    Expr *visit(ImpliedDoExpr *E) {
      // enter a new scope.
      InnerScope Scope(CurScope);
      CurScope = &Scope;
      auto Var = E->getVarDecl();
      Scope.Declare(Var->getIdentifier(), Var);

      Expr *InitialParam, *TerminalParam, *IncParam;

      InitialParam = visitLeaf(E->getInitialParameter());
      TerminalParam = visitLeaf(E->getTerminalParameter());
      if(E->getIncrementationParameter())
        IncParam = visitLeaf(E->getIncrementationParameter());
      else IncParam = nullptr;

      auto Body = E->getBody();
      SmallVector<Expr*, 8> ResolvedBody(Body.size());
      for(size_t I = 0; I < Body.size(); ++I)
        ResolvedBody[I] = visit(Body[I]);

      CurScope = CurScope->getParent();
      return ImpliedDoExpr::Create(C, E->getLocation(), Var,
                                   ResolvedBody, InitialParam,
                                   TerminalParam, IncParam);
    }
  };

  ImpliedDoResolver Visitor(C, Diags, *this);
  ExprResult Result = Visitor.visit(Expression.get());
  return Visitor.HasErrors? ExprError() : Result;
}

ExprResult Sema::ActOnDATAImpliedDoExpr(ASTContext &C, SourceLocation Loc,
                                        SourceLocation IDLoc,
                                        const IdentifierInfo *IDInfo,
                                        ArrayRef<ExprResult> Body,
                                        ExprResult E1, ExprResult E2,
                                        ExprResult E3) {
  // NB: The unresolved identifier resolution is done in the OuterImpliedDo.

  llvm::SmallVector<Expr*, 8> BodyExprs(Body.size());
  for(size_t I = 0; I < BodyExprs.size(); ++I)
    BodyExprs[I] = Body[I].take();

  auto Decl = VarDecl::Create(C, CurContext, IDLoc, IDInfo, C.IntegerTy);

  return ImpliedDoExpr::Create(C, Loc, Decl, BodyExprs,
                               E1.take(), E2.take(), E3.take());
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
  Diags.Report(Loc,diag::err_typecheck_assign_incompatible)
      << LHS.get()->getType() << RHS.get()->getType()
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
  Diag.Report(E.get()->getLocation(), diag::err_typecheck_expected_logical_expr)
    << E.get()->getType() << E.get()->getSourceRange();
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
  Diags.Report(E->getLocation(), DiagType)
    << E->getType() << E->getSourceRange();
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
      Diags.Report(Decl->getStmtLabel()->getLocation(),
                   diag::note_previous_definition)
          << Decl->getStmtLabel()->getSourceRange();
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
