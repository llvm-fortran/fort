//===--- ASTDumper.cpp - Dump Fortran AST --------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file dumps AST.
//
//===----------------------------------------------------------------------===//

#include "flang/AST/ASTDumper.h"
#include "flang/AST/Expr.h"
#include "flang/AST/Stmt.h"
#include "flang/AST/StmtVisitor.h"
#include "flang/AST/Type.h"
#include "flang/AST/FormatItem.h"
#include "flang/Basic/LLVM.h"
#include "llvm/Support/raw_ostream.h"
using namespace flang;

namespace {

class ASTDumper : public ConstStmtVisitor<ASTDumper> {
  raw_ostream &OS;

  int indent;
public:
  ASTDumper(raw_ostream &os) : OS(os), indent(0) {}

  // statements
  void dumpStmt(const Stmt *S);
  void dumpSubStmt(const Stmt *S) {
    dumpStmt(S);
  }

  void VisitConstructPartStmt(const ConstructPartStmt *S);
  void VisitDeclStmt(const DeclStmt *S);
  void VisitBundledCompoundStmt(const BundledCompoundStmt *S);
  void VisitProgramStmt(const ProgramStmt *S);
  void VisitEndProgramStmt(const EndProgramStmt *S);
  void VisitImplicitStmt(const ImplicitStmt *S);
  void VisitDimensionStmt(const DimensionStmt *S);
  void VisitDataStmt(const DataStmt *S);
  void VisitBlockStmt(const BlockStmt *S);
  void VisitAssignStmt(const AssignStmt *S);
  void VisitAssignedGotoStmt(const AssignedGotoStmt *S);
  void VisitGotoStmt(const GotoStmt *S);
  void VisitIfStmt(const IfStmt *S);
  void VisitDoStmt(const DoStmt *S);
  void VisitDoWhileStmt(const DoWhileStmt *S);
  void VisitContinueStmt(const ContinueStmt *S);
  void VisitStopStmt(const StopStmt *S);
  void VisitReturnStmt(const ReturnStmt *S);
  void VisitCallStmt(const CallStmt *S);
  void VisitAssignmentStmt(const AssignmentStmt *S);
  void VisitPrintStmt(const PrintStmt *S);
  void VisitWriteStmt(const WriteStmt *S);
  void VisitFormatStmt(const FormatStmt *S);
};

} // end anonymous namespace

void ASTDumper::dumpStmt(const Stmt *S) {
  for(int i = 0; i < indent; ++i)
    OS << "  ";
  Visit(S);
}

void ASTDumper::VisitConstructPartStmt(const ConstructPartStmt *S) {
  switch(S->getConstructStmtClass()) {
  case ConstructPartStmt::ElseStmtClass: OS << "else"; break;
  case ConstructPartStmt::EndIfStmtClass: OS << "end if"; break;
  case ConstructPartStmt::EndDoStmtClass: OS << "end do"; break;
  default: break;
  }
  if(S->getConstructName()) OS << ' ' << S->getConstructName()->getName();
  OS << "\n";
}

void ASTDumper::VisitDeclStmt(const DeclStmt *S) {
  S->getDeclaration()->print(OS);
  OS << "\n";
}

void ASTDumper::VisitBundledCompoundStmt(const BundledCompoundStmt *S) {
  auto Body = S->getBody();
  for(size_t I = 0; I < Body.size(); ++I) {
    if(I) OS<<"&";
    dumpStmt(Body[I]);
  }
}

void ASTDumper::VisitProgramStmt(const ProgramStmt *S) {
  const IdentifierInfo *Name = S->getProgramName();
  OS << "program";
  if (Name) OS << ":  '" << Name->getName() << "'";
  OS << "\n";
}

void ASTDumper::VisitEndProgramStmt(const EndProgramStmt *S) {
  const IdentifierInfo *Name = S->getProgramName();
  OS << "end program";
  if (Name) OS << ":  '" << Name->getName() << "'";
  OS << "\n";
}

void ASTDumper::VisitImplicitStmt(const ImplicitStmt *S) {
  OS << "implicit";
  if (S->isNone()) {
    OS << " none\n";
    return;
  }
  OS << " ";
  S->getType().print(OS);
  OS << " :: ";

  auto Spec = S->getLetterSpec();
  OS << " (" << Spec.first->getName();
  if (Spec.second)
    OS << "-" << Spec.second->getName();
  OS << ")\n";
}

void ASTDumper::VisitDimensionStmt(const DimensionStmt *S) {
  OS << "dimension " << S->getVariableName()->getNameStart()
     << "\n";
}

