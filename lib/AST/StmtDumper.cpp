//===--- StmtDumper.cpp - Dump Fortran Statements -------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file dumps statements.
//
//===----------------------------------------------------------------------===//

#include "flang/AST/StmtDumper.h"
#include "flang/AST/Expr.h"
#include "flang/AST/Stmt.h"
#include "flang/AST/Type.h"
#include "flang/AST/FormatItem.h"
#include "flang/Basic/LLVM.h"
#include "llvm/Support/raw_ostream.h"
using namespace flang;

namespace {

class StmtVisitor {
  raw_ostream &OS;
public:
  StmtVisitor(raw_ostream &os) : OS(os) {}

  void visit(StmtResult);

private:
#define VISIT(STMT) void visit(const STMT *S)
  VISIT(DeclStmt);
  VISIT(BundledCompoundStmt);
  VISIT(ProgramStmt);
  VISIT(EndProgramStmt);
  VISIT(UseStmt);
  VISIT(ImportStmt);
  VISIT(ImplicitStmt);
  VISIT(DimensionStmt);
  VISIT(AsynchronousStmt);
  VISIT(DataStmt);
  VISIT(BlockStmt);
  VISIT(AssignStmt);
  VISIT(AssignedGotoStmt);
  VISIT(GotoStmt);
  VISIT(IfStmt);
  VISIT(DoStmt);
  VISIT(ContinueStmt);
  VISIT(StopStmt);
  VISIT(AssignmentStmt);
  VISIT(PrintStmt);
  VISIT(FormatStmt);
#undef VISIT
};

} // end anonymous namespace

void StmtVisitor::visit(StmtResult S) {
#define HANDLE(STMT) \
  if (const STMT *stmt = dyn_cast<STMT>(S.get())) {     \
    visit(stmt);                                        \
    return;                                             \
  }
  HANDLE(DeclStmt);
  HANDLE(BundledCompoundStmt);
  HANDLE(ProgramStmt);
  HANDLE(EndProgramStmt);
  HANDLE(UseStmt);
  HANDLE(ImportStmt);
  HANDLE(ImplicitStmt);
  HANDLE(DimensionStmt);
  HANDLE(AsynchronousStmt);
  HANDLE(DataStmt);
  HANDLE(BlockStmt);
  HANDLE(AssignStmt);
  HANDLE(AssignedGotoStmt);
  HANDLE(GotoStmt);
  HANDLE(IfStmt);
  HANDLE(DoStmt);
  HANDLE(ContinueStmt);
  HANDLE(StopStmt);
  HANDLE(AssignmentStmt);
  HANDLE(PrintStmt);
  HANDLE(FormatStmt);
#undef HANDLE

  switch(S.get()->getStatementID()) {
  case Stmt::Else: OS << "(else)\n"; break;
  case Stmt::EndIf: OS << "(end if)\n"; break;
  case Stmt::EndDo: OS << "(end do)\n"; break;
  default: break;
  }
}

void StmtVisitor::visit(const DeclStmt *S) {
  S->getDeclaration()->print(OS);
  OS << "\n";
}

void StmtVisitor::visit(const BundledCompoundStmt *S) {
  auto Body = S->getBody();
  for(size_t I = 0; I < Body.size(); ++I) {
    if(I) OS<<"&";
    visit(Body[I]);
  }
}

void StmtVisitor::visit(const ProgramStmt *S) {
  const IdentifierInfo *Name = S->getProgramName();
  OS << "(program";
  if (Name) OS << ":  '" << Name->getName() << "'";
  OS << ")\n";
}
void StmtVisitor::visit(const EndProgramStmt *S) {
  const IdentifierInfo *Name = S->getProgramName();
  OS << "(end program";
  if (Name) OS << ":  '" << Name->getName() << "'";
  OS << ")\n";
}
void StmtVisitor::visit(const UseStmt *S) {
}
void StmtVisitor::visit(const ImportStmt *S) {
  ArrayRef<const IdentifierInfo *> NameList = S->getIDList();
  OS << "(import";
  if (NameList.size() != 0) {
    OS << ":";
    for (unsigned I = 0, E = NameList.size(); I != E; ++I)
      OS << "\n  ('" << NameList[I]->getName() << "')";
  }
  OS << ")\n";
}
void StmtVisitor::visit(const ImplicitStmt *S) {
  OS << "(implicit";
  if (S->isNone()) {
    OS << " none)\n";
    return;
  }
  OS << " ";
  S->getType().print(OS);
  OS << " :: ";

  auto Spec = S->getLetterSpec();
  OS << " (" << Spec.first->getName();
  if (Spec.second)
    OS << "-" << Spec.second->getName();
  OS << ") )\n";
}
void StmtVisitor::visit(const DimensionStmt *S) {
  OS << "DIMENSION " << S->getVariableName()->getNameStart()
     << "\n";
}

