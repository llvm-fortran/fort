//===--- Expr.cpp - Fortran Expressions -----------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "flang/AST/Expr.h"
#include "flang/AST/ASTContext.h"
#include "flang/AST/Decl.h"
#include "llvm/ADT/APSInt.h"
#include "llvm/ADT/StringRef.h"

namespace flang {

void APNumericStorage::setIntValue(ASTContext &C, const APInt &Val) {
  if (hasAllocation())
    C.Deallocate(pVal);

  BitWidth = Val.getBitWidth();
  unsigned NumWords = Val.getNumWords();
  const uint64_t* Words = Val.getRawData();
  if (NumWords > 1) {
    pVal = new (C) uint64_t[NumWords];
    std::copy(Words, Words + NumWords, pVal);
  } else if (NumWords == 1)
    VAL = Words[0];
  else
    VAL = 0;
}

SourceLocation ConstantExpr::getLocEnd() const {
  return MaxLoc;
}

IntegerConstantExpr::IntegerConstantExpr(ASTContext &C, SourceLocation Loc,
                                         SourceLocation MaxLoc, llvm::StringRef Data)
  : ConstantExpr(IntegerConstant, C.IntegerTy,  Loc, MaxLoc) {
  llvm::APInt Val(64,Data,10);
  Num.setValue(C, Val);
}

IntegerConstantExpr *IntegerConstantExpr::Create(ASTContext &C, SourceLocation Loc,
                                                 SourceLocation MaxLoc, llvm::StringRef Data) {
  return new (C) IntegerConstantExpr(C, Loc, MaxLoc, Data);
}

RealConstantExpr::RealConstantExpr(ASTContext &C, SourceLocation Loc,
                                   SourceLocation MaxLoc, llvm::StringRef Data,
                                   Kind kind)
  : ConstantExpr(RealConstant, kind == Kind4? C.RealTy : C.DoublePrecisionTy, Loc, MaxLoc) {
  APFloat Val(kind == Kind4? APFloat::IEEEsingle : APFloat::IEEEdouble, Data);
  Num.setValue(C, Val);
}

RealConstantExpr *RealConstantExpr::Create(ASTContext &C, SourceLocation Loc,
                                           SourceLocation MaxLoc, llvm::StringRef Data,
                                           Kind kind) {
  return new (C) RealConstantExpr(C, Loc, MaxLoc, Data, kind);
}

ComplexConstantExpr::ComplexConstantExpr(ASTContext &C, SourceLocation Loc, SourceLocation MaxLoc,
                                         const APFloat &Re, const APFloat &Im, Kind kind)
  : ConstantExpr(ComplexConstant, kind == Kind4? C.ComplexTy : C.DoubleComplexTy, Loc, MaxLoc) {
  this->Re.setValue(C, Re);
  this->Im.setValue(C, Im);
}

ComplexConstantExpr *ComplexConstantExpr::Create(ASTContext &C, SourceLocation Loc,
                                                 SourceLocation MaxLoc,
                                                 const APFloat &Re, const APFloat &Im,
                                                 Kind kind) {
  return new (C) ComplexConstantExpr(C, Loc, MaxLoc, Re, Im, kind);
}

CharacterConstantExpr::CharacterConstantExpr(ASTContext &C, SourceLocation Loc,
                                             SourceLocation MaxLoc, llvm::StringRef data)
  : ConstantExpr(CharacterConstant, C.CharacterTy, Loc, MaxLoc) {
  // TODO: A 'kind' on a character literal constant.
  Data = new (C) char[data.size() + 1];
  std::strncpy(Data, data.data(), data.size());
  Data[data.size()] = '\0';
}

CharacterConstantExpr *CharacterConstantExpr::Create(ASTContext &C, SourceLocation Loc,
                                                     SourceLocation MaxLoc,
                                                     llvm::StringRef Data) {
  return new (C) CharacterConstantExpr(C, Loc, MaxLoc, Data);
}

BOZConstantExpr::BOZConstantExpr(ASTContext &C, SourceLocation Loc,
                                 SourceLocation MaxLoc, llvm::StringRef Data)
  : ConstantExpr(BOZConstant, C.IntegerTy, Loc, MaxLoc) {
  unsigned Radix = 0;
  switch (Data[0]) {
  case 'B':
    Kind = Binary;
    Radix = 2;
    break;
  case 'O':
    Kind = Octal;
    Radix = 8;
    break;
  case 'Z': case 'X':
    Kind = Hexadecimal;
    Radix = 16;
    break;
  }

  size_t LastQuote = Data.rfind(Data[1]);
  assert(LastQuote == llvm::StringRef::npos && "Invalid BOZ constant!");
  llvm::StringRef NumStr = Data.slice(2, LastQuote);
  APInt Val;
  NumStr.getAsInteger(Radix, Val);
  Num.setValue(C, Val);
}

BOZConstantExpr *BOZConstantExpr::Create(ASTContext &C, SourceLocation Loc,
                                         SourceLocation MaxLoc, llvm::StringRef Data) {
  return new (C) BOZConstantExpr(C, Loc, MaxLoc, Data);
}

LogicalConstantExpr::LogicalConstantExpr(ASTContext &C, SourceLocation Loc,
                                         SourceLocation MaxLoc, llvm::StringRef Data)
  : ConstantExpr(LogicalConstant, C.LogicalTy, Loc, MaxLoc) {
  Val = (Data.compare_lower(".TRUE.") == 0);
}

LogicalConstantExpr *LogicalConstantExpr::Create(ASTContext &C, SourceLocation Loc,
                                                 SourceLocation MaxLoc, llvm::StringRef Data) {
  return new (C) LogicalConstantExpr(C, Loc, MaxLoc, Data);
}

RepeatedConstantExpr::RepeatedConstantExpr(SourceLocation Loc,
                                           IntegerConstantExpr *Repeat,
                                           Expr *Expression)
  : Expr(RepeatedConstant, Expression->getType(), Loc),
    RepeatCount(Repeat), E(Expression) {
}

RepeatedConstantExpr *RepeatedConstantExpr::Create(ASTContext &C, SourceLocation Loc,
                                                   IntegerConstantExpr *RepeatCount,
                                                   Expr* Expression) {
  return new (C) RepeatedConstantExpr(Loc, RepeatCount, Expression);
}

SourceLocation RepeatedConstantExpr::getLocStart() const {
  return RepeatCount->getLocStart();
}
SourceLocation RepeatedConstantExpr::getLocEnd() const {
  return E->getLocEnd();
}

MultiArgumentExpr::MultiArgumentExpr(ASTContext &C, ArrayRef<Expr*> Args) {
  assert(Args.size() > 0);
  NumArguments = Args.size();
  if(NumArguments == 1)
    Argument = Args[0];
  else {
    Arguments = new (C) Expr *[NumArguments];
    for (unsigned I = 0; I != NumArguments; ++I)
      Arguments[I] = Args[I];
  }
}

SubstringExpr::SubstringExpr(ASTContext &C, SourceLocation Loc, Expr *E,
                             Expr *Start, Expr *End)
  : DesignatorExpr(Loc,C.CharacterTy,DesignatorExpr::Substring),
    Target(E), StartingPoint(Start), EndPoint(End) {
}

SubstringExpr *SubstringExpr::Create(ASTContext &C, SourceLocation Loc,
                                     Expr *Target, Expr *StartingPoint,
                                     Expr *EndPoint) {
  return new(C) SubstringExpr(C, Loc, Target, StartingPoint, EndPoint);
}

SourceLocation SubstringExpr::getLocStart() const {
  return Target->getLocStart();
}

SourceLocation SubstringExpr::getLocEnd() const {
  if(EndPoint) return EndPoint->getLocEnd();
  else if(StartingPoint) return StartingPoint->getLocEnd();
  else return getLocation();
}

ArrayElementExpr::ArrayElementExpr(ASTContext &C, SourceLocation Loc, Expr *E,
                                   llvm::ArrayRef<Expr *> Subs)
  : DesignatorExpr(Loc, E->getType()->asArrayType()->getElementType(),
                   DesignatorExpr::ArrayElement),
    MultiArgumentExpr(C, Subs), Target(E) {
}

ArrayElementExpr *ArrayElementExpr::Create(ASTContext &C, SourceLocation Loc,
                                           Expr *Target,
                                           llvm::ArrayRef<Expr *> Subscripts) {
  return new(C) ArrayElementExpr(C, Loc, Target, Subscripts);
}

SourceLocation ArrayElementExpr::getLocStart() const {
  return Target->getLocStart();
}

SourceLocation ArrayElementExpr::getLocEnd() const {
  return getArguments().back()->getLocEnd();
}

VarExpr::VarExpr(SourceLocation Loc, const VarDecl *Var)
  : DesignatorExpr(Loc, Var->getType(), DesignatorExpr::ObjectName),
    Variable(Var) {}

VarExpr *VarExpr::Create(ASTContext &C, SourceLocation Loc, const VarDecl *VD) {
  return new (C) VarExpr(Loc, VD);
}

SourceLocation VarExpr::getLocEnd() const {
  return SourceLocation::getFromPointer(getLocation().getPointer() +
                               Variable->getIdentifier()->getLength());
}

ReturnedValueExpr::ReturnedValueExpr(SourceLocation Loc, FunctionDecl *F)
  : Expr(ReturnedValue, F->getType(), Loc),Func(F) {
}

ReturnedValueExpr *ReturnedValueExpr::Create(ASTContext &C, SourceLocation Loc,
                                             FunctionDecl *Func) {
  return new(C) ReturnedValueExpr(Loc, Func);
}

SourceLocation ReturnedValueExpr::getLocEnd() const {
  return SourceLocation::getFromPointer(getLocation().getPointer() +
                               Func->getIdentifier()->getLength());
}

UnresolvedIdentifierExpr::UnresolvedIdentifierExpr(ASTContext &C,
                                                   SourceLocation Loc,
                                                   const IdentifierInfo *ID)
  : Expr(UnresolvedIdentifier, C.IntegerTy, Loc), IDInfo(ID) {
}

UnresolvedIdentifierExpr *UnresolvedIdentifierExpr::Create(ASTContext &C,
                                                           SourceLocation Loc,
                                                           const IdentifierInfo *IDInfo) {
  return new(C) UnresolvedIdentifierExpr(C, Loc, IDInfo);
}

SourceLocation UnresolvedIdentifierExpr::getLocEnd() const {
  return SourceLocation::getFromPointer(getLocation().getPointer() +
                                        IDInfo->getLength());
}


UnaryExpr *UnaryExpr::Create(ASTContext &C, SourceLocation Loc, Operator Op,
                             Expr *E) {
  return new (C) UnaryExpr(Expr::Unary,
                           (Op != Not) ? E->getType() : C.LogicalTy,
                           Loc, Op, E);
}

SourceLocation UnaryExpr::getLocEnd() const {
  return E->getLocEnd();
}

DefinedOperatorUnaryExpr::DefinedOperatorUnaryExpr(SourceLocation Loc, Expr *E,
                                                   IdentifierInfo *IDInfo)
  : UnaryExpr(Expr::DefinedUnaryOperator, E->getType(), Loc, Defined, E),
    II(IDInfo) {}

DefinedOperatorUnaryExpr *DefinedOperatorUnaryExpr::Create(ASTContext &C,
                                                           SourceLocation Loc,
                                                           Expr *E,
                                                           IdentifierInfo *IDInfo) {
  return new (C) DefinedOperatorUnaryExpr(Loc, E, IDInfo);
}

BinaryExpr *BinaryExpr::Create(ASTContext &C, SourceLocation Loc, Operator Op,
                               QualType Type, Expr *LHS, Expr *RHS) {
  return new (C) BinaryExpr(Expr::Binary, Type, Loc, Op, LHS, RHS);
}

SourceLocation BinaryExpr::getLocStart() const {
  return LHS->getLocStart();
}

SourceLocation BinaryExpr::getLocEnd() const {
  return RHS->getLocEnd();
}

DefinedOperatorBinaryExpr *
DefinedOperatorBinaryExpr::Create(ASTContext &C, SourceLocation Loc, Expr *LHS,
                                  Expr *RHS, IdentifierInfo *IDInfo) {
  return new (C) DefinedOperatorBinaryExpr(Loc, LHS, RHS, IDInfo);
}

ImplicitCastExpr::ImplicitCastExpr(SourceLocation Loc, QualType Dest, Expr *e)
  : Expr(ImplicitCast,Dest,Loc),E(e) {
}

ImplicitCastExpr *ImplicitCastExpr::Create(ASTContext &C, SourceLocation Loc,
                                           QualType Dest, Expr *E) {
  return new(C) ImplicitCastExpr(Loc, Dest, E);
}

SourceLocation ImplicitCastExpr::getLocStart() const {
  return E->getLocStart();
}

SourceLocation ImplicitCastExpr::getLocEnd() const {
  return E->getLocEnd();
}

CallExpr::CallExpr(ASTContext &C, SourceLocation Loc,
                   FunctionDecl *Func, ArrayRef<Expr*> Args)
  : Expr(Call, Func->getType(), Loc), MultiArgumentExpr(C, Args),
    Function(Func) {
}

CallExpr *CallExpr::Create(ASTContext &C, SourceLocation Loc,
                           FunctionDecl *Func, ArrayRef<Expr*> Args) {
  return new(C) CallExpr(C, Loc, Func, Args);
}

SourceLocation CallExpr::getLocEnd() const {
  return getArguments().back()->getLocEnd();
}

IntrinsicFunctionCallExpr::
IntrinsicFunctionCallExpr(ASTContext &C, SourceLocation Loc,
                          intrinsic::FunctionKind Func,
                          ArrayRef<Expr*> Args,
                          QualType ReturnType)
  : Expr(IntrinsicFunctionCall, ReturnType, Loc),
    MultiArgumentExpr(C, Args), Function(Func) {
}

IntrinsicFunctionCallExpr *IntrinsicFunctionCallExpr::
Create(ASTContext &C, SourceLocation Loc,
       intrinsic::FunctionKind Func,
       ArrayRef<Expr*> Arguments,
       QualType ReturnType) {
  return new(C) IntrinsicFunctionCallExpr(C, Loc, Func, Arguments,
                                          ReturnType);
}

SourceLocation IntrinsicFunctionCallExpr::getLocEnd() const {
  return getArguments().back()->getLocEnd();
}

ImpliedDoExpr::ImpliedDoExpr(ASTContext &C, SourceLocation Loc,
                             VarDecl *Var, ArrayRef<Expr*> Body,
                             Expr *InitialParam, Expr *TerminalParam,
                             Expr *IncrementationParam)
  : Expr(ImpliedDo, QualType(), Loc), DoVar(Var),
    DoList(C, Body), Init(InitialParam), Terminate(TerminalParam),
    Increment(IncrementationParam) {
}

ImpliedDoExpr *ImpliedDoExpr::Create(ASTContext &C, SourceLocation Loc,
                                     VarDecl *DoVar, ArrayRef<Expr*> Body,
                                     Expr *InitialParam, Expr *TerminalParam,
                                     Expr *IncrementationParam) {
  return new(C) ImpliedDoExpr(C, Loc, DoVar, Body, InitialParam,
                              TerminalParam, IncrementationParam);
}

SourceLocation ImpliedDoExpr::getLocEnd() const {
  return Terminate->getLocEnd();
}

//===----------------------------------------------------------------------===//
// Expression Print Statements
//===----------------------------------------------------------------------===//

void Expr::dump() {
  this->print(llvm::outs());
}

void Expr::print(llvm::raw_ostream &O) {
}

void DesignatorExpr::print(llvm::raw_ostream &O) {
}

void SubstringExpr::print(llvm::raw_ostream &O) {
  Target->print(O);
  O << '(';
  if(StartingPoint) StartingPoint->print(O);
  O << ':';
  if(EndPoint) EndPoint->print(O);
  O << ')';
}

void ArrayElementExpr::print(llvm::raw_ostream &O) {
  Target->print(O);
  O << '(';
  auto Subscripts = getArguments();
  for(size_t I = 0; I < Subscripts.size(); ++I) {
    if(I) O << ", ";
    Subscripts[I]->print(O);
  }
  O << ')';
}

void UnaryExpr::print(llvm::raw_ostream &O) {
  O << '(';
  const char *op = "";
  switch (Op) {
  default: break;
  case Not:   op = ".NOT."; break;
  case Plus:  op = "+";     break;
  case Minus: op = "-";     break;
  }
  O << op;
  E->print(O);
  O << ')';
}

void DefinedOperatorUnaryExpr::print(llvm::raw_ostream &O) {
  O << '(' << II->getName();
  E->print(O);
  O << ')';
}

void ImplicitCastExpr::print(llvm::raw_ostream &O) {
  auto Type = getType();
  if(Type->isIntegerType())
    O << "INT(";
  else if(Type->isRealType())
    O << "REAL(";
  else if(Type->isComplexType())
    O << "CMPLX(";
  E->print(O);
  if(const ExtQuals *Ext = Type.getExtQualsPtrOnNull())
    O << ",Kind=" << Ext->getRawKindSelector();
  O << ')';
}

void CallExpr::print(llvm::raw_ostream &O) {
  O << Function->getName() << '(';
  auto Args = getArguments();
  for(size_t I = 0; I < Args.size(); ++I) {
    if(I) O << ", ";
    Args[I]->print(O);
  }
  O << ')';
}

void IntrinsicFunctionCallExpr::print(llvm::raw_ostream &O) {
  O << intrinsic::getFunctionName(Function) << '(';
  auto Args = getArguments();
  for(size_t I = 0; I < Args.size(); ++I) {
    if(I) O << ", ";
    Args[I]->print(O);
  }
  O << ')';
}

void ConstantExpr::print(llvm::raw_ostream &O) {
  if (Kind)
    O << '_' << Kind; 
}

void IntegerConstantExpr::print(llvm::raw_ostream &O) {
  O << Num.getValue();
}

void RealConstantExpr::print(llvm::raw_ostream &O) {
  llvm::SmallVector<char,32> Str;
  Num.getValue().toString(Str);
  Str.push_back('\0');
  O << Str.begin();
}

void ComplexConstantExpr::print(llvm::raw_ostream &O) {
  llvm::SmallVector<char,32> ReStr;
  Re.getValue().toString(ReStr);
  ReStr.push_back('\0');
  llvm::SmallVector<char,32> ImStr;
  Im.getValue().toString(ImStr);
  ImStr.push_back('\0');
  O << '(' << ReStr.begin() << ',' << ImStr.begin() << ')';
}

void CharacterConstantExpr::print(llvm::raw_ostream &O) {
  O << getValue();
}

void LogicalConstantExpr::print(llvm::raw_ostream &O) {
  O << (isTrue()? "true" : "false");
}

void RepeatedConstantExpr::print(llvm::raw_ostream &O) {
  O << getRepeatCount() << "*";
  getExpression()->print(O);
}

void VarExpr::print(llvm::raw_ostream &O) {
  O << *Variable;
}

void ReturnedValueExpr::print(llvm::raw_ostream &O) {
  O << Func->getIdentifier()->getName();
}

void UnresolvedIdentifierExpr::print(llvm::raw_ostream &O) {
  O << getIdentifier();
}

void BinaryExpr::print(llvm::raw_ostream &O) {
  O << '(';
  LHS->print(O);
  const char *op = 0;
  switch (Op) {
  default: break;
  case Eqv:              op = ".EQV.";  break;
  case Neqv:             op = ".NEQV."; break;
  case Or:               op = ".OR.";   break;
  case And:              op = ".AND.";  break;
  case Equal:            op = "==";     break;
  case NotEqual:         op = "/=";     break;
  case LessThan:         op = "<";      break;
  case LessThanEqual:    op = "<=";     break;
  case GreaterThan:      op = ">";      break;
  case GreaterThanEqual: op = ">=";     break;
  case Concat:           op = "//";     break;
  case Plus:             op = "+";      break;
  case Minus:            op = "-";      break;
  case Multiply:         op = "*";      break;
  case Divide:           op = "/";      break;
  case Power:            op = "**";     break;
  }
  O << op;
  RHS->print(O);
  O << ')';
}

void DefinedOperatorBinaryExpr::print(llvm::raw_ostream &O) {
  O << '(';
  LHS->print(O);
  II->getName();
  RHS->print(O);
  O << ')';
}

void ImpliedDoExpr::print(llvm::raw_ostream &O) {
  O << '(';
  auto Body = getBody();
  for(size_t I = 0; I < Body.size(); ++I) {
    if(I) O << ", ";
    Body[I]->print(O);
  }
  O << ", " << getVarDecl()->getIdentifier()->getName();
  O << " = ";
  getInitialParameter()->print(O);
  O << ", ";
  getTerminalParameter()->print(O);
  if(getIncrementationParameter()) {
     O << ", ";
     getIncrementationParameter()->print(O);
  }
  O << ')';
}

//===----------------------------------------------------------------------===//
// Array Specification
//===----------------------------------------------------------------------===//

ArraySpec::ArraySpec(ArraySpecKind K)
  : Kind(K) {}

ExplicitShapeSpec::ExplicitShapeSpec(ExprResult UB)
  : ArraySpec(k_ExplicitShape), LowerBound(), UpperBound(UB) {}
ExplicitShapeSpec::ExplicitShapeSpec(ExprResult LB, ExprResult UB)
  : ArraySpec(k_ExplicitShape), LowerBound(LB), UpperBound(UB) {}

ExplicitShapeSpec *ExplicitShapeSpec::Create(ASTContext &C, ExprResult UB) {
  return new (C) ExplicitShapeSpec(UB);
}

ExplicitShapeSpec *ExplicitShapeSpec::Create(ASTContext &C,
                                             ExprResult LB, ExprResult UB) {
  return new (C) ExplicitShapeSpec(LB, UB);
}

AssumedShapeSpec::AssumedShapeSpec()
  : ArraySpec(k_AssumedShape), LowerBound() {}
AssumedShapeSpec::AssumedShapeSpec(ExprResult LB)
  : ArraySpec(k_AssumedShape), LowerBound(LB) {}

AssumedShapeSpec *AssumedShapeSpec::Create(ASTContext &C) {
  return new (C) AssumedShapeSpec();
}

AssumedShapeSpec *AssumedShapeSpec::Create(ASTContext &C, ExprResult LB) {
  return new (C) AssumedShapeSpec(LB);
}

DeferredShapeSpec::DeferredShapeSpec()
  : ArraySpec(k_DeferredShape) {}

DeferredShapeSpec *DeferredShapeSpec::Create(ASTContext &C) {
  return new (C) DeferredShapeSpec();
}

ImpliedShapeSpec::ImpliedShapeSpec()
  : ArraySpec(k_ImpliedShape), LowerBound() {}
ImpliedShapeSpec::ImpliedShapeSpec(ExprResult LB)
  : ArraySpec(k_ImpliedShape), LowerBound(LB) {}

ImpliedShapeSpec *ImpliedShapeSpec::Create(ASTContext &C) {
  return new (C) ImpliedShapeSpec();
}

ImpliedShapeSpec *ImpliedShapeSpec::Create(ASTContext &C, ExprResult LB) {
  return new (C) ImpliedShapeSpec(LB);
}

} //namespace flang
