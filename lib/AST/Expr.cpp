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

SMLoc ConstantExpr::getMaxLocation() const {
  return MaxLoc;
}

IntegerConstantExpr::IntegerConstantExpr(ASTContext &C, llvm::SMLoc Loc,
                                         llvm::SMLoc MaxLoc, llvm::StringRef Data)
  : ConstantExpr(IntegerConstant, C.IntegerTy,  Loc, MaxLoc) {
  llvm::APInt Val(64,Data,10);
  Num.setValue(C, Val);
}

IntegerConstantExpr *IntegerConstantExpr::Create(ASTContext &C, llvm::SMLoc Loc,
                                                 llvm::SMLoc MaxLoc, llvm::StringRef Data) {
  return new (C) IntegerConstantExpr(C, Loc, MaxLoc, Data);
}

RealConstantExpr::RealConstantExpr(ASTContext &C, llvm::SMLoc Loc,
                                   llvm::SMLoc MaxLoc, llvm::StringRef Data)
  : ConstantExpr(RealConstant, C.RealTy, Loc, MaxLoc) {
  // FIXME: IEEEdouble?
  APFloat Val(APFloat::IEEEsingle, Data);
  Num.setValue(C, Val);
}

RealConstantExpr *RealConstantExpr::Create(ASTContext &C, llvm::SMLoc Loc,
                                           llvm::SMLoc MaxLoc, llvm::StringRef Data) {
  return new (C) RealConstantExpr(C, Loc, MaxLoc, Data);
}

DoublePrecisionConstantExpr::DoublePrecisionConstantExpr(ASTContext &C, llvm::SMLoc Loc,
                                                         llvm::SMLoc MaxLoc, llvm::StringRef Data)
  : ConstantExpr(RealConstant, C.DoublePrecisionTy, Loc, MaxLoc) {
  APFloat Val(APFloat::IEEEdouble, Data);
  Num.setValue(C, Val);
}

DoublePrecisionConstantExpr *DoublePrecisionConstantExpr::Create(ASTContext &C, llvm::SMLoc Loc,
                                                                 llvm::SMLoc MaxLoc,
                                                                 llvm::StringRef Data) {
  return new (C) DoublePrecisionConstantExpr(C, Loc, MaxLoc, Data);
}

ComplexConstantExpr::ComplexConstantExpr(ASTContext &C, llvm::SMLoc Loc, llvm::SMLoc MaxLoc,
                                         const APFloat &Re, const APFloat &Im)
  : ConstantExpr(ComplexConstant, C.ComplexTy, Loc, MaxLoc) {
  this->Re.setValue(C, Re);
  this->Im.setValue(C, Im);
}

ComplexConstantExpr *ComplexConstantExpr::Create(ASTContext &C, llvm::SMLoc Loc,
                                                 llvm::SMLoc MaxLoc,
                                                 const APFloat &Re, const APFloat &Im) {
  return new (C) ComplexConstantExpr(C, Loc, MaxLoc, Re, Im);
}

CharacterConstantExpr::CharacterConstantExpr(ASTContext &C, llvm::SMLoc Loc,
                                             llvm::SMLoc MaxLoc, llvm::StringRef data)
  : ConstantExpr(CharacterConstant, C.CharacterTy, Loc, MaxLoc) {
  // TODO: A 'kind' on a character literal constant.
  Data = new (C) char[data.size() + 1];
  std::strncpy(Data, data.data(), data.size());
  Data[data.size()] = '\0';
}

CharacterConstantExpr *CharacterConstantExpr::Create(ASTContext &C, llvm::SMLoc Loc,
                                                     llvm::SMLoc MaxLoc,
                                                     llvm::StringRef Data) {
  return new (C) CharacterConstantExpr(C, Loc, MaxLoc, Data);
}

BOZConstantExpr::BOZConstantExpr(ASTContext &C, llvm::SMLoc Loc,
                                 llvm::SMLoc MaxLoc, llvm::StringRef Data)
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

BOZConstantExpr *BOZConstantExpr::Create(ASTContext &C, llvm::SMLoc Loc,
                                         llvm::SMLoc MaxLoc, llvm::StringRef Data) {
  return new (C) BOZConstantExpr(C, Loc, MaxLoc, Data);
}

LogicalConstantExpr::LogicalConstantExpr(ASTContext &C, llvm::SMLoc Loc,
                                         llvm::SMLoc MaxLoc, llvm::StringRef Data)
  : ConstantExpr(LogicalConstant, C.LogicalTy, Loc, MaxLoc) {
  Val = (Data.compare_lower(".TRUE.") == 0);
}