void StmtVisitor::visit(const AsynchronousStmt *S) {
}

void StmtVisitor::visit(const DataStmt *S) {
  OS << "DATA ";
  auto Names = S->getNames();
  for(size_t I = 0; I < Names.size(); ++I) {
    if(I) OS << ", ";
    Names[I]->print(OS);
  }
  OS << " / ";
  auto Values = S->getValues();
  for(size_t I = 0; I < Values.size(); ++I) {
    if(I) OS << ", ";
    Values[I]->print(OS);
  }
  OS << " /\n";
}

void StmtVisitor::visit(const BlockStmt *S) {
  ArrayRef<StmtResult> Body = S->getIDList();
  for(size_t i = 0; i < Body.size(); ++i) {
    StmtResult stmt = Body[i];
    visit(stmt.get());
  }
}

void StmtVisitor::visit(const AssignStmt *S) {
  OS << "(assign ";
  if(S->getAddress().Statement)
    S->getAddress().Statement->getStmtLabel()->print(OS);
  OS << " to ";
  S->getDestination()->print(OS);
  OS << ")\n";
}
void StmtVisitor::visit(const AssignedGotoStmt *S) {
  OS << "(goto ";
  S->getDestination()->print(OS);
  OS << ")\n";
}

void StmtVisitor::visit(const GotoStmt *S) {
  OS << "(goto ";
  if(S->getDestination().Statement)
    S->getDestination().Statement->getStmtLabel()->print(OS);
  OS << ")\n";
}

void StmtVisitor::visit(const IfStmt* S) {
  OS << "(if ";
  S->getCondition()->print(OS);
  if(S->getThenStmt()) {
    OS << ") ";
    visit(S->getThenStmt());
  }
  OS << ")\n";
}

void StmtVisitor::visit(const DoStmt *S) {
  OS<<"(do ";
  if(S->getTerminatingStmt().Statement)
    S->getTerminatingStmt().Statement->getStmtLabel()->print(OS);
  OS << " ";
  S->getDoVar()->print(OS);
  OS << " = ";
  S->getInitialParameter()->print(OS);
  OS << ", ";
  S->getTerminalParameter()->print(OS);
  if(S->getIncrementationParameter()) {
    OS << ", ";
    S->getIncrementationParameter()->print(OS);
  }
  OS << ")\n";
}

void StmtVisitor::visit(const ContinueStmt *S) {
  OS << "continue\n";
}

void StmtVisitor::visit(const StopStmt *S) {
  if(S->getStopCode()){
    OS << "stop ";
    S->getStopCode()->print(OS);
    OS << "\n";
  } else
    OS << "stop\n";
}

void StmtVisitor::visit(const AssignmentStmt *S) {
  OS << "(";
  if(S->getLHS()) S->getLHS()->print(OS);
  OS << " = ";
  if(S->getRHS()) S->getRHS()->print(OS);
  OS << ")\n";
}
void StmtVisitor::visit(const PrintStmt *S) {
  OS << "(print)\n";
}

void StmtVisitor::visit(const FormatStmt *S) {
  OS << "FORMAT ";
  S->getItemList()->print(OS);
  if(S->getUnlimitedItemList())
    S->getUnlimitedItemList()->print(OS);
  OS << "\n";
}

void flang::dump(StmtResult S) {
  StmtVisitor SV(llvm::errs());
  SV.visit(S);
}

void flang::dump(ArrayRef<StmtResult> S) {
  StmtVisitor SV(llvm::errs());

  for (ArrayRef<StmtResult>::iterator I = S.begin(), E = S.end(); I != E; ++I){
    if(!I->get()) continue;
    if (!isa<ProgramStmt>(I->get()))
      SV.visit(*I);
  }
}
