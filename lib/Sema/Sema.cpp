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
#include "flang/Sema/SemaInternal.h"
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
    CurStmtLabelScope(nullptr),
    CurNamedConstructs(nullptr),
    CurImplicitTypingScope(nullptr) {
}

Sema::~Sema() {}

// FIXME
SourceRange Sema::getIdentifierRange(SourceLocation Loc, const IdentifierInfo *IDInfo) {
  return SourceRange(Loc, SourceLocation::getFromPointer(Loc.getPointer()+IDInfo->getLength()));
}

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

  // Enter new construct name scope
  Scope.NamedConstructs.setParent(CurNamedConstructs);
  CurNamedConstructs = &Scope.NamedConstructs;

  // Enter new implicit typing scope
  Scope.ImplicitTypingRules.setParent(CurImplicitTypingScope);
  CurImplicitTypingScope = &Scope.ImplicitTypingRules;

  CurExecutableStmts = &Scope.Body;
}

void Sema::PopExecutableProgramUnit(SourceLocation Loc) {

  // Fix the forward statement label references
  auto StmtLabelForwardDecls = CurStmtLabelScope->getForwardDecls();
  StmtLabelResolver Resolver(*this, Diags);
  for(size_t I = 0; I < StmtLabelForwardDecls.size(); ++I) {    
    if(auto Decl = CurStmtLabelScope->Resolve(StmtLabelForwardDecls[I].StmtLabel))
      Resolver.ResolveForwardUsage(StmtLabelForwardDecls[I], Decl);
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
  for(auto I = CurStmtLabelScope->decl_begin();
      I != CurStmtLabelScope->decl_end(); ++I) {
    auto Stmt = I->second;
    if(!Stmt->isStmtLabelUsed()) {
      auto StmtLabel = Stmt->getStmtLabel();
      Diags.Report(StmtLabel->getLocation(),
                   diag::warn_unused_stmt_label)
       << I->first << StmtLabel->getSourceRange();
    }
  }

  // Restore the previous statement labels
  CurStmtLabelScope = CurStmtLabelScope->getParent();

  // Restore the previous named constructs
  CurNamedConstructs = CurNamedConstructs->getParent();

  // Report unterminated statements.
  if(CurExecutableStmts->HasEntered()) {
    // If do ends with statement label, the error
    // was already reported as undeclared label use.
    ReportUnterminatedStmt(CurExecutableStmts->LastEntered(), Loc, false);
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

void Sema::DeclareConstructName(ConstructName Name, NamedConstructStmt *S) {
  if(auto Construct = CurNamedConstructs->Resolve(Name.IDInfo)) {
    Diags.Report(Name.Loc,
                 diag::err_redefinition_of_construct_name)
      << Name.IDInfo;
    Diags.Report(Construct->getName().Loc,
                 diag::note_previous_definition)
     << SourceRange(Construct->getName().Loc, Construct->getLocation());
  } else
    CurNamedConstructs->Declare(Name.IDInfo, S);
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

VarDecl *Sema::ActOnStatementFunctionArgument(ASTContext &C, SourceLocation IDLoc,
                                              const IdentifierInfo *IDInfo) {
  if (auto Prev = LookupIdentifier(IDInfo)) {
    Diags.Report(IDLoc, diag::err_redefinition) << IDInfo;
    Diags.Report(Prev->getLocation(), diag::note_previous_definition);
    return nullptr;
  }
  QualType Type;
  if(auto Prev = ResolveIdentifier(IDInfo)) {
     if(auto VD = dyn_cast<VarDecl>(Prev))
       Type = VD->getType();
  }

  VarDecl *VD = VarDecl::CreateArgument(C, CurContext, IDLoc, IDInfo);
  if(!Type.isNull())
    VD->setType(Type);
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

bool Sema::IsValidStatementFunctionIdentifier(const IdentifierInfo *IDInfo) {
  if (auto Prev = LookupIdentifier(IDInfo)) {
    if(auto VD = dyn_cast<VarDecl>(Prev))
      return VD->isUnusedSymbol();
    return false;
  }
  return true;
}

FunctionDecl *Sema::ActOnStatementFunction(ASTContext &C,
                                           SourceLocation IDLoc,
                                           const IdentifierInfo *IDInfo) {
  bool Declare = true;
  QualType ReturnType;
  if (auto Prev = LookupIdentifier(IDInfo)) {
    auto VD = dyn_cast<VarDecl>(Prev);
    if(!VD || !VD->isUnusedSymbol()) {
      Diags.Report(IDLoc, diag::err_redefinition) << IDInfo;
      Diags.Report(Prev->getLocation(), diag::note_previous_definition);
      Declare = false;
    } else {
      ReturnType = VD->getType();
      CurContext->removeDecl(Prev);
    }
  }

  DeclarationNameInfo NameInfo(IDInfo, IDLoc);
  auto ParentDC = CurContext;

  auto Func = FunctionDecl::Create(C, FunctionDecl::StatementFunction,
                                   ParentDC, NameInfo, ReturnType);
  if(Declare)
    ParentDC->addDecl(Func);
  PushDeclContext(Func);
  return Func;
}

void Sema::ActOnStatementFunctionBody(SourceLocation Loc, ExprResult Body) {
  auto Func = CurrentContextAsFunction();
  auto Type = Func->getType();
  Body = TypecheckAssignment(Type, Body, Loc, Loc);
  CurrentContextAsFunction()->setBody(Body.get());
}

void Sema::ActOnEndStatementFunction(ASTContext &C) {
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
  if(DS.isDoublePrecision()) {
    assert(!DS.getKindSelector());
    Kind = BuiltinType::Real8;
  } else if(DS.hasKindSelector())
    Kind = EvalAndCheckTypeKind(Result, DS.getKindSelector());

  unsigned LengthSelector = 0;
  if(DS.hasLengthSelector() && !DS.isStarLengthSelector())
    LengthSelector = EvalAndCheckCharacterLength(DS.getLengthSelector());

  Result = C.getExtQualType(TypeNode, Quals, Kind,
                            DS.isDoublePrecision(),
                            DS.isStarLengthSelector(),
                            LengthSelector);

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
      VD->MarkUsedAsVariable(IDLoc);
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


Decl *Sema::ActOnImplicitFunctionDecl(ASTContext &C, SourceLocation IDLoc,
                                      const IdentifierInfo *IDInfo) {
  auto FuncResult = IntrinsicFunctionMapping.Resolve(IDInfo);
  if(!FuncResult.IsInvalid) {
    auto Func = IntrinsicFunctionDecl::Create(C, CurContext, IDLoc, IDInfo,
                                              C.IntegerTy, FuncResult.Function);
    CurContext->addDecl(Func);
    return Func;
  }

  auto Type = ResolveImplicitType(IDInfo);
  if(Type.isNull()) {
    Diags.Report(IDLoc, diag::err_undeclared_var_use)
      << IDInfo;
    return nullptr;
  }

  auto Func = FunctionDecl::Create(C, FunctionDecl::External, CurContext,
                                   DeclarationNameInfo(IDInfo, IDLoc), Type);
  return Func;
}

Decl *Sema::ActOnPossibleImplicitFunctionDecl(ASTContext &C, SourceLocation IDLoc,
                                              const IdentifierInfo *IDInfo,
                                              Decl *PrevDecl) {
  if(PrevDecl->getDeclContext() == CurContext) {
    if(auto VD = dyn_cast<VarDecl>(PrevDecl)) {
      if(VD->isUnusedSymbol()) {
        auto VarType = VD->getType();
        // NB: AMBIGUITY - return the variable as it is probably
        // an array access or character substring expression.
        if(VarType->isArrayType() || VarType->isCharacterType())
          return PrevDecl;

        // FIXME: what about intrinsic?
        auto Func = FunctionDecl::Create(C, FunctionDecl::External, CurContext,
                                         DeclarationNameInfo(IDInfo, IDLoc), VarType);
        CurContext->removeDecl(VD);
        CurContext->addDecl(Func);
        return Func;
      }
    }
  }
  return PrevDecl;
}

bool Sema::ApplyImplicitRulesToArgument(VarDecl *Arg, SourceRange Range) {
  auto Type = ResolveImplicitType(Arg->getIdentifier());
  if(Type.isNull()) {
    Diags.Report(Range.isValid()? Range.Start : Arg->getLocation(),
                 diag::err_arg_no_implicit_type)
     << (Range.isValid()? Range : Arg->getSourceRange())
     << Arg->getIdentifier();
    Arg->setType(Context.RealTy); //Prevent further errors
    return false;
  }
  Arg->setType(Type);
  return true;
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

VarDecl *Sema::ExpectVarRefOrDeclImplicitVar(SourceLocation IDLoc,
                                             const IdentifierInfo *IDInfo) {
  auto Result = ResolveIdentifier(IDInfo);
  if(Result){
    if(auto VD = dyn_cast<VarDecl>(Result))
      return VD;
    Diags.Report(IDLoc, diag::err_expected_var_ref);
    Diags.Report(Result->getLocation(), diag::note_previous_definition);
    return nullptr;
  }

  Result = ActOnImplicitEntityDecl(Context, IDLoc, IDInfo);
  if(Result)
    return cast<VarDecl>(Result);
  return nullptr;
}

VarDecl *Sema::ExpectVarRef(SourceLocation IDLoc,
                            const IdentifierInfo *IDInfo) {
  auto Result = ResolveIdentifier(IDInfo);
  if(Result) {
    if(auto VD = dyn_cast<VarDecl>(Result))
      return VD;
    Diags.Report(IDLoc, diag::err_expected_var_ref);
    Diags.Report(Result->getLocation(), diag::note_previous_definition);
  }
  Diags.Report(IDLoc, diag::err_undeclared_var_use)
    << IDInfo;
  return nullptr;
}

StmtResult Sema::ActOnCompoundStmt(ASTContext &C, SourceLocation Loc,
                                   ArrayRef<Stmt*> Body, Expr *StmtLabel) {
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
    if(VD && VD->isUnusedSymbol()) {
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

StmtResult Sema::ActOnSAVE(ASTContext &C, SourceLocation Loc, Expr *StmtLabel) {
  auto Result = SaveStmt::Create(C, Loc, nullptr, StmtLabel);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

StmtResult Sema::ActOnSAVE(ASTContext &C, SourceLocation Loc,
                           SourceLocation IDLoc,
                           const IdentifierInfo *IDInfo,
                           Expr *StmtLabel) {
  auto Result = SaveStmt::Create(C, IDLoc, IDInfo, StmtLabel);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
}

bool Sema::CheckEquivalenceObject(SourceLocation Loc, Expr *E) {
  if(auto Var = dyn_cast<VarExpr>(E)) {
    auto VD = Var->getVarDecl();
    if(VD->isArgument() || VD->isParameter()) {
      Diags.Report(Loc, diag::err_spec_requires_local_var)
        << E->getSourceRange();
      Diags.Report(VD->getLocation(), diag::note_previous_definition_kind)
          << VD->getIdentifier() << (VD->isArgument()? 0 : 1)
          << getIdentifierRange(VD->getLocation(), VD->getIdentifier());
      return true;
    }
    if(VD->isUnusedSymbol())
      const_cast<VarDecl*>(VD)->MarkUsedAsVariable(E->getLocation());

  }  else {
    Diags.Report(Loc, diag::err_spec_requires_var_or_arr_el)
      << E->getSourceRange();
    return true;
  }
  return false;
}

// FIXME: add support for derived types.
// FIXME: check default character kind.
bool Sema::CheckEquivalenceType(QualType ExpectedType, const Expr *E) {
  auto ObjectType = E->getType();
  if(ObjectType->isArrayType())
    ObjectType = ObjectType->asArrayType()->getElementType();

  if(ExpectedType->isCharacterType()) {
    if(!ObjectType->isCharacterType()) {
      Diags.Report(E->getLocation(),
                   diag::err_typecheck_expected_char_expr)
        << ObjectType << E->getSourceRange();
      return true;
    }
  } else if(ExpectedType->isBuiltinType()) {
    if(IsDefaultBuiltinOrDoublePrecisionType(ExpectedType)) {
      if(ObjectType->isCharacterType()) {
        Diags.Report(E->getLocation(), diag::err_expected_numeric_or_logical_expr)
          << ObjectType << E->getSourceRange();
        return true;
      }
      if(!IsDefaultBuiltinOrDoublePrecisionType(ObjectType)) {
        Diags.Report(E->getLocation(),
                     diag::err_typecheck_expected_default_kind_expr)
          << ObjectType << E->getSourceRange();
        return true;
      }
    } else {
      if(!CheckTypesSameKind(ExpectedType, ObjectType)) {
        Diags.Report(E->getLocation(), diag::err_typecheck_expected_expr_of_type)
          << ExpectedType << ObjectType
          << E->getSourceRange();
        return true;
      }
    }
  }
  return false;
}

StmtResult Sema::ActOnEQUIVALENCE(ASTContext &C, SourceLocation Loc,
                                  SourceLocation PartLoc,
                                  ArrayRef<Expr*> ObjectList,
                                  Expr *StmtLabel) {
  // expression and type check.
  bool HasErrors = false;
  QualType ObjectType;
  for(auto I : ObjectList) {
    if(auto Arr = dyn_cast<ArrayElementExpr>(I)) {
      if(CheckEquivalenceObject(Loc, Arr->getTarget()))
        HasErrors = true;
      for(auto S : Arr->getSubscriptList()) {
        if(!StatementRequiresConstantExpression(Loc, S))
          HasErrors = true;
      }
    } else if(auto Str = dyn_cast<SubstringExpr>(I)) {
      if(CheckEquivalenceObject(Loc, Str->getTarget()));
        HasErrors = true;
      if(Str->getStartingPoint()) {
        if(!StatementRequiresConstantExpression(Loc, Str->getStartingPoint()))
          HasErrors = true;
      }
      if(Str->getEndPoint()) {
        if(!StatementRequiresConstantExpression(Loc, Str->getEndPoint()))
          HasErrors = true;
      }
    } else if(CheckEquivalenceObject(Loc, I))
      HasErrors = true;

    if(ObjectType.isNull()) {
      ObjectType = I->getType();
      if(ObjectType->isArrayType())
        ObjectType = ObjectType->asArrayType()->getElementType();
    } else {
      if(CheckEquivalenceType(ObjectType, I))
        HasErrors = true;
    }
  }

  auto Result = EquivalenceStmt::Create(C, PartLoc, ObjectList, StmtLabel);
  if(StmtLabel) DeclareStatementLabel(StmtLabel, Result);
  return Result;
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

    Expr *visitLeaf(Expr *E, int depth = 0) {
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
  if(auto Var = dyn_cast<VarExpr>(LHS.get()))
    CheckVarIsAssignable(Var);
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

bool Sema::CheckArrayBoundValue(Expr *E) {
  if(CheckArgumentDependentEvaluatableIntegerExpression(E))
    return true;

  auto Type = E->getType();

  // Make sure it's an integer expression
  if(!Type.isNull() && !Type->isIntegerType()) {
    Diags.Report(E->getLocation(), diag::err_expected_integer_constant_expr)
      << E->getSourceRange();
    return false;
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
