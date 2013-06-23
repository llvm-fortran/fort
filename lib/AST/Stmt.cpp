//===--- Stmt.cpp - Fortran Statements ------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements the statement objects.
//
//===----------------------------------------------------------------------===//

#include "flang/AST/Stmt.h"
#include "flang/AST/ASTContext.h"
#include "flang/Basic/IdentifierTable.h"
#include "llvm/ADT/StringRef.h"

namespace flang {

//===----------------------------------------------------------------------===//
// Statement Base Class
//===----------------------------------------------------------------------===//

Stmt::~Stmt() {}

Stmt *Stmt::Create(ASTContext &C, StmtTy StmtType, SourceLocation Loc, Expr *StmtLabel) {
  return new(C) Stmt(StmtType, Loc, StmtLabel);
}

//===----------------------------------------------------------------------===//
// Bundled Compound Statement
//===----------------------------------------------------------------------===//

BundledCompoundStmt::BundledCompoundStmt(ASTContext &C, SourceLocation Loc,
                                         ArrayRef<Stmt*> Body, Expr *StmtLabel)
  : ListStmt(C, BundledCompound, Loc, Body, StmtLabel) {
}

BundledCompoundStmt *BundledCompoundStmt::Create(ASTContext &C, SourceLocation Loc,
                                                 ArrayRef<Stmt*> Body,
                                                 Expr *StmtLabel) {
  return new(C) BundledCompoundStmt(C, Loc, Body, StmtLabel);
}

//===----------------------------------------------------------------------===//
// Program Statement
//===----------------------------------------------------------------------===//

ProgramStmt *ProgramStmt::Create(ASTContext &C, const IdentifierInfo *ProgName,
                                 SourceLocation Loc, SourceLocation NameLoc,
                                 Expr *StmtLabel) {
  return new (C) ProgramStmt(ProgName, Loc, NameLoc, StmtLabel);
}

//===----------------------------------------------------------------------===//
// EndProgram Statement
//===----------------------------------------------------------------------===//

EndProgramStmt *EndProgramStmt::Create(ASTContext &C,
                                       const IdentifierInfo *ProgName,
                                       SourceLocation Loc, SourceLocation NameLoc,
                                       Expr *StmtLabel) {
  return new (C) EndProgramStmt(ProgName, Loc, NameLoc, StmtLabel);
}

//===----------------------------------------------------------------------===//
// Use Statement
//===----------------------------------------------------------------------===//

UseStmt::UseStmt(ASTContext &C, ModuleNature MN, const IdentifierInfo *modName,
                 ArrayRef<RenamePair> RenameList, Expr *StmtLabel)
  : ListStmt(C, Use, SourceLocation(), RenameList, StmtLabel),
    ModNature(MN), ModName(modName), Only(false) {}

UseStmt *UseStmt::Create(ASTContext &C, ModuleNature MN,
                         const IdentifierInfo *modName,
                         Expr *StmtLabel) {
  return  new (C) UseStmt(C, MN, modName, ArrayRef<RenamePair>(), StmtLabel);
}

UseStmt *UseStmt::Create(ASTContext &C, ModuleNature MN,
                         const IdentifierInfo *modName, bool Only,
                         ArrayRef<RenamePair> RenameList,
                         Expr *StmtLabel) {
  UseStmt *US = new (C) UseStmt(C, MN, modName, RenameList, StmtLabel);
  US->Only = Only;
  return US;
}

llvm::StringRef UseStmt::getModuleName() const {
  return ModName->getName();
}

//===----------------------------------------------------------------------===//
// Import Statement
//===----------------------------------------------------------------------===//

ImportStmt::ImportStmt(ASTContext &C, SourceLocation Loc,
                       ArrayRef<const IdentifierInfo*> Names,
                       Expr *StmtLabel)
  : ListStmt(C, Import, Loc, Names, StmtLabel) {}

ImportStmt *ImportStmt::Create(ASTContext &C, SourceLocation Loc,
                               ArrayRef<const IdentifierInfo*> Names,
                               Expr *StmtLabel) {
  return new (C) ImportStmt(C, Loc, Names, StmtLabel);
}

//===----------------------------------------------------------------------===//
// Implicit Statement
//===----------------------------------------------------------------------===//

ImplicitStmt::ImplicitStmt(SourceLocation Loc, Expr *StmtLabel)
  : Stmt(Implicit, Loc, StmtLabel), None(true),
    LetterSpec(LetterSpecTy(nullptr,nullptr)) {
}

ImplicitStmt::ImplicitStmt(SourceLocation Loc, QualType T,
                           LetterSpecTy Spec,
                           Expr *StmtLabel)
  : Stmt(Implicit, Loc, StmtLabel), Ty(T), None(false),
    LetterSpec(Spec) {}

ImplicitStmt *ImplicitStmt::Create(ASTContext &C, SourceLocation Loc,
                                   Expr *StmtLabel) {
  return new (C) ImplicitStmt(Loc, StmtLabel);
}

ImplicitStmt *ImplicitStmt::Create(ASTContext &C, SourceLocation Loc, QualType T,
                                   LetterSpecTy LetterSpec,
                                   Expr *StmtLabel) {
  return new (C) ImplicitStmt(Loc, T, LetterSpec, StmtLabel);
}

//===----------------------------------------------------------------------===//
// Parameter Statement
//===----------------------------------------------------------------------===//

ParameterStmt::ParameterStmt(ASTContext &C, SourceLocation Loc,
                             ArrayRef<ParamPair> PList, Expr *StmtLabel)
  : ListStmt(C, Parameter, Loc, PList, StmtLabel) {}

ParameterStmt *ParameterStmt::Create(ASTContext &C, SourceLocation Loc,
                                     ArrayRef<ParamPair>ParamList,
                                     Expr *StmtLabel) {
  return new (C) ParameterStmt(C, Loc, ParamList, StmtLabel);
}

//===----------------------------------------------------------------------===//
// Dimension Statement
//===----------------------------------------------------------------------===//

DimensionStmt::DimensionStmt(ASTContext &C, SourceLocation Loc,
                             const IdentifierInfo* IDInfo,
                             ArrayRef<ArrayType::Dimension> Dims,
                             Expr *StmtLabel)
  : ListStmt(C, Dimension, Loc, Dims, StmtLabel) , VarName(IDInfo) {
}

DimensionStmt *DimensionStmt::Create(ASTContext &C, SourceLocation Loc,
                                     const IdentifierInfo* IDInfo,
                                     ArrayRef<ArrayType::Dimension> Dims,
                                     Expr *StmtLabel) {
  return new (C) DimensionStmt(C, Loc, IDInfo, Dims, StmtLabel);
}

//===----------------------------------------------------------------------===//
// Format Statement
//===----------------------------------------------------------------------===//

FormatStmt::FormatStmt(SourceLocation Loc, FormatSpec *fs, Expr *StmtLabel)
  : Stmt(Format, Loc, StmtLabel), FS(fs) {}

FormatStmt *FormatStmt::Create(ASTContext &C, SourceLocation Loc, FormatSpec *fs,
                               Expr *StmtLabel) {
  return new (C) FormatStmt(Loc, fs, StmtLabel);
}

//===----------------------------------------------------------------------===//
// Entry Statement
//===----------------------------------------------------------------------===//

EntryStmt::EntryStmt(SourceLocation Loc, Expr *StmtLabel)
  : Stmt(Entry, Loc, StmtLabel) {}

EntryStmt *EntryStmt::Create(ASTContext &C, SourceLocation Loc, Expr *StmtLabel) {
  return new (C) EntryStmt(Loc, StmtLabel);
}

//===----------------------------------------------------------------------===//
// Asynchronous Statement
//===----------------------------------------------------------------------===//

AsynchronousStmt::
AsynchronousStmt(ASTContext &C, SourceLocation Loc,
                 ArrayRef<const IdentifierInfo*> objNames,
                 Expr *StmtLabel)
  : ListStmt(C, Asynchronous, Loc, objNames, StmtLabel) {}

AsynchronousStmt *AsynchronousStmt::
Create(ASTContext &C, SourceLocation Loc, ArrayRef<const IdentifierInfo*> objNames,
       Expr *StmtLabel) {
  return new (C) AsynchronousStmt(C, Loc, objNames, StmtLabel);
}

//===----------------------------------------------------------------------===//
// External Statement
//===----------------------------------------------------------------------===//

ExternalStmt::ExternalStmt(ASTContext &C, SourceLocation Loc,
                           ArrayRef<const IdentifierInfo *> ExternalNames,
                           Expr *StmtLabel)
  : ListStmt(C, External, Loc, ExternalNames, StmtLabel) {}

ExternalStmt *ExternalStmt::Create(ASTContext &C, SourceLocation Loc,
                                   ArrayRef<const IdentifierInfo*>ExternalNames,
                                   Expr *StmtLabel) {
  return new (C) ExternalStmt(C, Loc, ExternalNames, StmtLabel);
}

//===----------------------------------------------------------------------===//
// Intrinsic Statement
//===----------------------------------------------------------------------===//

IntrinsicStmt::IntrinsicStmt(ASTContext &C, SourceLocation Loc,
                           ArrayRef<const IdentifierInfo *> IntrinsicNames,
                           Expr *StmtLabel)
  : ListStmt(C, Intrinsic, Loc, IntrinsicNames, StmtLabel) {}

IntrinsicStmt *IntrinsicStmt::Create(ASTContext &C, SourceLocation Loc,
                                   ArrayRef<const IdentifierInfo*> IntrinsicNames,
                                   Expr *StmtLabel) {
  return new (C) IntrinsicStmt(C, Loc, IntrinsicNames, StmtLabel);
}

//===----------------------------------------------------------------------===//
// Block Statement
//===----------------------------------------------------------------------===//

BlockStmt::BlockStmt(ASTContext &C, SourceLocation Loc,
                     ArrayRef<StmtResult> Body)
  : ListStmt(C, Block, Loc, Body, nullptr) {
}

BlockStmt *BlockStmt::Create(ASTContext &C, SourceLocation Loc,
                             ArrayRef<StmtResult> Body) {
  return new(C) BlockStmt(C, Loc, Body);
}

//===----------------------------------------------------------------------===//
// Assign Statement
//===----------------------------------------------------------------------===//

AssignStmt::AssignStmt(SourceLocation Loc, StmtLabelReference Addr,
                       Expr *Dest, Expr *StmtLabel)
  : Stmt(Assign, Loc, StmtLabel), Address(Addr), Destination(Dest) {
}

AssignStmt *AssignStmt::Create(ASTContext &C, SourceLocation Loc,
                               StmtLabelReference Address,
                               Expr *Destination,
                               Expr *StmtLabel) {
  return new(C) AssignStmt(Loc, Address, Destination, StmtLabel);
}

void AssignStmt::setAddress(StmtLabelReference Address) {
  assert(!this->Address.Statement);
  assert(Address.Statement);
  this->Address = Address;
}

//===----------------------------------------------------------------------===//
// Assigned Goto Statement
//===----------------------------------------------------------------------===//

AssignedGotoStmt::AssignedGotoStmt(ASTContext &C, SourceLocation Loc, Expr *Dest,
                                   ArrayRef<StmtLabelReference> Vals,
                                   Expr *StmtLabel)
  : ListStmt(C, AssignedGoto, Loc, Vals, StmtLabel), Destination(Dest) {
}

AssignedGotoStmt *AssignedGotoStmt::Create(ASTContext &C, SourceLocation Loc,
                                           Expr *Destination,
                                           ArrayRef<StmtLabelReference> AllowedValues,
                                           Expr *StmtLabel) {
  return new(C) AssignedGotoStmt(C, Loc, Destination, AllowedValues, StmtLabel);
}

void AssignedGotoStmt::setAllowedValue(size_t I, StmtLabelReference Address) {
  assert(I < getAllowedValues().size());
  assert(!getAllowedValues()[I].Statement);
  assert(Address.Statement);
  getMutableList()[I] = Address;
}

//===----------------------------------------------------------------------===//
// Goto Statement
//===----------------------------------------------------------------------===//

GotoStmt::GotoStmt(SourceLocation Loc, StmtLabelReference Dest, Expr *StmtLabel)
  : Stmt(Goto, Loc, StmtLabel), Destination(Dest) {
}

GotoStmt *GotoStmt::Create(ASTContext &C, SourceLocation Loc,
                           StmtLabelReference Destination,
                           Expr *StmtLabel) {
  return new(C) GotoStmt(Loc, Destination, StmtLabel);
}

void GotoStmt::setDestination(StmtLabelReference Destination) {
  assert(!this->Destination.Statement);
  assert(Destination.Statement);
  this->Destination = Destination;
}

//===----------------------------------------------------------------------===//
// If Statement
//===----------------------------------------------------------------------===//

IfStmt::IfStmt(SourceLocation Loc, Expr *Cond, Expr *StmtLabel)
  : Stmt(If, Loc, StmtLabel), Condition(Cond),
    ThenArm(nullptr), ElseArm(nullptr)  {
}

IfStmt *IfStmt::Create(ASTContext &C, SourceLocation Loc,
                       Expr *Condition, Expr *StmtLabel) {
  return new(C) IfStmt(Loc, Condition, StmtLabel);
}

void IfStmt::setThenStmt(Stmt *Body) {
  assert(!ThenArm);
  assert(Body);
  ThenArm = Body;
}

void IfStmt::setElseStmt(Stmt *Body) {
  assert(!ElseArm);
  assert(Body);
  ElseArm = Body;
}

//===----------------------------------------------------------------------===//
// Control flow block Statement
//===----------------------------------------------------------------------===//

CFBlockStmt::CFBlockStmt(StmtTy Type, SourceLocation Loc, Expr *StmtLabel)
  : Stmt(Type, Loc, StmtLabel), Body(nullptr) {}

void CFBlockStmt::setBody(Stmt *Body) {
  assert(!this->Body);
  assert(Body);
  this->Body = Body;
}

//===----------------------------------------------------------------------===//
// Do Statement
//===----------------------------------------------------------------------===//

DoStmt::DoStmt(SourceLocation Loc, StmtLabelReference TermStmt,
               Expr *DoVariable, Expr *InitialParam,
               Expr *TerminalParam, Expr *IncrementationParam,
               Expr *StmtLabel)
  : CFBlockStmt(Do, Loc, StmtLabel), TerminatingStmt(TermStmt), DoVar(DoVariable),
    Init(InitialParam), Terminate(TerminalParam), Increment(IncrementationParam) {
}

DoStmt *DoStmt::Create(ASTContext &C, SourceLocation Loc, StmtLabelReference TermStmt,
                       Expr *DoVariable, Expr *InitialParam,
                       Expr *TerminalParam, Expr *IncrementationParam,
                       Expr *StmtLabel) {
  return new(C) DoStmt(Loc, TermStmt, DoVariable, InitialParam,TerminalParam,
                       IncrementationParam, StmtLabel);
}

void DoStmt::setTerminatingStmt(StmtLabelReference Stmt) {
  assert(!TerminatingStmt.Statement);
  assert(Stmt.Statement);
  TerminatingStmt = Stmt;
}

//===----------------------------------------------------------------------===//
// Continue Statement
//===----------------------------------------------------------------------===//

ContinueStmt::ContinueStmt(SourceLocation Loc, Expr *StmtLabel)
  : Stmt(Continue, Loc, StmtLabel) {
}
ContinueStmt *ContinueStmt::Create(ASTContext &C, SourceLocation Loc, Expr *StmtLabel) {
  return new(C) ContinueStmt(Loc, StmtLabel);
}

//===----------------------------------------------------------------------===//
// Stop Statement
//===----------------------------------------------------------------------===//

StopStmt::StopStmt(SourceLocation Loc, Expr *stopCode, Expr *StmtLabel)
  : Stmt(Stop, Loc, StmtLabel), StopCode(stopCode) {
}
StopStmt *StopStmt::Create(ASTContext &C, SourceLocation Loc, Expr *stopCode, Expr *StmtLabel) {
  return new(C) StopStmt(Loc, stopCode, StmtLabel);
}

//===----------------------------------------------------------------------===//
// Assignment Statement
//===----------------------------------------------------------------------===//

AssignmentStmt::AssignmentStmt(SourceLocation Loc, Expr *lhs, Expr *rhs,
                               Expr *StmtLabel)
  : Stmt(Assignment, Loc, StmtLabel), LHS(lhs), RHS(rhs)
{}

AssignmentStmt *AssignmentStmt::Create(ASTContext &C, SourceLocation Loc, Expr *LHS,
                                       Expr *RHS, Expr *StmtLabel) {
  return new (C) AssignmentStmt(Loc, LHS, RHS, StmtLabel);
}

//===----------------------------------------------------------------------===//
// Print Statement
//===----------------------------------------------------------------------===//

PrintStmt::PrintStmt(ASTContext &C, SourceLocation L, FormatSpec *fs,
                     ArrayRef<ExprResult> OutList, Expr *StmtLabel)
  : ListStmt(C, Print, L, OutList, StmtLabel), FS(fs) {}

PrintStmt *PrintStmt::Create(ASTContext &C, SourceLocation L, FormatSpec *fs,
                             ArrayRef<ExprResult> OutList,
                             Expr *StmtLabel) {
  return new (C) PrintStmt(C, L, fs, OutList, StmtLabel);
}

} //namespace flang
