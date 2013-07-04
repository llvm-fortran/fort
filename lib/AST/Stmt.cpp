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
#include "flang/AST/Expr.h"
#include "flang/AST/ASTContext.h"
#include "flang/Basic/IdentifierTable.h"
#include "llvm/ADT/StringRef.h"

namespace flang {

//===----------------------------------------------------------------------===//
// Statement Base Class
//===----------------------------------------------------------------------===//

Stmt::~Stmt() {}

//===----------------------------------------------------------------------===//
// Statement Part Statement
//===----------------------------------------------------------------------===//

ConstructPartStmt::ConstructPartStmt(ConstructStmtClass StmtType, SourceLocation Loc,
                                     const IdentifierInfo *Construct,
                                     Expr *StmtLabel)
  : Stmt(ConstructPartStmtClass, Loc, StmtLabel),
    ConstructId(StmtType), ConstructName(Construct) {
}


ConstructPartStmt *ConstructPartStmt::Create(ASTContext &C, ConstructStmtClass StmtType,
                                             SourceLocation Loc,
                                             const IdentifierInfo *Construct,
                                             Expr *StmtLabel) {
  return new(C) ConstructPartStmt(StmtType, Loc, Construct, StmtLabel);
}

//===----------------------------------------------------------------------===//
// Declaration Statement
//===----------------------------------------------------------------------===//

DeclStmt::DeclStmt(SourceLocation Loc, NamedDecl *Decl, Expr *StmtLabel)
  : Stmt(DeclStmtClass, Loc, StmtLabel), Declaration(Decl) {
}

DeclStmt *DeclStmt::Create(ASTContext &C, SourceLocation Loc,
                           NamedDecl *Declaration, Expr *StmtLabel) {
  return new(C) DeclStmt(Loc, Declaration, StmtLabel);
}

//===----------------------------------------------------------------------===//
// Bundled Compound Statement
//===----------------------------------------------------------------------===//

CompoundStmt::CompoundStmt(ASTContext &C, SourceLocation Loc,
                           ArrayRef<Stmt*> Body, Expr *StmtLabel)
  : ListStmt(C, CompoundStmtClass, Loc, Body, StmtLabel) {
}

CompoundStmt *CompoundStmt::Create(ASTContext &C, SourceLocation Loc,
                                   ArrayRef<Stmt*> Body, Expr *StmtLabel) {
  return new(C) CompoundStmt(C, Loc, Body, StmtLabel);
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
  : ListStmt(C, UseStmtClass, SourceLocation(), RenameList, StmtLabel),
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
  : ListStmt(C, ImportStmtClass, Loc, Names, StmtLabel) {}

ImportStmt *ImportStmt::Create(ASTContext &C, SourceLocation Loc,
                               ArrayRef<const IdentifierInfo*> Names,
                               Expr *StmtLabel) {
  return new (C) ImportStmt(C, Loc, Names, StmtLabel);
}

//===----------------------------------------------------------------------===//
// Parameter Statement
//===----------------------------------------------------------------------===//

ParameterStmt::ParameterStmt(SourceLocation Loc, const IdentifierInfo *Name,
                             Expr *Val, Expr *StmtLabel)
  : Stmt(ParameterStmtClass, Loc, StmtLabel), IDInfo(Name),
    Value(Val) {
}

ParameterStmt *ParameterStmt::Create(ASTContext &C, SourceLocation Loc,
                                     const IdentifierInfo *Name,
                                     Expr *Value, Expr *StmtLabel) {
  return new(C) ParameterStmt(Loc, Name, Value, StmtLabel);
}

//===----------------------------------------------------------------------===//
// Implicit Statement
//===----------------------------------------------------------------------===//

ImplicitStmt::ImplicitStmt(SourceLocation Loc, Expr *StmtLabel)
  : Stmt(ImplicitStmtClass, Loc, StmtLabel), None(true),
    LetterSpec(LetterSpecTy(nullptr,nullptr)) {
}

ImplicitStmt::ImplicitStmt(SourceLocation Loc, QualType T,
                           LetterSpecTy Spec,
                           Expr *StmtLabel)
  : Stmt(ImplicitStmtClass, Loc, StmtLabel), Ty(T), None(false),
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
// Dimension Statement
//===----------------------------------------------------------------------===//

DimensionStmt::DimensionStmt(ASTContext &C, SourceLocation Loc,
                             const IdentifierInfo* IDInfo,
                             ArrayRef<ArraySpec*> Dims,
                             Expr *StmtLabel)
  : ListStmt(C, DimensionStmtClass, Loc, Dims, StmtLabel) , VarName(IDInfo) {
}

DimensionStmt *DimensionStmt::Create(ASTContext &C, SourceLocation Loc,
                                     const IdentifierInfo* IDInfo,
                                     ArrayRef<ArraySpec*> Dims,
                                     Expr *StmtLabel) {
  return new (C) DimensionStmt(C, Loc, IDInfo, Dims, StmtLabel);
}

//===----------------------------------------------------------------------===//
// Format Statement
//===----------------------------------------------------------------------===//

FormatStmt::FormatStmt(SourceLocation Loc, FormatItemList *ItemList,
                       FormatItemList *UnlimitedItemList, Expr *StmtLabel)
  : Stmt(FormatStmtClass, Loc, StmtLabel), Items(ItemList),
    UnlimitedItems(UnlimitedItemList) {
}

FormatStmt *FormatStmt::Create(ASTContext &C, SourceLocation Loc,
                               FormatItemList *ItemList,
                               FormatItemList *UnlimitedItemList,
                               Expr *StmtLabel) {
  return new (C) FormatStmt(Loc, ItemList, UnlimitedItemList, StmtLabel);
}

//===----------------------------------------------------------------------===//
// Entry Statement
//===----------------------------------------------------------------------===//

EntryStmt::EntryStmt(SourceLocation Loc, Expr *StmtLabel)
  : Stmt(EntryStmtClass, Loc, StmtLabel) {}

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
  : ListStmt(C, AsynchronousStmtClass, Loc, objNames, StmtLabel) {}

AsynchronousStmt *AsynchronousStmt::
Create(ASTContext &C, SourceLocation Loc, ArrayRef<const IdentifierInfo*> objNames,
       Expr *StmtLabel) {
  return new (C) AsynchronousStmt(C, Loc, objNames, StmtLabel);
}

//===----------------------------------------------------------------------===//
// External Statement
//===----------------------------------------------------------------------===//

ExternalStmt::ExternalStmt(SourceLocation Loc, const IdentifierInfo *Name,
                           Expr *StmtLabel)
  : Stmt(ExternalStmtClass, Loc, StmtLabel), IDInfo(Name) {}

ExternalStmt *ExternalStmt::Create(ASTContext &C, SourceLocation Loc,
                                   const IdentifierInfo *Name,
                                   Expr *StmtLabel) {
  return new (C) ExternalStmt(Loc, Name, StmtLabel);
}

//===----------------------------------------------------------------------===//
// Intrinsic Statement
//===----------------------------------------------------------------------===//

IntrinsicStmt::IntrinsicStmt(SourceLocation Loc, const IdentifierInfo *Name,
                           Expr *StmtLabel)
  : Stmt(IntrinsicStmtClass, Loc, StmtLabel), IDInfo(Name) {}

IntrinsicStmt *IntrinsicStmt::Create(ASTContext &C, SourceLocation Loc,
                                     const IdentifierInfo *Name,
                                     Expr *StmtLabel) {
  return new (C) IntrinsicStmt(Loc, Name, StmtLabel);
}

//===----------------------------------------------------------------------===//
// Data Statement
//===----------------------------------------------------------------------===//

DataStmt::DataStmt(ASTContext &C, SourceLocation Loc,
                   ArrayRef<Expr*> Names,
                   ArrayRef<Expr*> Values,
                   Expr *StmtLabel)
  : Stmt(DataStmtClass, Loc, StmtLabel) {
  NumNames = Names.size();
  NameList = new (C) Expr* [NumNames];
  for(size_t I = 0; I < Names.size(); ++I)
    NameList[I] = Names[I];

  NumValues = Values.size();
  ValueList = new (C) Expr* [NumValues];
  for(size_t I = 0; I < Values.size(); ++I)
    ValueList[I] = Values[I];
}

DataStmt *DataStmt::Create(ASTContext &C, SourceLocation Loc,
                           ArrayRef<Expr*> Names,
                           ArrayRef<Expr*> Values,
                           Expr *StmtLabel) {
  return new(C) DataStmt(C, Loc, Names, Values, StmtLabel);
}

//===----------------------------------------------------------------------===//
// Block Statement
//===----------------------------------------------------------------------===//

BlockStmt::BlockStmt(ASTContext &C, SourceLocation Loc,
                     ArrayRef<Stmt*> Body)
  : ListStmt(C, BlockStmtClass, Loc, Body, nullptr) {
}

BlockStmt *BlockStmt::Create(ASTContext &C, SourceLocation Loc,
                             ArrayRef<Stmt*> Body) {
  return new(C) BlockStmt(C, Loc, Body);
}

//===----------------------------------------------------------------------===//
// Assign Statement
//===----------------------------------------------------------------------===//

AssignStmt::AssignStmt(SourceLocation Loc, StmtLabelReference Addr,
                       Expr *Dest, Expr *StmtLabel)
  : Stmt(AssignStmtClass, Loc, StmtLabel), Address(Addr), Destination(Dest) {
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
  : ListStmt(C, AssignedGotoStmtClass, Loc, Vals, StmtLabel), Destination(Dest) {
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
  : Stmt(GotoStmtClass, Loc, StmtLabel), Destination(Dest) {
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
  : Stmt(IfStmtClass, Loc, StmtLabel), Condition(Cond),
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

CFBlockStmt::CFBlockStmt(StmtClass Type, SourceLocation Loc, Expr *StmtLabel)
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
  : CFBlockStmt(DoStmtClass, Loc, StmtLabel), TerminatingStmt(TermStmt), DoVar(DoVariable),
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
// Do while statement
//===----------------------------------------------------------------------===//

DoWhileStmt::DoWhileStmt(SourceLocation Loc, Expr *Cond, Expr *StmtLabel)
  : CFBlockStmt(DoWhileStmtClass, Loc, StmtLabel), Condition(Cond) {
}

DoWhileStmt *DoWhileStmt::Create(ASTContext &C, SourceLocation Loc,
                                 Expr *Condition, Expr *StmtLabel) {
  return new(C) DoWhileStmt(Loc, Condition, StmtLabel);
}

//===----------------------------------------------------------------------===//
// Continue Statement
//===----------------------------------------------------------------------===//

ContinueStmt::ContinueStmt(SourceLocation Loc, Expr *StmtLabel)
  : Stmt(ContinueStmtClass, Loc, StmtLabel) {
}
ContinueStmt *ContinueStmt::Create(ASTContext &C, SourceLocation Loc, Expr *StmtLabel) {
  return new(C) ContinueStmt(Loc, StmtLabel);
}

//===----------------------------------------------------------------------===//
// Stop Statement
//===----------------------------------------------------------------------===//

StopStmt::StopStmt(SourceLocation Loc, Expr *stopCode, Expr *StmtLabel)
  : Stmt(StopStmtClass, Loc, StmtLabel), StopCode(stopCode) {
}
StopStmt *StopStmt::Create(ASTContext &C, SourceLocation Loc, Expr *stopCode, Expr *StmtLabel) {
  return new(C) StopStmt(Loc, stopCode, StmtLabel);
}

//===----------------------------------------------------------------------===//
// Return Statement
//===----------------------------------------------------------------------===//

ReturnStmt::ReturnStmt(SourceLocation Loc, Expr *e, Expr *StmtLabel)
  : Stmt(ReturnStmtClass, Loc, StmtLabel), E(e) {
}

ReturnStmt *ReturnStmt::Create(ASTContext &C, SourceLocation Loc, Expr * E,
                               Expr *StmtLabel) {
  return new(C) ReturnStmt(Loc, E, StmtLabel);
}

//===----------------------------------------------------------------------===//
// Call Statement
//===----------------------------------------------------------------------===//

CallStmt::CallStmt(ASTContext &C, SourceLocation Loc,
                   FunctionDecl *Func, ArrayRef<Expr*> Args, Expr *StmtLabel)
  : Stmt(CallStmtClass, Loc, StmtLabel), MultiArgumentExpr(C, Args),
    Function(Func) {
}

CallStmt *CallStmt::Create(ASTContext &C, SourceLocation Loc,
                           FunctionDecl *Func, ArrayRef<Expr*> Args,
                           Expr *StmtLabel) {
  return new(C) CallStmt(C, Loc, Func, Args, StmtLabel);
}

//===----------------------------------------------------------------------===//
// Assignment Statement
//===----------------------------------------------------------------------===//

AssignmentStmt::AssignmentStmt(SourceLocation Loc, Expr *lhs, Expr *rhs,
                               Expr *StmtLabel)
  : Stmt(AssignmentStmtClass, Loc, StmtLabel), LHS(lhs), RHS(rhs)
{}

AssignmentStmt *AssignmentStmt::Create(ASTContext &C, SourceLocation Loc, Expr *LHS,
                                       Expr *RHS, Expr *StmtLabel) {
  return new (C) AssignmentStmt(Loc, LHS, RHS, StmtLabel);
}

//===----------------------------------------------------------------------===//
// Print Statement
//===----------------------------------------------------------------------===//

PrintStmt::PrintStmt(ASTContext &C, SourceLocation L, FormatSpec *fs,
                     ArrayRef<Expr*> OutList, Expr *StmtLabel)
  : Stmt(PrintStmtClass, L, StmtLabel),
    MultiArgumentExpr(C, OutList), FS(fs) {}

PrintStmt *PrintStmt::Create(ASTContext &C, SourceLocation L, FormatSpec *fs,
                             ArrayRef<Expr*> OutList,
                             Expr *StmtLabel) {
  return new (C) PrintStmt(C, L, fs, OutList, StmtLabel);
}

//===----------------------------------------------------------------------===//
// Write Statement
//===----------------------------------------------------------------------===//

WriteStmt::WriteStmt(ASTContext &C, SourceLocation Loc, UnitSpec *us,
                     FormatSpec *fs, ArrayRef<Expr*> OutList, Expr *StmtLabel)
  : Stmt(WriteStmtClass, Loc, StmtLabel),
    MultiArgumentExpr(C, OutList), US(us), FS(fs) {
}

WriteStmt *WriteStmt::Create(ASTContext &C, SourceLocation Loc, UnitSpec *US,
                             FormatSpec *FS, ArrayRef<Expr*> OutList, Expr *StmtLabel) {
  return new(C) WriteStmt(C, Loc, US, FS, OutList, StmtLabel);
}

} //namespace flang