LogicalConstantExpr *LogicalConstantExpr::Create(ASTContext &C, llvm::SMLoc Loc,
                                                 llvm::SMLoc MaxLoc, llvm::StringRef Data) {
  return new (C) LogicalConstantExpr(C, Loc, MaxLoc, Data);
}

SubstringExpr::SubstringExpr(ASTContext &C, llvm::SMLoc Loc, ExprResult E,
                             ExprResult Start, ExprResult End)
  : DesignatorExpr(Loc,C.CharacterTy,DesignatorExpr::Substring),
    Target(E), StartingPoint(Start), EndPoint(End) {
}

SubstringExpr *SubstringExpr::Create(ASTContext &C, llvm::SMLoc Loc,
                                     ExprResult Target, ExprResult StartingPoint,
                                     ExprResult EndPoint) {
  return new(C) SubstringExpr(C, Loc, Target, StartingPoint, EndPoint);
}

SMLoc SubstringExpr::getMinLocation() const {
  return Target.get()->getMinLocation();
}

SMLoc SubstringExpr::getMaxLocation() const {
  if(EndPoint.isUsable()) return EndPoint.get()->getMaxLocation();
  else if(StartingPoint.isUsable()) return StartingPoint.get()->getMaxLocation();
  else return getLocation();
}

ArrayElementExpr::ArrayElementExpr(ASTContext &C, llvm::SMLoc Loc, ExprResult E,
                                   llvm::ArrayRef<ExprResult> Subs)
  : DesignatorExpr(Loc, E.get()->getType()->asArrayType()->getElementType(),
                   DesignatorExpr::ArrayElement),
    Target(E) {
  NumSubscripts = Subs.size();
  SubscriptList = new (C) ExprResult [NumSubscripts];

  for (unsigned I = 0; I != NumSubscripts; ++I)
    SubscriptList[I] = Subs[I];
}

ArrayElementExpr *ArrayElementExpr::Create(ASTContext &C, llvm::SMLoc Loc,
                                           ExprResult Target,
                                           llvm::ArrayRef<ExprResult> Subscripts) {
  return new(C) ArrayElementExpr(C, Loc, Target, Subscripts);
}

SMLoc ArrayElementExpr::getMinLocation() const {
  return Target.get()->getMinLocation();
}

SMLoc ArrayElementExpr::getMaxLocation() const {
  return SubscriptList[NumSubscripts - 1].get()->getMaxLocation();
}

VarExpr::VarExpr(llvm::SMLoc Loc, const VarDecl *Var)
  : DesignatorExpr(Loc, Var->getType(), DesignatorExpr::ObjectName),
    Variable(Var) {}

VarExpr *VarExpr::Create(ASTContext &C, llvm::SMLoc Loc, const VarDecl *VD) {
  return new (C) VarExpr(Loc, VD);
}

SMLoc VarExpr::getMaxLocation() const {
  return SMLoc::getFromPointer(getLocation().getPointer() +
                               Variable->getIdentifier()->getLength());
}

UnaryExpr *UnaryExpr::Create(ASTContext &C, llvm::SMLoc loc, Operator op,
                             ExprResult e) {
  return new (C) UnaryExpr(Expr::Unary,
                           (op != Not) ? e.get()->getType() : C.LogicalTy,
                           loc, op, e);
}

SMLoc UnaryExpr::getMaxLocation() const {
  return E.get()->getMaxLocation();
}

DefinedOperatorUnaryExpr::DefinedOperatorUnaryExpr(llvm::SMLoc loc, ExprResult e,
                                                   IdentifierInfo *ii)
  : UnaryExpr(Expr::DefinedUnaryOperator, e.get()->getType(), loc, Defined, e),
    II(ii) {}

DefinedOperatorUnaryExpr *DefinedOperatorUnaryExpr::Create(ASTContext &C,
                                                           llvm::SMLoc loc,
                                                           ExprResult e,
                                                           IdentifierInfo *ii) {
  return new (C) DefinedOperatorUnaryExpr(loc, e, ii);
}

BinaryExpr *BinaryExpr::Create(ASTContext &C, llvm::SMLoc loc, Operator op,
                               ExprResult lhs, ExprResult rhs) {
  QualType Ty;

  switch (op) {
  default: {
    // FIXME: Combine two types.
    Ty = lhs.get()->getType();
    break;
  }
  case Eqv: case Neqv: case Or: case And:
  case Equal: case NotEqual: case LessThan: case LessThanEqual:
  case GreaterThan: case GreaterThanEqual:
    Ty = C.LogicalTy;
    break;
  case Concat:
    Ty = C.CharacterTy;
    break;
  }

  return new (C) BinaryExpr(Expr::Binary, Ty, loc, op, lhs, rhs);
}