void ASTDumper::VisitDataStmt(const DataStmt *S) {
  OS << "data ";
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

void ASTDumper::VisitBlockStmt(const BlockStmt *S) {
  indent++;
  ArrayRef<StmtResult> Body = S->getIDList();
  for(size_t i = 0; i < Body.size(); ++i) {
    StmtResult stmt = Body[i];
    if(isa<ConstructPartStmt>(stmt.get()) && i == Body.size()-1){
      indent--;
      dumpStmt(stmt.get());
      return;
    }
    dumpStmt(stmt.get());
  }
  indent--;
}

void ASTDumper::VisitAssignStmt(const AssignStmt *S) {
  OS << "assign ";
  if(S->getAddress().Statement)
    S->getAddress().Statement->getStmtLabel()->print(OS);
  OS << " to ";
  S->getDestination()->print(OS);
  OS << "\n";
}

void ASTDumper::VisitAssignedGotoStmt(const AssignedGotoStmt *S) {
  OS << "goto ";
  S->getDestination()->print(OS);
  OS << "\n";
}

void ASTDumper::VisitGotoStmt(const GotoStmt *S) {
  OS << "goto ";
  if(S->getDestination().Statement)
    S->getDestination().Statement->getStmtLabel()->print(OS);
  OS << "\n";
}

void ASTDumper::VisitIfStmt(const IfStmt* S) {
  OS << "if ";
  S->getCondition()->print(OS);
  OS << "\n";

  if(S->getThenStmt())
    dumpSubStmt(S->getThenStmt());
  if(S->getElseStmt())
    dumpSubStmt(S->getElseStmt());
}

void ASTDumper::VisitDoStmt(const DoStmt *S) {
  OS<<"do ";
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
  OS << "\n";
  if(S->getBody())
    dumpSubStmt(S->getBody());
}

void ASTDumper::VisitDoWhileStmt(const DoWhileStmt *S) {
  OS << "do while(";
  S->getCondition()->print(OS);
  OS << ")\n";
  if(S->getBody())
    dumpSubStmt(S->getBody());
}

void ASTDumper::VisitContinueStmt(const ContinueStmt *S) {
  OS << "continue\n";
}

void ASTDumper::VisitStopStmt(const StopStmt *S) {
  if(S->getStopCode()){
    OS << "stop ";
    S->getStopCode()->print(OS);
    OS << "\n";
  } else
    OS << "stop\n";
}

void ASTDumper::VisitReturnStmt(const ReturnStmt *S) {
  if(S->getE()) {
    OS << "return ";
    S->getE()->print(OS);
    OS << "\n";
  } else OS << "return \n";
}

void ASTDumper::VisitCallStmt(const CallStmt *S) {
  OS << "call " << S->getFunction()->getName() << '(';
  auto Args = S->getArguments();
  for(size_t I = 0; I < Args.size(); ++I) {
    if(I) OS << ", ";
    Args[I]->print(OS);
  }
  OS << ")\n";
}

void ASTDumper::VisitAssignmentStmt(const AssignmentStmt *S) {
  if(S->getLHS()) S->getLHS()->print(OS);
  OS << " = ";
  if(S->getRHS()) S->getRHS()->print(OS);
  OS << "\n";
}

void ASTDumper::VisitPrintStmt(const PrintStmt *S) {
  OS << "print ";
  auto OutList = S->getIDList();
  for(size_t I = 0; I < OutList.size(); ++I) {
    if(I) OS << ", ";
    OutList[I].get()->print(OS);
  }
  OS << "\n";
}

void ASTDumper::VisitWriteStmt(const WriteStmt *S) {
  OS << "write ";
  auto OutList = S->getIDList();
  for(size_t I = 0; I < OutList.size(); ++I) {
    if(I) OS << ", ";
    OutList[I]->print(OS);
  }
  OS << "\n";
}

void ASTDumper::VisitFormatStmt(const FormatStmt *S) {
  OS << "format ";
  S->getItemList()->print(OS);
  if(S->getUnlimitedItemList())
    S->getUnlimitedItemList()->print(OS);
  OS << "\n";
}

void flang::dump(StmtResult S) {
  ASTDumper SV(llvm::errs());
  SV.dumpStmt(S.get());
}

void flang::dump(ArrayRef<StmtResult> S) {
  ASTDumper SV(llvm::errs());

  for (ArrayRef<StmtResult>::iterator I = S.begin(), E = S.end(); I != E; ++I){
    if(!I->get()) continue;
    if (!isa<ProgramStmt>(I->get()))
      SV.dumpStmt((*I).get());
  }
}
