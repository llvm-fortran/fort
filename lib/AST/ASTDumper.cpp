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

#include "flang/AST/Expr.h"
#include "flang/AST/Stmt.h"
#include "flang/AST/ExprVisitor.h"
#include "flang/AST/StmtVisitor.h"
#include "flang/AST/DeclVisitor.h"
#include "flang/AST/Type.h"
#include "flang/AST/FormatItem.h"
#include "flang/Basic/LLVM.h"
#include "llvm/Support/raw_ostream.h"
using namespace flang;

namespace {

class ASTDumper : public ConstStmtVisitor<ASTDumper>,
  public ConstExprVisitor<ASTDumper>,
  public ConstDeclVisitor<ASTDumper> {
  raw_ostream &OS;

  int indent;
public:
  ASTDumper(raw_ostream &os) : OS(os), indent(0) {}

  // utilities
  void dumpCompoundPartStart(const char *Name);
  void dumpCompoundPartEnd();
  void dumpIndent();

  // declarations
  void dumpDecl(const Decl *D);
  void dumpDeclContext(const DeclContext *Ctx);
  void VisitTranslationUnitDecl(const TranslationUnitDecl *D);
  void VisitMainProgramDecl(const MainProgramDecl *D);
  void VisitFunctionDecl(const FunctionDecl *D);
  void VisitVarDecl(const VarDecl *D);
  void VisitReturnVarDecl(const ReturnVarDecl *D);

  // statements
  void dumpStmt(const Stmt *S);
  void dumpSubStmt(const Stmt *S);

  void VisitConstructPartStmt(const ConstructPartStmt *S);
  void VisitDeclStmt(const DeclStmt *S);
  void VisitCompoundStmt(const CompoundStmt *S);
  void VisitProgramStmt(const ProgramStmt *S);
  void VisitEndProgramStmt(const EndProgramStmt *S);
  void VisitParameterStmt(const ParameterStmt *S);
  void VisitImplicitStmt(const ImplicitStmt *S);
  void VisitDimensionStmt(const DimensionStmt *S);
  void VisitExternalStmt(const ExternalStmt *S);
  void VisitIntrinsicStmt(const IntrinsicStmt *S);
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

  // expressions
  void dumpExpr(const Expr *E);
  void dumpExprList(ArrayRef<Expr*> List);
  void dumpExprOrNull(const Expr *E) {
    if(E) dumpExpr(E);
  }

  void VisitIntegerConstantExpr(const IntegerConstantExpr *E);
  void VisitRealConstantExpr(const RealConstantExpr *E);
  void VisitComplexConstantExpr(const ComplexConstantExpr *E);
  void VisitCharacterConstantExpr(const CharacterConstantExpr *E);
  void VisitBOZConstantExpr(const BOZConstantExpr *E);
  void VisitLogicalConstantExpr(const LogicalConstantExpr *E);
  void VisitRepeatedConstantExpr(const RepeatedConstantExpr *E);
  void VisitVarExpr(const VarExpr *E);
  void VisitUnresolvedIdentifierExpr(const UnresolvedIdentifierExpr *E);
  void VisitReturnedValueExpr(const ReturnedValueExpr *E);
  void VisitUnaryExpr(const UnaryExpr *E);
  void VisitDefinedUnaryOperatorExpr(const DefinedUnaryOperatorExpr *E);
  void VisitImplicitCastExpr(const ImplicitCastExpr *E);
  void VisitBinaryExpr(const BinaryExpr *E);
  void VisitDefinedBinaryOperatorExpr(const DefinedBinaryOperatorExpr *E);
  void VisitSubstringExpr(const SubstringExpr *E);
  void VisitArrayElementExpr(const ArrayElementExpr *E);
  void VisitCallExpr(const CallExpr *E);
  void VisitIntrinsicCallExpr(const IntrinsicCallExpr *E);
  void VisitImpliedDoExpr(const ImpliedDoExpr *E);
  void VisitArrayConstructorExpr(const ArrayConstructorExpr *E);

  // array specification
  void dumpArraySpec(const ArraySpec *S);

};

} // end anonymous namespace

