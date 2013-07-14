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
  : Context(ctxt), Diags(D), CurContext(0), IntrinsicFunctionMapping(LangOptions()),
    CurExecutableStmts(nullptr),
    CurStmtLabelScope(nullptr), CurImplicitTypingScope(nullptr) {
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

bool Sema::IsInsideFunctionOrSubroutine() const {
  auto FD = dyn_cast<FunctionDecl>(CurContext);
  return FD && (FD->isNormalFunction() || FD->isSubroutine());
}

FunctionDecl *Sema::CurrentContextAsFunction() const {
  return dyn_cast<FunctionDecl>(CurContext);
}


void Sema::PushExecutableProgramUnit(ExecutableProgramUnitScope &Scope) {
  // Enter new statement label scope
  Scope.StmtLabels.setParent(CurStmtLabelScope);
  CurStmtLabelScope = &Scope.StmtLabels;

  // Enter new implicit typing scope
  Scope.ImplicitTypingRules.setParent(CurImplicitTypingScope);
  CurImplicitTypingScope = &Scope.ImplicitTypingRules;

  CurExecutableStmts = &Scope.Body;
}

static bool IsValidDoTerminatingStatement(Stmt *S);

// Unterminated labeled do statement
static void ReportUnterminatedLabeledDoStmt(DiagnosticsEngine &Diags,
                                            const BlockStmtBuilder::Entry &S,
                                            SourceLocation Loc) {
  std::string Str;
  llvm::raw_string_ostream Stream(Str);
  S.ExpectedEndDoLabel->dump(Stream);
  Diags.Report(Loc, diag::err_expected_stmt_label_end_do) << Stream.str();
}

// Unterminated if/do statement
static void ReportUnterminatedStmt(DiagnosticsEngine &Diags,
                                   const BlockStmtBuilder::Entry &S,
                                   SourceLocation Loc,
                                   bool ReportUnterminatedLabeledDo = true) {
  const char * Keyword;
  switch(S.Statement->getStmtClass()) {
  case Stmt::IfStmtClass: Keyword = "END IF"; break;
  case Stmt::DoWhileStmtClass:
  case Stmt::DoStmtClass: {
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
  auto StmtLabelForwardDecls = CurStmtLabelScope->getForwardDecls();
  for(size_t I = 0; I < StmtLabelForwardDecls.size(); ++I) {
    if(auto Decl = CurStmtLabelScope->Resolve(StmtLabelForwardDecls[I].StmtLabel))
      StmtLabelForwardDecls[I].ResolveCallback(Diags, StmtLabelForwardDecls[I], Decl);
    else {
      std::string Str;
      llvm::raw_string_ostream Stream(Str);
      StmtLabelForwardDecls[I].StmtLabel->dump(Stream);
      Diags.Report(StmtLabelForwardDecls[I].StmtLabel->getLocation(),
                   diag::err_undeclared_stmt_label_use)
          << Stream.str()
          << StmtLabelForwardDecls[I].StmtLabel->getSourceRange();
    }
  }
  // FIXME: TODO warning unused statement labels.
  // Clear the statement labels scope
  CurStmtLabelScope = CurStmtLabelScope->getParent();

  // Report unterminated statements.
  if(CurExecutableStmts->HasEntered()) {
    // If do ends with statement label, the error
    // was already reported as undeclared label use.
    ReportUnterminatedStmt(Diags, CurExecutableStmts->LastEntered(), Loc, false);
  }
  auto Body = CurExecutableStmts->LeaveOuterBody(Context, cast<Decl>(CurContext)->getLocation());
  if(auto FD = dyn_cast<FunctionDecl>(CurContext))
    FD->setBody(Body);
  else
    cast<MainProgramDecl>(CurContext)->setBody(Body);

  CurImplicitTypingScope = CurImplicitTypingScope->getParent();
}

void BlockStmtBuilder::Enter(Entry S) {
  S.BeginOffset = StmtList.size();
  ControlFlowStack.push_back(S);
}

Stmt *BlockStmtBuilder::CreateBody(ASTContext &C,
                                             const Entry &Last) {
  auto Ref = ArrayRef<Stmt*>(StmtList);
  return BlockStmt::Create(C, Last.Statement->getLocation(),
                           ArrayRef<Stmt*>(Ref.begin() + Last.BeginOffset,
                                           Ref.end()));
}

void BlockStmtBuilder::LeaveIfThen(ASTContext &C) {
  auto Last = ControlFlowStack.back();
  assert(isa<IfStmt>(Last));

  auto Body = CreateBody(C, Last);
  cast<IfStmt>(Last.Statement)->setThenStmt(Body);
  StmtList.erase(StmtList.begin() + Last.BeginOffset, StmtList.end());
}

void BlockStmtBuilder::Leave(ASTContext &C) {
  assert(ControlFlowStack.size());
  auto Last = ControlFlowStack.pop_back_val();

  /// Create the body
  auto Body = CreateBody(C, Last);
  if(auto Parent = dyn_cast<IfStmt>(Last.Statement)) {
    if(Parent->getThenStmt())
      Parent->setElseStmt(Body);
    else Parent->setThenStmt(Body);
  } else
    cast<CFBlockStmt>(Last.Statement)->setBody(Body);
  StmtList.erase(StmtList.begin() + Last.BeginOffset, StmtList.end());
}

Stmt *BlockStmtBuilder::LeaveOuterBody(ASTContext &C, SourceLocation Loc) {
  if(StmtList.size() == 1) return StmtList[0];
  return BlockStmt::Create(C, Loc, StmtList);
}

bool BlockStmtBuilder::HasEntered(Stmt::StmtClass StmtType) const {
  for(auto I : ControlFlowStack) {
    if(I.is(StmtType)) return true;
  }
  return false;
}

void BlockStmtBuilder::Append(Stmt *S) {
  assert(S);
  StmtList.push_back(S);
}

void Sema::DeclareStatementLabel(Expr *StmtLabel, Stmt *S) {
  if(auto Decl = getCurrentStmtLabelScope()->Resolve(StmtLabel)) {
    std::string Str;
    llvm::raw_string_ostream Stream(Str);
    StmtLabel->dump(Stream);
    Diags.Report(StmtLabel->getLocation(),
                 diag::err_redefinition_of_stmt_label)
        << Stream.str() << StmtLabel->getSourceRange();
    Diags.Report(Decl->getStmtLabel()->getLocation(),
                 diag::note_previous_definition)
        << Decl->getStmtLabel()->getSourceRange();
  }
  else {
    getCurrentStmtLabelScope()->Declare(StmtLabel, S);
    /// Check to see if it matches the last do stmt.
    CheckStatementLabelEndDo(StmtLabel, S);
  }
}

void Sema::ActOnTranslationUnit(TranslationUnitScope &Scope) {
  PushDeclContext(Context.getTranslationUnitDecl());
  CurImplicitTypingScope = &Scope.ImplicitTypingRules;
  CurStmtLabelScope = &Scope.StmtLabels;
}

void Sema::ActOnEndTranslationUnit() {

}

MainProgramDecl *Sema::ActOnMainProgram(ASTContext &C, MainProgramScope &Scope,
                                        const IdentifierInfo *IDInfo,
                                        SourceLocation NameLoc) {
  bool Declare = true;
  if (auto Prev = LookupIdentifier(IDInfo)) {
    Diags.Report(NameLoc, diag::err_redefinition) << IDInfo;
    Diags.Report(Prev->getLocation(), diag::note_previous_definition);
    Declare = false;
  }

  DeclarationName DN(IDInfo);
  DeclarationNameInfo NameInfo(DN, NameLoc);
  auto ParentDC = C.getTranslationUnitDecl();
  auto Program = MainProgramDecl::Create(C, ParentDC, NameInfo);
  if(Declare)
    ParentDC->addDecl(Program);
  PushDeclContext(Program);
  PushExecutableProgramUnit(Scope);
  return Program;
}

void Sema::ActOnEndMainProgram(SourceLocation Loc, const IdentifierInfo *IDInfo, SourceLocation NameLoc) {
  assert(CurContext && "DeclContext imbalance!");

  DeclarationName DN(IDInfo);
  DeclarationNameInfo EndNameInfo(DN, NameLoc);

  StringRef ProgName = cast<MainProgramDecl>(CurContext)->getName();
  if (ProgName.empty()) {
    PopExecutableProgramUnit(Loc);
    PopDeclContext();
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
  PopExecutableProgramUnit(Loc);
  PopDeclContext();
}

bool Sema::IsValidFunctionType(QualType Type) {
  if(Type->isIntegerType() || Type->isRealType() || Type->isComplexType() ||
     Type->isCharacterType() || Type->isLogicalType()) return true;
  return false;
}

void Sema::SetFunctionType(FunctionDecl *Function, QualType Type,
                           SourceLocation DiagLoc, SourceRange DiagRange) {
  if(!IsValidFunctionType(Type)) {
    Diags.Report(DiagLoc, diag::err_func_invalid_type)
      << Function->getIdentifier(); // FIXME: add diag range.
    return;
  }
  Function->setType(Type);
}

FunctionDecl *Sema::ActOnSubProgram(ASTContext &C, SubProgramScope &Scope,
                                    bool IsSubRoutine, SourceLocation IDLoc,
                                    const IdentifierInfo *IDInfo, DeclSpec &ReturnTypeDecl) {
  bool Declare = true;
  if (auto Prev = LookupIdentifier(IDInfo)) {
    Diags.Report(IDLoc, diag::err_redefinition) << IDInfo;
    Diags.Report(Prev->getLocation(), diag::note_previous_definition);
    Declare = false;
  }

  DeclarationNameInfo NameInfo(IDInfo, IDLoc);
  auto ParentDC = CurContext;

  QualType ReturnType;
  if(ReturnTypeDecl.getTypeSpecType() != TST_unspecified)
    ReturnType = ActOnTypeName(C, ReturnTypeDecl);
  auto Func = FunctionDecl::Create(C, IsSubRoutine? FunctionDecl::Subroutine :
                                                    FunctionDecl::NormalFunction,
                                   ParentDC, NameInfo, ReturnType);
  if(ReturnTypeDecl.getTypeSpecType() != TST_unspecified)
    SetFunctionType(Func, ReturnType, IDLoc, SourceRange());//FIXME: proper loc and range
  if(Declare)
    ParentDC->addDecl(Func);
  PushDeclContext(Func);
  PushExecutableProgramUnit(Scope);
  auto RetVar = ReturnVarDecl::Create(C, CurContext, IDLoc, IDInfo);
  CurContext->addDecl(RetVar);
  return Func;
}

VarDecl *Sema::ActOnSubProgramArgument(ASTContext &C, SourceLocation IDLoc,
                                       const IdentifierInfo *IDInfo) {
  if (auto Prev = LookupIdentifier(IDInfo)) {
    Diags.Report(IDLoc, diag::err_redefinition) << IDInfo;
    Diags.Report(Prev->getLocation(), diag::note_previous_definition);
    return nullptr;
  }

  VarDecl *VD = VarDecl::CreateArgument(C, CurContext, IDLoc, IDInfo);
  CurContext->addDecl(VD);
  return VD;
}

void Sema::ActOnSubProgramStarArgument(ASTContext &C, SourceLocation Loc) {
  // FIXME: TODO
}

void Sema::ActOnSubProgramArgumentList(ASTContext &C, ArrayRef<VarDecl*> Arguments) {
  assert(isa<FunctionDecl>(CurContext));
  cast<FunctionDecl>(CurContext)->setArguments(C, Arguments);
}

void Sema::ActOnEndSubProgram(ASTContext &C, SourceLocation Loc) {
  PopExecutableProgramUnit(Loc);
  PopDeclContext();
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
  unsigned Kind = BuiltinType::NoKind;
  // FIXME: eval DS.getKindSelector();
  if(DS.isDoublePrecision()) {
    assert(!Kind);
    Kind = BuiltinType::Real8;
  }
  if(DS.hasLengthSelector() && !DS.isStarLengthSelector())
    CheckCharacterLengthSpec(DS.getLengthSelector());
  Result = C.getExtQualType(TypeNode, Quals, Kind,
                            DS.isDoublePrecision(),
                            DS.isStarLengthSelector(),
                            DS.getLengthSelector());

  if (!Quals.hasAttributeSpec(Qualifiers::AS_dimension))
    return Result;

  return ActOnArraySpec(C, Result, DS.getDimensions());
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
    FunctionDecl *FD = dyn_cast<FunctionDecl>(Prev);
    if(auto VD = dyn_cast<VarDecl>(Prev)) {
      if(VD->isArgument() && VD->getType().isNull()) {
        VD->setType(T);
        return VD;
      }
    } else if(isa<ReturnVarDecl>(Prev))
      FD = CurrentContextAsFunction();
    if(FD && (FD->isNormalFunction() || FD->isExternal())) {
      if(FD->getType().isNull()) {
        SetFunctionType(FD, T, IDLoc, SourceRange()); //Fixme: proper loc and range
        return FD;
      } else {
        Diags.Report(IDLoc, diag::err_func_return_type_already_specified) << IDInfo;
        return nullptr;
      }
    }
    Diags.Report(IDLoc, diag::err_redefinition) << IDInfo;
    Diags.Report(Prev->getLocation(), diag::note_previous_definition);
    return nullptr;
  }

  VarDecl *VD = VarDecl::Create(C, CurContext, IDLoc, IDInfo, T);
  CurContext->addDecl(VD);

  if(!T.isNull()) {
    auto SubT = T;
    if(T->isArrayType()) {
      CheckArrayTypeDeclarationCompability(T->asArrayType(), VD);
      SubT = T->asArrayType()->getElementType();
    }
    else if(SubT->isCharacterType())
      CheckCharacterLengthDeclarationCompability(SubT, VD);
  }

  return VD;
}

Decl *Sema::ActOnEntityDecl(ASTContext &C, DeclSpec &DS, SourceLocation IDLoc,
                            const IdentifierInfo *IDInfo) {
  QualType T = ActOnTypeName(C, DS);
  return ActOnEntityDecl(C, T, IDLoc, IDInfo);
}

QualType Sema::ResolveImplicitType(const IdentifierInfo *IDInfo) {
  auto Result = getCurrentImplicitTypingScope()->Resolve(IDInfo);
  if(Result.first == ImplicitTypingScope::NoneRule) return QualType();
  else if(Result.first == ImplicitTypingScope::DefaultRule) {
    char letter = toupper(IDInfo->getNameStart()[0]);
    // IMPLICIT statement:
    // `If a mapping is not specified for a letter, the default for a
    //  program unit or an interface body is default integer if the
    //  letter is I, K, ..., or N and default real otherwise`
    if(letter >= 'I' && letter <= 'N')
      return Context.IntegerTy;
    else return Context.RealTy;
  } else return Result.second;
}

Decl *Sema::ActOnImplicitEntityDecl(ASTContext &C, SourceLocation IDLoc,
                                    const IdentifierInfo *IDInfo) {
  auto Type = ResolveImplicitType(IDInfo);
  if(Type.isNull()) {
    Diags.Report(IDLoc, diag::err_undeclared_var_use)
      << IDInfo;
    return nullptr;
  }
  return ActOnEntityDecl(C, Type, IDLoc, IDInfo);
}

Decl *Sema::LookupIdentifier(const IdentifierInfo *IDInfo) {
  auto Result = CurContext->lookup(IDInfo);
  if(Result.first >= Result.second) return nullptr;
  assert(Result.first + 1 >= Result.second);
  return *Result.first;
}

Decl *Sema::ResolveIdentifier(const IdentifierInfo *IDInfo) {
  for(auto Context = CurContext; Context; Context = Context->getParent()) {
    auto Result = Context->lookup(IDInfo);
    if(Result.first < Result.second) {
      assert(Result.first + 1 >= Result.second);
      return *Result.first;
    }
  }
  return nullptr;
}

StmtResult Sema::ActOnCompoundStmt(ASTContext &C, SourceLocation Loc,
                                   ArrayRef<Stmt*> Body, Expr *StmtLabel) {
  if(Body.size() == 1) {
    auto Result = Body[0];
    if(StmtLabel) {
      DeclareStatementLabel(StmtLabel, Result);
      Result->setLocation(Loc);
      Result->setStmtLabel(StmtLabel);
    }
    return Result;
  }
  auto Result = CompoundStmt::Create(C, Loc, Body, StmtLabel);
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
  if(!getCurrentImplicitTypingScope()->Apply(LetterSpec,Ty)) {
    if(getCurrentImplicitTypingScope()->isNoneInThisScope())
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
  if(!getCurrentImplicitTypingScope()->ApplyNone())
    Diags.Report(Loc, diag::err_use_implicit_none_stmt);

  auto Result = ImplicitStmt::Create(C, Loc, StmtLabel);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

StmtResult Sema::ActOnPARAMETER(ASTContext &C, SourceLocation Loc,
                                SourceLocation EqualLoc,
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
  if(!Value.get()->isEvaluatable(C)) {
    llvm::SmallVector<const Expr*, 16> Results;
    Value.get()->GatherNonEvaluatableExpressions(C, Results);
    Diags.Report(IDLoc, diag::err_parameter_requires_const_init)
        << IDInfo << Value.get()->getSourceRange();
    for(auto E : Results) {
      Diags.Report(E->getLocation(), diag::note_parameter_value_invalid_expr)
          << E->getSourceRange();
    }
    return StmtError();
  }

  if(VD) {
    Value = TypecheckAssignment(VD->getType(), Value,
                                EqualLoc, IDLoc);
    if(Value.isInvalid()) return StmtError();
    // FIXME: if value is invalid, mutate into parameter givin a zero value
    VD->MutateIntoParameter(Value.get());
  } else {
    QualType T = Value.get()->getType();
    VD = VarDecl::Create(C, CurContext, IDLoc, IDInfo, T);
    VD->MutateIntoParameter(Value.get());
    CurContext->addDecl(VD);
  }

  auto Result = ParameterStmt::Create(C, Loc, IDInfo, Value.get(), StmtLabel);
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
                                SourceLocation IDLoc,
                                const IdentifierInfo *IDInfo,
                                ArrayRef<ArraySpec*> Dims,
                                Expr *StmtLabel) {
  auto Result = DimensionStmt::Create(C, IDLoc, IDInfo, Dims, StmtLabel);
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
                               SourceLocation IDLoc, const IdentifierInfo *IDInfo,
                               Expr *StmtLabel) {
  QualType Type;
  SourceLocation TypeLoc;
  if (auto Prev = LookupIdentifier(IDInfo)) {
    auto VD = dyn_cast<VarDecl>(Prev);
    if(VD && VD->isLocalVariable()) {
        Type = VD->getType();
        TypeLoc = VD->getLocation();
        CurContext->removeDecl(VD);
    } else {
      Diags.Report(IDLoc, diag::err_redefinition) << IDInfo;
      Diags.Report(Prev->getLocation(), diag::note_previous_definition);
      return StmtError();
    }
  }

  DeclarationNameInfo DeclName(IDInfo,IDLoc);
  auto Decl = FunctionDecl::Create(C, FunctionDecl::External, CurContext,
                                   DeclName, Type);
  if(!Type.isNull())
    SetFunctionType(Decl, Type, TypeLoc, SourceRange()); //FIXME: proper loc, and range
  CurContext->addDecl(Decl);

  auto Result = ExternalStmt::Create(C, IDLoc, IDInfo, StmtLabel);
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

  auto Result = IntrinsicStmt::Create(C, IDLoc, IDInfo, StmtLabel);
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

    Expr *visitLeaf(Expr *E,int depth = 0) {
      if(auto Unresolved = dyn_cast<UnresolvedIdentifierExpr>(E)) {
        auto IDInfo = Unresolved->getIdentifier();
        if(auto Declaration = CurScope->Resolve(IDInfo)) {
          if(depth) {
            Diags.Report(E->getLocation(),diag::err_expected_integer_constant_expr)
              << E->getSourceRange();
            HasErrors = true;
          } else {
            // an implied do variable
            auto VD = dyn_cast<VarDecl>(Declaration);
            assert(VD);
            return VarExpr::Create(C, Unresolved->getLocation(), VD);
          }
        } else {
           if(auto OuterDeclaration = S.ResolveIdentifier(IDInfo)) {
            if(auto VD = dyn_cast<VarDecl>(OuterDeclaration)) {
              // a constant variable
              if(VD->isParameter())
                return VarExpr::Create(C, E->getLocation(), VD);
            }
            Diags.Report(E->getLocation(),diag::err_implied_do_expect_leaf_expr)
              << E->getSourceRange();
          } else
            Diags.Report(E->getLocation(), diag::err_undeclared_var_use)
              << IDInfo << E->getSourceRange();
          HasErrors = true;
        }
      }
      else if(auto Unary = dyn_cast<UnaryExpr>(E)) {
        return UnaryExpr::Create(C, Unary->getLocation(),
                                 Unary->getOperator(),
                                 visitLeaf(Unary->getExpression(), depth+1));
      }
      else if(auto Binary = dyn_cast<BinaryExpr>(E)) {
        return BinaryExpr::Create(C, Binary->getLocation(),
                                  Binary->getOperator(),
                                  Binary->getType(),
                                  visitLeaf(Binary->getLHS(), depth+1),
                                  visitLeaf(Binary->getRHS(), depth+1));
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
  if(!isa<VarExpr>(LHS.get()) && !isa<ArrayElementExpr>(LHS.get()) &&
     !isa<SubstringExpr>(LHS.get()) && !isa<ReturnedValueExpr>(LHS.get())) {
    Diags.Report(Loc,diag::err_expr_not_assignable) << LHS.get()->getSourceRange();
    return StmtError();
  }
  if(LHS.get()->getType().isNull() ||
     RHS.get()->getType().isNull())
    return StmtError();
  RHS = TypecheckAssignment(LHS.get()->getType(), RHS,
                            Loc, LHS.get()->getLocStart());
  if(RHS.isInvalid()) return StmtError();

  auto Result = AssignmentStmt::Create(C, Loc, LHS.take(), RHS.take(), StmtLabel);
  getCurrentBody()->Append(Result);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

QualType Sema::ActOnArraySpec(ASTContext &C, QualType ElemTy,
                              ArrayRef<ArraySpec*> Dims) {
  for(size_t I = 0; I < Dims.size(); ++I) {
    auto Shape = Dims[I];
    if(auto Explicit = dyn_cast<ExplicitShapeSpec>(Shape)) {
      CheckArrayBoundValue(Explicit->getUpperBound());
      auto Lower = Explicit->getLowerBound();
      if(Lower) {
        CheckArrayBoundValue(Lower);
        // FIXME: check lower bound <= upper bound
      }
    } else {
      auto Implied = cast<ImpliedShapeSpec>(Shape);
      if(I != (Dims.size() - 1)) {
        // Implied spec must always be last.
        Diags.Report(Implied->getLocation(),
                     diag::err_array_implied_shape_must_be_last);
      }
      if(Implied->getLowerBound())
        CheckArrayBoundValue(Implied->getLowerBound());
    }
  }

  return QualType(ArrayType::Create(C, ElemTy, Dims), 0);
}

// FIXME: what about the case of SUBROUTINE F(X,A) { REAL A(X) }
bool Sema::CheckArrayBoundValue(Expr *E) {
  // Make sure it's an integer expression
  auto Type = E->getType();
  if(!Type.isNull() && !Type->isIntegerType()) {
    Diags.Report(E->getLocation(), diag::err_expected_integer_constant_expr)
      << E->getSourceRange();
    return false;
  }

  if(auto VE = dyn_cast<VarExpr>(E)) {
    if(VE->getVarDecl()->isArgument()) {
      if(Type.isNull()) {
        Diags.Report(E->getLocation(), diag::err_expected_integer_constant_expr)
          << E->getSourceRange();
        return false;
      }
      return true;
    }
  }

  // Make sure the value is a constant expression.s
  if(!E->isEvaluatable(Context)) {
    Diags.Report(E->getLocation(), diag::err_expected_integer_constant_expr)
      << E->getSourceRange();
    return false;
  }
  return true;
}

bool Sema::CheckArrayTypeDeclarationCompability(const ArrayType *T, VarDecl *VD) {
  if(VD->isParameter())
    return false;
  for(auto I = T->begin(); I != T->end(); ++I) {
    auto Shape = *I;
    if(auto Explicit = dyn_cast<ExplicitShapeSpec>(Shape)) {
    } else {
      // implied
      auto Implied = cast<ImpliedShapeSpec>(Shape);
      if(!VD->isArgument()) {
        Diags.Report(Implied->getLocation(), diag::err_array_implied_shape_incompatible)
          << VD->getIdentifier();
        Diags.Report(VD->getLocation(), diag::note_declared_at)
          << VD->getSourceRange();
        return false;
      }
    }
  }
  return true;
}

bool Sema::CheckCharacterLengthSpec(const Expr *E) {
  auto Type = E->getType();
  if(!Type.isNull() && !Type->isIntegerType()) {
    Diags.Report(E->getLocation(), diag::err_expected_integer_constant_expr)
      << E->getSourceRange();
    return false;
  }

  // Make sure the value is a constant expression.
  if(!E->isEvaluatable(Context)) {
    Diags.Report(E->getLocation(), diag::err_expected_integer_constant_expr)
      << E->getSourceRange();
    return false;
  }
  return true;
}

bool Sema::CheckCharacterLengthDeclarationCompability(QualType T, VarDecl *VD) {
  auto Ext = T.getExtQualsPtrOrNull();
  if(Ext) {
    if(Ext->isStarLengthSelector() && !VD->isArgument()) {
      Diags.Report(VD->getLocation(), diag::err_char_star_length_incompatible)
        << (VD->isParameter()? 1 : 0) << VD->getIdentifier()
        << VD->getSourceRange();
    }
  }
  return true;
}

static void ResolveAssignStmtLabel(DiagnosticsEngine &Diags,
                                   const StmtLabelScope::ForwardDecl &Self,
                                   Stmt *Destination) {
  cast<AssignStmt>(Self.Statement)->setAddress(StmtLabelReference(Destination));
}

StmtResult Sema::ActOnAssignStmt(ASTContext &C, SourceLocation Loc,
                                 ExprResult Value, VarExpr* VarRef,
                                 Expr *StmtLabel) {
  Stmt *Result;
  auto Decl = getCurrentStmtLabelScope()->Resolve(Value.get());
  if(!Decl) {
    Result = AssignStmt::Create(C, Loc, StmtLabelReference(),VarRef, StmtLabel);
    getCurrentStmtLabelScope()->DeclareForwardReference(
      StmtLabelScope::ForwardDecl(Value.get(), Result,
                                  ResolveAssignStmtLabel));
  } else
    Result = AssignStmt::Create(C, Loc, StmtLabelReference(Decl),
                                VarRef, StmtLabel);

  getCurrentBody()->Append(Result);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

static void ResolveAssignedGotoStmtLabel(DiagnosticsEngine &Diags,
                                         const StmtLabelScope::ForwardDecl &Self,
                                         Stmt *Destination) {
  cast<AssignedGotoStmt>(Self.Statement)->
    setAllowedValue(Self.ResolveCallbackData,StmtLabelReference(Destination));
}

StmtResult Sema::ActOnAssignedGotoStmt(ASTContext &C, SourceLocation Loc,
                                       VarExpr* VarRef,
                                       ArrayRef<ExprResult> AllowedValues,
                                       Expr *StmtLabel) {
  SmallVector<StmtLabelReference, 4> AllowedLabels(AllowedValues.size());
  for(size_t I = 0; I < AllowedValues.size(); ++I) {
    auto Decl = getCurrentStmtLabelScope()->Resolve(AllowedValues[I].get());
    AllowedLabels[I] = Decl? StmtLabelReference(Decl): StmtLabelReference();
  }
  auto Result = AssignedGotoStmt::Create(C, Loc, VarRef, AllowedLabels, StmtLabel);

  for(size_t I = 0; I < AllowedValues.size(); ++I) {
    if(!AllowedLabels[I].Statement) {
      getCurrentStmtLabelScope()->DeclareForwardReference(
        StmtLabelScope::ForwardDecl(AllowedValues[I].get(), Result,
                                             ResolveAssignedGotoStmtLabel, I));
    }
  }

  getCurrentBody()->Append(Result);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

static void ResolveGotoStmtLabel(DiagnosticsEngine &Diags,
                                 const StmtLabelScope::ForwardDecl &Self,
                                 Stmt *Destination) {
  cast<GotoStmt>(Self.Statement)->setDestination(StmtLabelReference(Destination));
}

StmtResult Sema::ActOnGotoStmt(ASTContext &C, SourceLocation Loc,
                               ExprResult Destination, Expr *StmtLabel) {
  Stmt *Result;
  auto Decl = getCurrentStmtLabelScope()->Resolve(Destination.get());
  if(!Decl) {
    Result = GotoStmt::Create(C, Loc, StmtLabelReference(), StmtLabel);
    getCurrentStmtLabelScope()->DeclareForwardReference(
      StmtLabelScope::ForwardDecl(Destination.get(), Result,
                                  ResolveGotoStmtLabel));
  } else
    Result = GotoStmt::Create(C, Loc, StmtLabelReference(Decl), StmtLabel);

  getCurrentBody()->Append(Result);
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
  getCurrentBody()->Append(Result);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  getCurrentBody()->Enter(Result);
  return Result;
}

StmtResult Sema::ActOnElseIfStmt(ASTContext &C, SourceLocation Loc,
                                 ExprResult Condition, Expr *StmtLabel) {
  if(!getCurrentBody()->HasEntered() ||
     !getCurrentBody()->LastEntered().is(Stmt::IfStmtClass)) {
    if(getCurrentBody()->HasEntered(Stmt::IfStmtClass))
      ReportUnterminatedStmt(Diags, getCurrentBody()->LastEntered(), Loc);
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
  auto ParentIf = static_cast<IfStmt*>(getCurrentBody()->LastEntered().Statement);
  getCurrentBody()->Leave(C);
  ParentIf->setElseStmt(Result);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  getCurrentBody()->Enter(Result);
  return Result;
}

StmtResult Sema::ActOnElseStmt(ASTContext &C, SourceLocation Loc, Expr *StmtLabel) {
  if(!getCurrentBody()->HasEntered() ||
     !getCurrentBody()->LastEntered().is(Stmt::IfStmtClass)) {
    if(getCurrentBody()->HasEntered(Stmt::IfStmtClass))
      ReportUnterminatedStmt(Diags, getCurrentBody()->LastEntered(), Loc);
    else
      Diags.Report(Loc, diag::err_stmt_not_in_if) << "ELSE";
    return StmtError();
  }
  auto Result = ConstructPartStmt::Create(C, ConstructPartStmt::ElseStmtClass, Loc, nullptr, StmtLabel);
  getCurrentBody()->Append(Result);
  getCurrentBody()->LeaveIfThen(C);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

StmtResult Sema::ActOnEndIfStmt(ASTContext &C, SourceLocation Loc, Expr *StmtLabel) {
  // Report begin .. end mismatch
  if(!getCurrentBody()->HasEntered() ||
     !getCurrentBody()->LastEntered().is(Stmt::IfStmtClass)) {
    if(getCurrentBody()->HasEntered(Stmt::IfStmtClass))
      ReportUnterminatedStmt(Diags, getCurrentBody()->LastEntered(), Loc);
    else
      Diags.Report(Loc, diag::err_stmt_not_in_if) << "END IF";
    return StmtError();
  }

  auto Result = ConstructPartStmt::Create(C, ConstructPartStmt::EndIfStmtClass, Loc, nullptr, StmtLabel);
  getCurrentBody()->Append(Result);
  getCurrentBody()->Leave(C);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

static void ResolveDoStmtLabel(DiagnosticsEngine &Diags,
                               const StmtLabelScope::ForwardDecl &Self,
                               Stmt *Destination) {
  cast<DoStmt>(Self.Statement)->setTerminatingStmt(StmtLabelReference(Destination));
}

static int ExpectRealOrIntegerOrDoublePrec(DiagnosticsEngine &Diags, const Expr *E,
                                           unsigned DiagType = diag::err_typecheck_expected_do_expr) {
  auto T = E->getType();
  if(T->isIntegerType() || T->isRealType()) return 0;
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
  switch(S->getStmtClass()) {
  case Stmt::DoStmtClass: case Stmt::IfStmtClass: case Stmt::DoWhileStmtClass:
  case Stmt::ConstructPartStmtClass:
    return false;
  default:
    return true;
  }
}

static bool IsValidDoTerminatingStatement(Stmt *S) {
  switch(S->getStmtClass()) {
  case Stmt::GotoStmtClass: case Stmt::AssignedGotoStmtClass:
  case Stmt::StopStmtClass: case Stmt::DoStmtClass:
  case Stmt::DoWhileStmtClass:
  case Stmt::ConstructPartStmtClass:
    return false;
  case Stmt::IfStmtClass: {
    auto NextStmt = static_cast<IfStmt*>(S)->getThenStmt();
    return NextStmt && IsValidDoLogicalIfThenStatement(NextStmt);
  }
  default:
    return true;
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
  auto DoVarType = DoVar->getType();
  E1 = TypecheckAssignment(DoVarType, E1);
  E2 = TypecheckAssignment(DoVarType, E2);
  if(E3.isUsable())
    E3 = TypecheckAssignment(DoVarType, E3);

  // Make sure the statement label isn't already declared
  if(TerminatingStmt.get()) {
    if(auto Decl = getCurrentStmtLabelScope()->Resolve(TerminatingStmt.get())) {
      std::string String;
      llvm::raw_string_ostream Stream(String);
      TerminatingStmt.get()->dump(Stream);
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
  getCurrentBody()->Append(Result);
  if(TerminatingStmt.get())
    getCurrentStmtLabelScope()->DeclareForwardReference(
      StmtLabelScope::ForwardDecl(TerminatingStmt.get(), Result,
                                           ResolveDoStmtLabel));
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  if(TerminatingStmt.get())
    getCurrentBody()->Enter(BlockStmtBuilder::Entry(
                             Result,TerminatingStmt.get()));
  else getCurrentBody()->Enter(Result);
  return Result;
}

/// FIXME: Fortran 90+: make multiple do end at one label obsolete
void Sema::CheckStatementLabelEndDo(Expr *StmtLabel, Stmt *S) {
  if(!getCurrentBody()->HasEntered())
    return;

  auto I = getCurrentBody()->ControlFlowStack.size();
  size_t LastUnterminated = 0;
  do {
    I--;
    if(!isa<DoStmt>(getCurrentBody()->ControlFlowStack[I].Statement)) {
      if(!LastUnterminated) LastUnterminated = I;
    } else {
      auto ParentDo = cast<DoStmt>(getCurrentBody()->ControlFlowStack[I].Statement);
      auto ParentDoExpectedLabel = getCurrentBody()->ControlFlowStack[I].ExpectedEndDoLabel;
      if(!ParentDoExpectedLabel) {
        if(!LastUnterminated) LastUnterminated = I;
      } else {
        if(getCurrentStmtLabelScope()->IsSame(ParentDoExpectedLabel, StmtLabel)) {
          // END DO
          getCurrentStmtLabelScope()->RemoveForwardReference(ParentDo);
          if(!IsValidDoTerminatingStatement(S)) {
            Diags.Report(S->getLocation(),
                         diag::err_invalid_do_terminating_stmt);
          }
          ParentDo->setTerminatingStmt(StmtLabelReference(S));
          if(!LastUnterminated)
            getCurrentBody()->Leave(Context);
          else
            ReportUnterminatedStmt(Diags,
                                   getCurrentBody()->ControlFlowStack[LastUnterminated],
                                   S->getLocation());
        } else if(!LastUnterminated) LastUnterminated = I;
      }
    }
  } while(I>0);
}

StmtResult Sema::ActOnDoWhileStmt(ASTContext &C, SourceLocation Loc, ExprResult Condition,
                                  Expr *StmtLabel) {
  if(!IsLogicalExpression(Condition)) {
    ReportExpectedLogical(Diags, Condition);
  }

  auto Result = DoWhileStmt::Create(C, Loc, Condition.take(), StmtLabel);
  getCurrentBody()->Append(Result);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  getCurrentBody()->Enter(Result);
  return Result;
}

StmtResult Sema::ActOnEndDoStmt(ASTContext &C, SourceLocation Loc, Expr *StmtLabel) {
  // Report begin .. end mismatch
  if(!getCurrentBody()->HasEntered() ||
     (!isa<DoStmt>(getCurrentBody()->LastEntered().Statement) &&
      !isa<DoWhileStmt>(getCurrentBody()->LastEntered().Statement))) {
    bool HasMatchingDo = false;
    for(auto I : getCurrentBody()->ControlFlowStack) {
      if(isa<DoWhileStmt>(I.Statement) ||
        (isa<DoStmt>(I.Statement) && !I.ExpectedEndDoLabel)) {
        HasMatchingDo = true;
        break;
      }
    }
    if(!HasMatchingDo)
      Diags.Report(Loc, diag::err_end_do_without_do);
    else ReportUnterminatedStmt(Diags, getCurrentBody()->LastEntered(), Loc);
    return StmtError();
  }

  auto Last = getCurrentBody()->LastEntered();

  if(isa<DoStmt>(Last.Statement)) {
    // If last loop was a DO with terminating label, we expect it to finish before this loop
    if(getCurrentBody()->LastEntered().ExpectedEndDoLabel) {
      ReportUnterminatedLabeledDoStmt(Diags, getCurrentBody()->LastEntered(), Loc);
      return StmtError();
    }
  }

  auto Result = ConstructPartStmt::Create(C, ConstructPartStmt::EndDoStmtClass, Loc, nullptr, StmtLabel);
  getCurrentBody()->Append(Result);
  getCurrentBody()->Leave(C);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

StmtResult Sema::ActOnContinueStmt(ASTContext &C, SourceLocation Loc, Expr *StmtLabel) {
  auto Result = ContinueStmt::Create(C, Loc, StmtLabel);
  getCurrentBody()->Append(Result);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

StmtResult Sema::ActOnStopStmt(ASTContext &C, SourceLocation Loc, ExprResult StopCode, Expr *StmtLabel) {
  auto Result = StopStmt::Create(C, Loc, StopCode.take(), StmtLabel);
  getCurrentBody()->Append(Result);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

StmtResult Sema::ActOnReturnStmt(ASTContext &C, SourceLocation Loc, ExprResult E, Expr *StmtLabel) {
  if(!IsInsideFunctionOrSubroutine()) {
    Diags.Report(Loc, diag::err_stmt_not_in_func) << "RETURN";
    return StmtError();
  }
  auto Result = ReturnStmt::Create(C, Loc, E.take(), StmtLabel);
  getCurrentBody()->Append(Result);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

StmtResult Sema::ActOnCallStmt(ASTContext &C, SourceLocation Loc, FunctionDecl *Function,
                               ArrayRef<ExprResult> Arguments, Expr *StmtLabel) {
  SmallVector<Expr*, 8> Args(Arguments.size());
  for(size_t I = 0; I < Arguments.size(); ++I)
    Args[I] = Arguments[I].take();

  CheckCallArgumentCount(Function, Args, Loc);

  auto Result = CallStmt::Create(C, Loc, Function, Args, StmtLabel);
  getCurrentBody()->Append(Result);
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