SMLoc BinaryExpr::getMinLocation() const {
  return LHS.get()->getMinLocation();
}

SMLoc BinaryExpr::getMaxLocation() const {
  return RHS.get()->getMaxLocation();
}

DefinedOperatorBinaryExpr *
DefinedOperatorBinaryExpr::Create(ASTContext &C, llvm::SMLoc loc, ExprResult lhs,
                                  ExprResult rhs, IdentifierInfo *ii) {
  return new (C) DefinedOperatorBinaryExpr(loc, lhs, rhs, ii);
}

static inline QualType ConversionType(ASTContext &C, IntrinsicCallExpr::IntrinsicFunction Op) {
  switch(Op) {
    case IntrinsicCallExpr::INT: return C.IntegerTy;
    case IntrinsicCallExpr::REAL: return C.RealTy;
    case IntrinsicCallExpr::DBLE: return C.DoublePrecisionTy;
    case IntrinsicCallExpr::CMPLX: return C.ComplexTy;
  }
  llvm_unreachable("Unknown conversion type!");
}

IntrinsicCallExpr::IntrinsicCallExpr(ASTContext &C, llvm::SMLoc L,
                               IntrinsicFunction op, ExprResult e)
  : Expr(Conversion,ConversionType(C, op),L),Op(op),E(e) {
}

IntrinsicCallExpr *IntrinsicCallExpr::Create(ASTContext &C, llvm::SMLoc L,
                                       IntrinsicFunction Op, ExprResult E) {
  return new(C) IntrinsicCallExpr(C, L, Op, E);
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
  Target.get()->print(O);
  O << '(';
  if(StartingPoint.get()) StartingPoint.get()->print(O);
  O << ':';
  if(EndPoint.get()) EndPoint.get()->print(O);
  O << ')';
}

void ArrayElementExpr::print(llvm::raw_ostream &O) {
  Target.get()->print(O);
  O << '(';
  llvm::ArrayRef<ExprResult> Subscripts = getSubscriptList();
  for(size_t I = 0; I < Subscripts.size(); ++I) {
    if(I) O << ", ";
    Subscripts[I].get()->print(O);
  }
  O << ')';
}

void UnaryExpr::print(llvm::raw_ostream &O) {
  O << '(';
  const char *op = 0;
  switch (Op) {
  default: break;
  case Not:   op = ".NOT."; break;
  case Plus:  op = "+";     break;
  case Minus: op = "-";     break;
  }
  O << op;
  E.get()->print(O);
  O << ')';
}

void DefinedOperatorUnaryExpr::print(llvm::raw_ostream &O) {
  O << '(' << II->getName();
  E.get()->print(O);
  O << ')';
}

void IntrinsicCallExpr::print(llvm::raw_ostream &O) {
  switch(Op) {
#define HANDLE(WHAT) case WHAT: O << #WHAT; break
  HANDLE(INT);
  HANDLE(REAL);
  HANDLE(DBLE);
  HANDLE(CMPLX);
  HANDLE(ICHAR);
  HANDLE(CHAR);

  HANDLE(AINT);
  HANDLE(ANINT);
  HANDLE(NINT);
  HANDLE(ABS);
  HANDLE(MOD);
  HANDLE(SIGN);
  HANDLE(DIM);
  HANDLE(DPROD);
  HANDLE(MAX);
  HANDLE(MIN);
  HANDLE(LEN);
  HANDLE(INDEX);
  HANDLE(AIMAG);
  HANDLE(CONJG);

  HANDLE(SQRT);
  HANDLE(EXP);
  HANDLE(LOG);
  HANDLE(LOG10);
  HANDLE(SIN);
  HANDLE(COS);
  HANDLE(TAN);
  HANDLE(ASIN);
  HANDLE(ACOS);
  HANDLE(ATAN);
  HANDLE(ATAN2);
  HANDLE(SINH);
  HANDLE(COSH);
  HANDLE(TANH);

  HANDLE(LGE);
  HANDLE(LGT);
  HANDLE(LLE);
  HANDLE(LLT);
#undef HANDLE
  }

  O << '(';
  E.get()->print(O);
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

void DoublePrecisionConstantExpr::print(llvm::raw_ostream &O) {
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

void VarExpr::print(llvm::raw_ostream &O) {
  O << *Variable;
}

void BinaryExpr::print(llvm::raw_ostream &O) {
  O << '(';
  LHS.get()->print(O);
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
  RHS.get()->print(O);
  O << ')';
}

void DefinedOperatorBinaryExpr::print(llvm::raw_ostream &O) {
  O << '(';
  LHS.get()->print(O);
  II->getName();
  RHS.get()->print(O);
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