// utilities


void ASTDumper::dumpCompoundPartStart(const char *Name) {
  OS << Name << ' ';
}

void ASTDumper::dumpCompoundPartEnd() {
  OS << "\n";
}

void ASTDumper::dumpIndent() {
  for(int i = 0; i < indent; ++i)
    OS << "  ";
}

// declarations

void ASTDumper::dumpDecl(const Decl *D) {
  dumpIndent();
  ConstDeclVisitor<ASTDumper>::Visit(D);
}

void ASTDumper::dumpDeclContext(const DeclContext *Ctx) {
  auto I = Ctx->decls_begin();
  for(auto E = Ctx->decls_end(); I!=E; ++I) {
    if((*I)->getDeclContext() == Ctx)
      dumpDecl(*I);
  }
}

void ASTDumper::VisitTranslationUnitDecl(const TranslationUnitDecl *D) {
  dumpDeclContext(D);
}

void ASTDumper::VisitMainProgramDecl(const MainProgramDecl *D) {
  OS << "program " << D->getName() << "\n";
  indent++;
  dumpDeclContext(D);
  indent--;
  if(D->getBody())
    dumpSubStmt(D->getBody());
}

void ASTDumper::VisitFunctionDecl(const FunctionDecl *D) {
  if(!D->getType().isNull()) {
    D->getType().print(OS);
    OS << ' ';
  }
  OS << (D->isNormalFunction()? "function " : "subroutine ")
     << D->getName() << "(";
  auto Args = D->getArguments();
  for(size_t I = 0; I < Args.size(); ++I) {
    if(I) OS << ", ";
    OS << cast<VarDecl>(Args[I])->getName();
  }
  OS << ")\n";
  indent++;
  dumpDeclContext(D);
  indent--;
  if(D->getBody())
    dumpSubStmt(D->getBody());
}

void ASTDumper::VisitVarDecl(const VarDecl *D) {
  if(!D->getType().isNull()) {
    D->getType().print(OS);
    OS << ' ';
  }
  OS << D->getName() << "\n";
}

void ASTDumper::VisitReturnVarDecl(const ReturnVarDecl *D) {
  OS << "return var\n";
}

// statements

void ASTDumper::dumpStmt(const Stmt *S) {
  dumpIndent();
  ConstStmtVisitor<ASTDumper>::Visit(S);
}

void ASTDumper::dumpSubStmt(const Stmt *S) {
  if(isa<BlockStmt>(S))
    dumpStmt(S);
  else {
    ++indent;
    dumpStmt(S);
    --indent;
  }
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
  dumpDecl(S->getDeclaration());
  OS << "\n";
}

