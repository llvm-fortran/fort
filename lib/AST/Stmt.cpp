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

Stmt *Stmt::Create(ASTContext &C, StmtTy StmtType, SMLoc Loc, ExprResult StmtLabel) {
  return new(C) Stmt(StmtType, Loc, StmtLabel);
}

//===----------------------------------------------------------------------===//
// Program Statement
//===----------------------------------------------------------------------===//

ProgramStmt *ProgramStmt::Create(ASTContext &C, const IdentifierInfo *ProgName,
                                 llvm::SMLoc Loc, llvm::SMLoc NameLoc,
                                 ExprResult StmtLabel) {
  return new (C) ProgramStmt(ProgName, Loc, NameLoc, StmtLabel);
}

//===----------------------------------------------------------------------===//
// EndProgram Statement
//===----------------------------------------------------------------------===//

EndProgramStmt *EndProgramStmt::Create(ASTContext &C,
                                       const IdentifierInfo *ProgName,
                                       llvm::SMLoc Loc, llvm::SMLoc NameLoc,
                                       ExprResult StmtLabel) {
  return new (C) EndProgramStmt(ProgName, Loc, NameLoc, StmtLabel);
}

//===----------------------------------------------------------------------===//
// Use Statement
//===----------------------------------------------------------------------===//

UseStmt::UseStmt(ASTContext &C, ModuleNature MN, const IdentifierInfo *modName,
                 ArrayRef<RenamePair> RenameList, ExprResult StmtLabel)
  : ListStmt(C, Use, SMLoc(), RenameList, StmtLabel),
    ModNature(MN), ModName(modName), Only(false) {}

UseStmt *UseStmt::Create(ASTContext &C, ModuleNature MN,
                         const IdentifierInfo *modName,
                         ExprResult StmtLabel) {
  return  new (C) UseStmt(C, MN, modName, ArrayRef<RenamePair>(), StmtLabel);
}