void ASTDumper::VisitCompoundStmt(const CompoundStmt *S) {
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

void ASTDumper::VisitParameterStmt(const ParameterStmt *S) {
  dumpCompoundPartStart("parameter");
  OS << S->getIdentifier()->getName() << " = ";
  dumpExpr(S->getValue());
  dumpCompoundPartEnd();
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

void ASTDumper::VisitExternalStmt(const ExternalStmt *S) {
  dumpCompoundPartStart("external");
  OS << S->getIdentifier()->getName();
  dumpCompoundPartEnd();
}

void ASTDumper::VisitIntrinsicStmt(const IntrinsicStmt *S) {
  dumpCompoundPartStart("intrinsic");
  OS << S->getIdentifier()->getName();
  dumpCompoundPartEnd();
}

void ASTDumper::VisitDataStmt(const DataStmt *S) {
  OS << "data ";
  dumpExprList(S->getNames());
  OS << " / ";
  dumpExprList(S->getValues());
  OS << " /";
  if(S->getBody())
    dumpStmt(S->getBody());
}

void ASTDumper::VisitBlockStmt(const BlockStmt *S) {
  indent++;
  auto Body = S->getStatements();
  for(size_t I = 0; I < Body.size(); ++I) {
    auto S = Body[I];
    if(isa<ConstructPartStmt>(S) && I == Body.size()-1){
      indent--;
      dumpStmt(S);
      return;
    }
    dumpStmt(S);
  }
  indent--;
}

void ASTDumper::VisitAssignStmt(const AssignStmt *S) {
  OS << "assign ";
  if(S->getAddress().Statement)
    dumpExpr(S->getAddress().Statement->getStmtLabel());
  OS << " to ";
  dumpExpr(S->getDestination());
  OS << "\n";
}

void ASTDumper::VisitAssignedGotoStmt(const AssignedGotoStmt *S) {
  OS << "goto ";
  dumpExpr(S->getDestination());
  OS << "\n";
}

void ASTDumper::VisitGotoStmt(const GotoStmt *S) {
  OS << "goto ";
  if(S->getDestination().Statement)
    dumpExpr(S->getDestination().Statement->getStmtLabel());
  OS << "\n";
}

void ASTDumper::VisitIfStmt(const IfStmt* S) {
  OS << "if ";
  dumpExpr(S->getCondition());
  OS << "\n";

  if(S->getThenStmt())
    dumpSubStmt(S->getThenStmt());
  if(S->getElseStmt())
    dumpSubStmt(S->getElseStmt());
}

void ASTDumper::VisitDoStmt(const DoStmt *S) {
  OS<<"do ";
  if(S->getTerminatingStmt().Statement)
    dumpExpr(S->getTerminatingStmt().Statement->getStmtLabel());
  OS << " ";
  dumpExpr(S->getDoVar());
  OS << " = ";
  dumpExpr(S->getInitialParameter());
  OS << ", ";
  dumpExpr(S->getTerminalParameter());
  if(S->getIncrementationParameter()) {
    OS << ", ";
    dumpExpr(S->getIncrementationParameter());
  }
  OS << "\n";
  if(S->getBody())
    dumpSubStmt(S->getBody());
}

void ASTDumper::VisitDoWhileStmt(const DoWhileStmt *S) {
  OS << "do while(";
  dumpExpr(S->getCondition());
  OS << ")\n";
  if(S->getBody())
    dumpSubStmt(S->getBody());
}

void ASTDumper::VisitContinueStmt(const ContinueStmt *S) {
  OS << "continue\n";
}

void ASTDumper::VisitStopStmt(const StopStmt *S) {
  OS << "stop ";
  dumpExprOrNull(S->getStopCode());
  OS << "\n";
}

void ASTDumper::VisitReturnStmt(const ReturnStmt *S) {
  OS << "return ";
  dumpExprOrNull(S->getE());
  OS << "\n";
}

void ASTDumper::VisitCallStmt(const CallStmt *S) {
  OS << "call " << S->getFunction()->getName() << '(';
  dumpExprList(S->getArguments());
  OS << ")\n";
}

void ASTDumper::VisitAssignmentStmt(const AssignmentStmt *S) {
  dumpExprOrNull(S->getLHS());
  OS << " = ";
  dumpExprOrNull(S->getRHS());
  OS << "\n";
}

void ASTDumper::VisitPrintStmt(const PrintStmt *S) {
  OS << "print ";
  dumpExprList(S->getOutputList());
  OS << "\n";
}

void ASTDumper::VisitWriteStmt(const WriteStmt *S) {
  OS << "write ";
  dumpExprList(S->getOutputList());
  OS << "\n";
}

void ASTDumper::VisitFormatStmt(const FormatStmt *S) {
  OS << "format ";
  S->getItemList()->print(OS);
  if(S->getUnlimitedItemList())
    S->getUnlimitedItemList()->print(OS);
  OS << "\n";
}

// expressions

void ASTDumper::dumpExpr(const Expr *E) {
  ConstExprVisitor<ASTDumper>::Visit(E);
}

void ASTDumper::dumpExprList(ArrayRef<Expr*> List) {
  for(size_t I = 0; I < List.size(); ++I) {
    if(I) OS << ", ";
    dumpExpr(List[I]);
  }
}

void ASTDumper::VisitIntegerConstantExpr(const IntegerConstantExpr *E) {
  OS << E->getValue();
}

void ASTDumper::VisitRealConstantExpr(const RealConstantExpr *E)  {
  llvm::SmallVector<char, 32> Str;
  E->getValue().toString(Str);
  Str.push_back('\0');
  OS << Str.begin();
}

void ASTDumper::VisitComplexConstantExpr(const ComplexConstantExpr *E)  {
  llvm::SmallVector<char,32> ReStr;
  E->getRealValue().toString(ReStr);
  ReStr.push_back('\0');
  llvm::SmallVector<char,32> ImStr;
  E->getImaginaryValue().toString(ImStr);
  ImStr.push_back('\0');
  OS << '(' << ReStr.begin() << ',' << ImStr.begin() << ')';
}

void ASTDumper::VisitCharacterConstantExpr(const CharacterConstantExpr *E)  {
  OS << E->getValue();
}

void ASTDumper::VisitBOZConstantExpr(const BOZConstantExpr *E) {

}

void ASTDumper::VisitLogicalConstantExpr(const LogicalConstantExpr *E)  {
  OS << (E->isTrue()? "true" : "false");
}

void ASTDumper::VisitRepeatedConstantExpr(const RepeatedConstantExpr *E)  {
  OS << E->getRepeatCount() << "*";
  dumpExpr(E->getExpression());
}

void ASTDumper::VisitVarExpr(const VarExpr *E) {
  OS << E->getVarDecl()->getName();
}

void ASTDumper::VisitUnresolvedIdentifierExpr(const UnresolvedIdentifierExpr *E) {
  OS << E->getIdentifier()->getName();
}

void ASTDumper::VisitReturnedValueExpr(const ReturnedValueExpr *E) {
  OS << E->getFuncDecl()->getName();
}

void ASTDumper::VisitUnaryExpr(const UnaryExpr *E) {
  OS << '(';
  const char *op = "";
  switch (E->getOperator()) {
  default: break;
  case UnaryExpr::Not:   op = ".NOT."; break;
  case UnaryExpr::Plus:  op = "+";     break;
  case UnaryExpr::Minus: op = "-";     break;
  }
  OS << op;
  dumpExpr(E->getExpression());
  OS << ')';
}

void ASTDumper::VisitDefinedUnaryOperatorExpr(const DefinedUnaryOperatorExpr *E) {
  OS << '(' << E->getIdentifierInfo()->getName();
  dumpExpr(E->getExpression());
  OS << ')';
}

void ASTDumper::VisitImplicitCastExpr(const ImplicitCastExpr *E) {
  auto Type = E->getType();
  if(Type->isIntegerType())
    OS << "INT(";
  else if(Type->isRealType())
    OS << "REAL(";
  else if(Type->isComplexType())
    OS << "CMPLX(";
  dumpExpr(E->getExpression());
  if(const ExtQuals *Ext = Type.getExtQualsPtrOrNull())
    OS << ",Kind=" << BuiltinType::getTypeKindString(Ext->getKindSelector());
  OS << ')';
}

void ASTDumper::VisitBinaryExpr(const BinaryExpr *E) {
  OS << '(';
  dumpExpr(E->getLHS());
  const char *op = 0;
  switch (E->getOperator()) {
  default: break;
  case BinaryExpr::Eqv:              op = ".EQV.";  break;
  case BinaryExpr::Neqv:             op = ".NEQV."; break;
  case BinaryExpr::Or:               op = ".OR.";   break;
  case BinaryExpr::And:              op = ".AND.";  break;
  case BinaryExpr::Equal:            op = "==";     break;
  case BinaryExpr::NotEqual:         op = "/=";     break;
  case BinaryExpr::LessThan:         op = "<";      break;
  case BinaryExpr::LessThanEqual:    op = "<=";     break;
  case BinaryExpr::GreaterThan:      op = ">";      break;
  case BinaryExpr::GreaterThanEqual: op = ">=";     break;
  case BinaryExpr::Concat:           op = "//";     break;
  case BinaryExpr::Plus:             op = "+";      break;
  case BinaryExpr::Minus:            op = "-";      break;
  case BinaryExpr::Multiply:         op = "*";      break;
  case BinaryExpr::Divide:           op = "/";      break;
  case BinaryExpr::Power:            op = "**";     break;
  }
  OS << op;
  dumpExpr(E->getRHS());
  OS << ')';
}

void ASTDumper::VisitDefinedBinaryOperatorExpr(const DefinedBinaryOperatorExpr *E) {
  OS << '(';
  dumpExpr(E->getLHS());
  OS << E->getIdentifierInfo()->getName();
  dumpExpr(E->getRHS());
  OS << ')';
}

void ASTDumper::VisitSubstringExpr(const SubstringExpr *E) {
  dumpExpr(E->getTarget());
  OS << '(';
  dumpExprOrNull(E->getStartingPoint());
  OS << ':';
  dumpExprOrNull(E->getEndPoint());
  OS << ')';
}

void ASTDumper::VisitArrayElementExpr(const ArrayElementExpr *E) {
  dumpExpr(E->getTarget());
  OS << '(';
  dumpExprList(E->getArguments());
  OS << ')';
}

void ASTDumper::VisitCallExpr(const CallExpr *E) {
  OS << E->getFunction()->getName() << '(';
  dumpExprList(E->getArguments());
  OS << ')';
}

void ASTDumper::VisitIntrinsicCallExpr(const IntrinsicCallExpr *E) {
  OS << intrinsic::getFunctionName(E->getIntrinsicFunction()) << '(';
  dumpExprList(E->getArguments());
  OS << ')';
}

void ASTDumper::VisitImpliedDoExpr(const ImpliedDoExpr *E) {
  OS << '(';
  dumpExprList(E->getBody());
  OS << ", " << E->getVarDecl()->getIdentifier()->getName();
  OS << " = ";
  dumpExpr(E->getInitialParameter());
  OS << ", ";
  dumpExpr(E->getTerminalParameter());
  if(E->getIncrementationParameter()) {
     OS << ", ";
     dumpExpr(E->getIncrementationParameter());
  }
  OS << ')';
}

void ASTDumper::VisitArrayConstructorExpr(const ArrayConstructorExpr *E) {
  OS << "(/";
  dumpExprList(E->getItems());
  OS << " /)";
}

// array specification
void ASTDumper::dumpArraySpec(const ArraySpec *S) {
  if(auto Explicit = dyn_cast<ExplicitShapeSpec>(S)) {
    if(Explicit->getLowerBound()) {
      dumpExpr(Explicit->getLowerBound());
      OS << ':';
    }
    dumpExpr(Explicit->getUpperBound());
  } else if(auto Implied = dyn_cast<ImpliedShapeSpec>(S)) {
    if(Implied->getLowerBound()) {
      dumpExpr(Implied ->getLowerBound());
      OS << ':';
    }
    OS << '*';
  } else OS << "<unknown array spec>";
}

namespace flang {

void Decl::dump() const {
  dump(llvm::errs());
}
void Decl::dump(llvm::raw_ostream &OS) const {
  ASTDumper SV(OS);
  SV.dumpDecl(this);
}

void Stmt::dump() const {
  dump(llvm::errs());
}
void Stmt::dump(llvm::raw_ostream &OS) const {
  ASTDumper SV(OS);
  SV.dumpStmt(this);
}

void Expr::dump() const {
  dump(llvm::errs());
}
void Expr::dump(llvm::raw_ostream &OS) const {
  ASTDumper SV(OS);
  SV.dumpExpr(this);
}

void ArraySpec::dump() const {
  dump(llvm::errs());
}
void ArraySpec::dump(llvm::raw_ostream &OS) const {
  ASTDumper SV(OS);
  SV.dumpArraySpec(this);
}

}