UseStmt *UseStmt::Create(ASTContext &C, ModuleNature MN,
                         const IdentifierInfo *modName, bool Only,
                         ArrayRef<RenamePair> RenameList,
                         ExprResult StmtLabel) {
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

ImportStmt::ImportStmt(ASTContext &C, SMLoc Loc,
                       ArrayRef<const IdentifierInfo*> Names,
                       ExprResult StmtLabel)
  : ListStmt(C, Import, Loc, Names, StmtLabel) {}

ImportStmt *ImportStmt::Create(ASTContext &C, SMLoc Loc,
                               ArrayRef<const IdentifierInfo*> Names,
                               ExprResult StmtLabel) {
  return new (C) ImportStmt(C, Loc, Names, StmtLabel);
}

//===----------------------------------------------------------------------===//
// Implicit Statement
//===----------------------------------------------------------------------===//

ImplicitStmt::ImplicitStmt(ASTContext &C, SMLoc L, ExprResult StmtLabel)
  : ListStmt(C, Implicit, L, ArrayRef<LetterSpec>(), StmtLabel), None(true) {}

ImplicitStmt::ImplicitStmt(ASTContext &C, SMLoc L, QualType T,
                           ArrayRef<LetterSpec> SpecList,
                           ExprResult StmtLabel)
  : ListStmt(C, Implicit, L, SpecList, StmtLabel), Ty(T), None(false) {}

ImplicitStmt *ImplicitStmt::Create(ASTContext &C, SMLoc L,
                                   ExprResult StmtLabel) {
  return new (C) ImplicitStmt(C, L, StmtLabel);
}

ImplicitStmt *ImplicitStmt::Create(ASTContext &C, SMLoc L, QualType T,
                                   ArrayRef<LetterSpec> SpecList,
                                   ExprResult StmtLabel) {
  return new (C) ImplicitStmt(C, L, T, SpecList, StmtLabel);
}

//===----------------------------------------------------------------------===//
// Parameter Statement
//===----------------------------------------------------------------------===//

ParameterStmt::ParameterStmt(ASTContext &C, SMLoc Loc,
                             ArrayRef<ParamPair> PList, ExprResult StmtLabel)
  : ListStmt(C, Parameter, Loc, PList, StmtLabel) {}

ParameterStmt *ParameterStmt::Create(ASTContext &C, SMLoc Loc,
                                     ArrayRef<ParamPair>ParamList,
                                     ExprResult StmtLabel) {
  return new (C) ParameterStmt(C, Loc, ParamList, StmtLabel);
}

//===----------------------------------------------------------------------===//
// Dimension Statement
//===----------------------------------------------------------------------===//

DimensionStmt::DimensionStmt(ASTContext &C, SMLoc Loc,
                             const IdentifierInfo* IDInfo,
                             ArrayRef<ArrayType::Dimension> Dims,
                             ExprResult StmtLabel)
  : ListStmt(C, Dimension, Loc, Dims, StmtLabel) , VarName(IDInfo) {
}

DimensionStmt *DimensionStmt::Create(ASTContext &C, SMLoc Loc,
                                     const IdentifierInfo* IDInfo,
                                     ArrayRef<ArrayType::Dimension> Dims,
                                     ExprResult StmtLabel) {
  return new (C) DimensionStmt(C, Loc, IDInfo, Dims, StmtLabel);
}

//===----------------------------------------------------------------------===//
// Format Statement
//===----------------------------------------------------------------------===//

FormatStmt::FormatStmt(SMLoc Loc, FormatSpec *fs, ExprResult StmtLabel)
  : Stmt(Format, Loc, StmtLabel), FS(fs) {}

FormatStmt *FormatStmt::Create(ASTContext &C, SMLoc Loc, FormatSpec *fs,
                               ExprResult StmtLabel) {
  return new (C) FormatStmt(Loc, fs, StmtLabel);
}

//===----------------------------------------------------------------------===//
// Entry Statement
//===----------------------------------------------------------------------===//

EntryStmt::EntryStmt(SMLoc Loc, ExprResult StmtLabel)
  : Stmt(Entry, Loc, StmtLabel) {}

EntryStmt *EntryStmt::Create(ASTContext &C, SMLoc Loc, ExprResult StmtLabel) {
  return new (C) EntryStmt(Loc, StmtLabel);
}

//===----------------------------------------------------------------------===//
// Asynchronous Statement
//===----------------------------------------------------------------------===//

AsynchronousStmt::
AsynchronousStmt(ASTContext &C, SMLoc Loc,
                 ArrayRef<const IdentifierInfo*> objNames,
                 ExprResult StmtLabel)
  : ListStmt(C, Asynchronous, Loc, objNames, StmtLabel) {}

AsynchronousStmt *AsynchronousStmt::
Create(ASTContext &C, SMLoc Loc, ArrayRef<const IdentifierInfo*> objNames,
       ExprResult StmtLabel) {
  return new (C) AsynchronousStmt(C, Loc, objNames, StmtLabel);
}

//===----------------------------------------------------------------------===//
// External Statement
//===----------------------------------------------------------------------===//

ExternalStmt::ExternalStmt(ASTContext &C, SMLoc Loc,
                           ArrayRef<const IdentifierInfo *> ExternalNames,
                           ExprResult StmtLabel)
  : ListStmt(C, External, Loc, ExternalNames, StmtLabel) {}

ExternalStmt *ExternalStmt::Create(ASTContext &C, SMLoc Loc,
                                   ArrayRef<const IdentifierInfo*>ExternalNames,
                                   ExprResult StmtLabel) {
  return new (C) ExternalStmt(C, Loc, ExternalNames, StmtLabel);
}

//===----------------------------------------------------------------------===//
// Intrinsic Statement
//===----------------------------------------------------------------------===//

IntrinsicStmt::IntrinsicStmt(ASTContext &C, SMLoc Loc,
                           ArrayRef<const IdentifierInfo *> IntrinsicNames,
                           ExprResult StmtLabel)
  : ListStmt(C, Intrinsic, Loc, IntrinsicNames, StmtLabel) {}

IntrinsicStmt *IntrinsicStmt::Create(ASTContext &C, SMLoc Loc,
                                   ArrayRef<const IdentifierInfo*> IntrinsicNames,
                                   ExprResult StmtLabel) {
  return new (C) IntrinsicStmt(C, Loc, IntrinsicNames, StmtLabel);
}

//===----------------------------------------------------------------------===//
// Block Statement
//===----------------------------------------------------------------------===//

BlockStmt::BlockStmt(ASTContext &C, SMLoc Loc,
                     ArrayRef<StmtResult> Body)
  : ListStmt(C, Block, Loc, Body, ExprResult()) {
}

BlockStmt *BlockStmt::Create(ASTContext &C, SMLoc Loc,
                             ArrayRef<StmtResult> Body) {
  return new(C) BlockStmt(C, Loc, Body);
}

//===----------------------------------------------------------------------===//
// Assign Statement
//===----------------------------------------------------------------------===//

AssignStmt::AssignStmt(SMLoc Loc, StmtLabelReference Addr,
                       ExprResult Dest, ExprResult StmtLabel)
  : Stmt(Assign, Loc, StmtLabel), Address(Addr), Destination(Dest) {
}

AssignStmt *AssignStmt::Create(ASTContext &C, SMLoc Loc,
                               StmtLabelReference Address,
                               ExprResult Destination,
                               ExprResult StmtLabel) {
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

AssignedGotoStmt::AssignedGotoStmt(ASTContext &C, SMLoc Loc, ExprResult Dest,
                                   ArrayRef<StmtLabelReference> Vals,
                                   ExprResult StmtLabel)
  : ListStmt(C, AssignedGoto, Loc, Vals, StmtLabel), Destination(Dest) {
}

AssignedGotoStmt *AssignedGotoStmt::Create(ASTContext &C, SMLoc Loc,
                                           ExprResult Destination,
                                           ArrayRef<StmtLabelReference> AllowedValues,
                                           ExprResult StmtLabel) {
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

GotoStmt::GotoStmt(SMLoc Loc, StmtLabelReference Dest, ExprResult StmtLabel)
  : Stmt(Goto, Loc, StmtLabel), Destination(Dest) {
}

GotoStmt *GotoStmt::Create(ASTContext &C, SMLoc Loc,
                           StmtLabelReference Destination,
                           ExprResult StmtLabel) {
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

IfStmt::IfStmt(SMLoc Loc, ExprResult Cond, ExprResult StmtLabel)
  : Stmt(If, Loc, StmtLabel), Condition(Cond),
    ThenArm(nullptr), ElseArm(nullptr)  {
}

IfStmt *IfStmt::Create(ASTContext &C, SMLoc Loc,
                       ExprResult Condition, ExprResult StmtLabel) {
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
// Do Statement
//===----------------------------------------------------------------------===//

DoStmt::DoStmt(SMLoc Loc, StmtLabelReference TermStmt,
               ExprResult DoVariable, ExprResult InitialParam,
               ExprResult TerminalParam, ExprResult IncrementationParam,
               ExprResult StmtLabel)
  : Stmt(Do, Loc, StmtLabel), TerminatingStmt(TermStmt), DoVar(DoVariable),
    Init(InitialParam), Terminate(TerminalParam), Increment(IncrementationParam),
    Body(nullptr) {
}

DoStmt *DoStmt::Create(ASTContext &C, SMLoc Loc, StmtLabelReference TermStmt,
                       ExprResult DoVariable, ExprResult InitialParam,
                       ExprResult TerminalParam, ExprResult IncrementationParam,
                       ExprResult StmtLabel) {
  return new(C) DoStmt(Loc, TermStmt, DoVariable, InitialParam,TerminalParam,
                       IncrementationParam, StmtLabel);
}

void DoStmt::setTerminatingStmt(StmtLabelReference Stmt) {
  assert(!TerminatingStmt.Statement);
  assert(Stmt.Statement);
  TerminatingStmt = Stmt;
}

void DoStmt::setBody(Stmt *Body) {
  assert(!this->Body);
  assert(Body);
  this->Body = Body;
}

//===----------------------------------------------------------------------===//
// Continue Statement
//===----------------------------------------------------------------------===//

ContinueStmt::ContinueStmt(SMLoc Loc, ExprResult StmtLabel)
  : Stmt(Continue, Loc, StmtLabel) {
}
ContinueStmt *ContinueStmt::Create(ASTContext &C, SMLoc Loc, ExprResult StmtLabel) {
  return new(C) ContinueStmt(Loc, StmtLabel);
}

//===----------------------------------------------------------------------===//
// Stop Statement
//===----------------------------------------------------------------------===//

StopStmt::StopStmt(SMLoc Loc, ExprResult stopCode, ExprResult StmtLabel)
  : Stmt(Stop, Loc, StmtLabel), StopCode(stopCode) {
}
StopStmt *StopStmt::Create(ASTContext &C, SMLoc Loc, ExprResult stopCode, ExprResult StmtLabel) {
  return new(C) StopStmt(Loc, stopCode, StmtLabel);
}

//===----------------------------------------------------------------------===//
// Assignment Statement
//===----------------------------------------------------------------------===//

AssignmentStmt::AssignmentStmt(ExprResult lhs, ExprResult rhs,
                               ExprResult StmtLabel)
  : Stmt(Assignment, llvm::SMLoc(), StmtLabel), LHS(lhs), RHS(rhs)
{}

AssignmentStmt *AssignmentStmt::Create(ASTContext &C, ExprResult lhs,
                                       ExprResult rhs, ExprResult StmtLabel) {
  return new (C) AssignmentStmt(lhs, rhs, StmtLabel);
}

//===----------------------------------------------------------------------===//
// Print Statement
//===----------------------------------------------------------------------===//

PrintStmt::PrintStmt(ASTContext &C, SMLoc L, FormatSpec *fs,
                     ArrayRef<ExprResult> OutList, ExprResult StmtLabel)
  : ListStmt(C, Print, L, OutList, StmtLabel), FS(fs) {}

PrintStmt *PrintStmt::Create(ASTContext &C, SMLoc L, FormatSpec *fs,
                             ArrayRef<ExprResult> OutList,
                             ExprResult StmtLabel) {
  return new (C) PrintStmt(C, L, fs, OutList, StmtLabel);
}

} //namespace flang
