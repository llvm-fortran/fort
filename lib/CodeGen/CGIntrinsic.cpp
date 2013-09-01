//===--- CGIntrinsic.cpp - Emit LLVM Code for Intrinsic calls ------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This contains code to emit Expr nodes with scalar LLVM types as LLVM code.
//
//===----------------------------------------------------------------------===//

#include <limits>
#include "CodeGenFunction.h"
#include "CodeGenModule.h"
#include "CGSystemRuntime.h"
#include "flang/AST/ASTContext.h"
#include "flang/AST/ExprVisitor.h"
#include "flang/Frontend/CodeGenOptions.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/CFG.h"

namespace flang {
namespace CodeGen {

RValueTy CodeGenFunction::EmitIntrinsicCall(const IntrinsicCallExpr *E) {
  using namespace intrinsic;

  auto Func = getGenericFunctionKind(E->getIntrinsicFunction());
  auto Group = getFunctionGroup(Func);
  auto Args = E->getArguments();

  switch(Group) {
  case GROUP_CONVERSION:
    if(Func == INT ||
       Func == REAL) {
      if(Args[0]->getType()->isComplexType())
        return EmitComplexToScalarConversion(EmitComplexExpr(Args[0]),
                                             E->getType());
      else
        return EmitScalarToScalarConversion(EmitScalarExpr(Args[0]),
                                            E->getType());
    } else if(Func == CMPLX) {
      if(Args[0]->getType()->isComplexType())
        return EmitComplexToComplexConversion(EmitComplexExpr(Args[0]),
                                              E->getType());
      else {
        if(Args.size() == 2) {
          auto ElementType = getContext().getComplexTypeElementType(E->getType());
          return ComplexValueTy(EmitScalarToScalarConversion(EmitScalarExpr(Args[0]), ElementType),
                                EmitScalarToScalarConversion(EmitScalarExpr(Args[1]), ElementType));
        }
        else return EmitScalarToComplexConversion(EmitScalarExpr(Args[0]),
                                                  E->getType());
      }
    } else if(Func == ICHAR) {
      auto Value = EmitCharacterDereference(EmitCharacterExpr(Args[0]));
      return Builder.CreateZExtOrTrunc(Value, ConvertType(getContext().IntegerTy));
    } else if(Func == CHAR) {
      auto Temp = CreateTempAlloca(CGM.Int8Ty, "char");
      auto Value = CharacterValueTy(Temp,
                                    llvm::ConstantInt::get(CGM.SizeTy, 1));
      Builder.CreateStore(Builder.CreateSExtOrTrunc(EmitScalarExpr(Args[0]), CGM.Int8Ty),
                          Value.Ptr);
      return Value;
    } else llvm_unreachable("invalid conversion intrinsic");
    break;

  case GROUP_TRUNCATION:
    return EmitIntrinsicCallScalarTruncation(Func, EmitScalarExpr(Args[0]),
                                             E->getType());

  case GROUP_COMPLEX:
    return EmitIntrinsicCallComplex(Func, EmitComplexExpr(Args[0]));

  case GROUP_MATHS:
    if(Func == MAX || Func == MIN)
      return EmitIntrinsicMinMax(Func, Args);
    if(Args[0]->getType()->isComplexType())
      return EmitIntrinsicCallComplexMath(Func, EmitComplexExpr(Args[0]));
    return EmitIntrinsicCallScalarMath(Func, EmitScalarExpr(Args[0]),
                                       Args.size() == 2?
                                        EmitScalarExpr(Args[1]) : nullptr);
  case GROUP_CHARACTER:
    if(Args.size() == 1)
      return EmitIntrinsicCallCharacter(Func, EmitCharacterExpr(Args[0]));
    else
      return EmitIntrinsicCallCharacter(Func, EmitCharacterExpr(Args[0]),
                                        EmitCharacterExpr(Args[1]));

  case GROUP_NUMERIC_INQUIRY:
    return EmitIntrinsicNumericInquiry(Func, Args[0], E->getType());

  case GROUP_SYSTEM:
    return EmitSystemIntrinsic(Func, Args);

  default:
    llvm_unreachable("invalid intrinsic");
    break;
  }

  return RValueTy();
}

llvm::Value *CodeGenFunction::EmitIntrinsicCallScalarTruncation(intrinsic::FunctionKind Func,
                                                                llvm::Value *Value,
                                                                QualType ResultType) {
  llvm::Value *FuncDecl = nullptr;
  auto ValueType = Value->getType();
  switch(Func) {
  case intrinsic::AINT:
    FuncDecl = GetIntrinsicFunction(llvm::Intrinsic::trunc, ValueType);
    break;
  case intrinsic::ANINT:
  case intrinsic::NINT:
    FuncDecl = GetIntrinsicFunction(llvm::Intrinsic::rint, ValueType);
    break;
  }

  auto Result = Builder.CreateCall(FuncDecl, Value);
  if(Func == intrinsic::NINT)
    return EmitScalarToScalarConversion(Result, ResultType);
  return Result;
}

#define MANGLE_MATH_FUNCTION(Str, Type) \
  ((Type)->isFloatTy() ? Str "f" : Str)

llvm::Value* CodeGenFunction::EmitIntrinsicCallScalarMath(intrinsic::FunctionKind Func,
                                                          llvm::Value *A1, llvm::Value *A2) {
  using namespace intrinsic;

  llvm::Value *FuncDecl = nullptr;
  auto ValueType = A1->getType();
  switch(Func) {
  case ABS:
    if(ValueType->isIntegerTy()) {
      auto Condition = Builder.CreateICmpSGE(A1, llvm::ConstantInt::get(ValueType, 0));
      return Builder.CreateSelect(Condition, A1, Builder.CreateNeg(A1));
    }
    FuncDecl = GetIntrinsicFunction(llvm::Intrinsic::fabs, ValueType);
    break;

  case MOD:
    if(ValueType->isIntegerTy())
      return Builder.CreateSRem(A1, A2);
    else
      return Builder.CreateFRem(A1, A2);
    break;

  // |a1|  if a2 >= 0
  // -|a1| if a2 < 0
  case SIGN: {
    auto A1Abs = EmitIntrinsicCallScalarMath(ABS, A1);
    auto Cond = EmitScalarRelationalExpr(BinaryExpr::GreaterThanEqual,
                                         A2, GetConstantZero(A2->getType()));
    return Builder.CreateSelect(Cond, A1Abs, EmitScalarUnaryMinus(A1Abs));
    break;
  }

  //a1-a2 if a1>a2
  //  0   if a1<=a2
  case DIM: {
    auto Cond = EmitScalarRelationalExpr(BinaryExpr::GreaterThan,
                                         A1, A2);
    return Builder.CreateSelect(Cond,
                                EmitScalarBinaryExpr(BinaryExpr::Minus,
                                                     A1, A2),
                                GetConstantZero(A1->getType()));
    break;
  }

  case DPROD: {
    auto TargetType = getContext().DoublePrecisionTy;
    return Builder.CreateFMul(EmitScalarToScalarConversion(A1, TargetType),
                              EmitScalarToScalarConversion(A2, TargetType));
  }

  case SQRT:
    FuncDecl = GetIntrinsicFunction(llvm::Intrinsic::sqrt, ValueType);
    break;
  case EXP:
    FuncDecl = GetIntrinsicFunction(llvm::Intrinsic::exp, ValueType);
    break;
  case LOG:
    FuncDecl = GetIntrinsicFunction(llvm::Intrinsic::log, ValueType);
    break;
  case LOG10:
    FuncDecl = GetIntrinsicFunction(llvm::Intrinsic::log10, ValueType);
    break;
  case SIN:
    FuncDecl = GetIntrinsicFunction(llvm::Intrinsic::sin, ValueType);
    break;
  case COS:
    FuncDecl = GetIntrinsicFunction(llvm::Intrinsic::cos, ValueType);
    break;
  case TAN:
    FuncDecl = CGM.GetCFunction(MANGLE_MATH_FUNCTION("tan", ValueType),
                                ValueType, ValueType);
    break;
  case ASIN:
    FuncDecl = CGM.GetCFunction(MANGLE_MATH_FUNCTION("asin", ValueType),
                                ValueType, ValueType);
    break;
  case ACOS:
    FuncDecl = CGM.GetCFunction(MANGLE_MATH_FUNCTION("acos", ValueType),
                                ValueType, ValueType);
    break;
  case ATAN:
    FuncDecl = CGM.GetCFunction(MANGLE_MATH_FUNCTION("atan", ValueType),
                                ValueType, ValueType);
    break;
  case ATAN2: {
    llvm::Type *Args[] = {ValueType, ValueType};
    FuncDecl = CGM.GetCFunction(MANGLE_MATH_FUNCTION("atan2", ValueType),
                                llvm::makeArrayRef(Args, 2),
                                ValueType);
    break;
  }
  case SINH:
    FuncDecl = CGM.GetCFunction(MANGLE_MATH_FUNCTION("sinh", ValueType),
                                ValueType, ValueType);
    break;
  case COSH:
    FuncDecl = CGM.GetCFunction(MANGLE_MATH_FUNCTION("cosh", ValueType),
                                ValueType, ValueType);
    break;
  case TANH:
    FuncDecl = CGM.GetCFunction(MANGLE_MATH_FUNCTION("tanh", ValueType),
                                ValueType, ValueType);
    break;
  default:
    llvm_unreachable("invalid scalar math intrinsic");
  }
  if(A2)
    return Builder.CreateCall2(FuncDecl, A1, A2);
  return Builder.CreateCall(FuncDecl, A1);
}

llvm::Value *CodeGenFunction::EmitIntrinsicMinMax(intrinsic::FunctionKind Func,
                                                  ArrayRef<Expr*> Arguments) {
  SmallVector<llvm::Value*, 8> Args(Arguments.size());
  for(size_t I = 0; I < Arguments.size(); ++I)
    Args[I] = EmitScalarExpr(Arguments[I]);
  return EmitIntrinsicScalarMinMax(Func, Args);
}

llvm::Value *CodeGenFunction::EmitIntrinsicScalarMinMax(intrinsic::FunctionKind Func,
                                                        ArrayRef<llvm::Value*> Args) {
  auto Value = Args[0];
  auto Op = Func == intrinsic::MAX? BinaryExpr::GreaterThanEqual :
                                    BinaryExpr::LessThanEqual;
  for(size_t I = 1; I < Args.size(); ++I)
    Value = Builder.CreateSelect(EmitScalarRelationalExpr(Op,
                                 Value, Args[I]), Value, Args[I]);
  return Value;
}

// Lets pretend ** is an intrinsic
ComplexValueTy CodeGenFunction::EmitComplexPowi(ComplexValueTy LHS, llvm::Value *RHS) {
  auto ElementType = LHS.Re->getType();
  llvm::Type *ArgTypes[] = { ElementType, ElementType, CGM.Int32Ty };
  auto Func = CGM.GetRuntimeFunction(MANGLE_MATH_FUNCTION("cpowi", ElementType),
                                     llvm::makeArrayRef(ArgTypes, 3),
                                     getTypes().GetComplexType(ElementType));
  llvm::Value *Args[] = { LHS.Re, LHS.Im, RHS };
  return ExtractComplexValue(
           EmitRuntimeCall(Func, llvm::makeArrayRef(Args, 3)));
}

ComplexValueTy CodeGenFunction::EmitComplexPow(ComplexValueTy LHS, ComplexValueTy RHS) {
  auto ElementType = LHS.Re->getType();
  llvm::Type *ElementTypeX4[] = { ElementType, ElementType, ElementType, ElementType };
  auto Func = CGM.GetRuntimeFunction(MANGLE_MATH_FUNCTION("cpow", ElementType),
                                     llvm::makeArrayRef(ElementTypeX4, 4),
                                     getTypes().GetComplexType(ElementType));
  llvm::Value *Args[] = { LHS.Re, LHS.Im, RHS.Re, RHS.Im };
  return ExtractComplexValue(
           EmitRuntimeCall(Func, llvm::makeArrayRef(Args, 4)));
}

RValueTy CodeGenFunction::EmitIntrinsicCallComplexMath(intrinsic::FunctionKind Func,
                                                       ComplexValueTy Value) {
  llvm::Value *FuncDecl = nullptr;
  auto ElementType = Value.Re->getType();
  auto ValueType = getTypes().GetComplexType(ElementType);
  llvm::Type *ElementTypes[] = { ElementType, ElementType };
  auto ArgType = llvm::makeArrayRef(ElementTypes, 2);

  switch(Func) {
  case intrinsic::ABS:
    FuncDecl = CGM.GetRuntimeFunction(MANGLE_MATH_FUNCTION("cabs", ElementType),
                                      ArgType, ElementType);
    break;
  case intrinsic::SQRT:
    FuncDecl = CGM.GetRuntimeFunction(MANGLE_MATH_FUNCTION("csqrt", ElementType),
                                      ArgType, ValueType);
    break;
  case intrinsic::EXP:
    FuncDecl = CGM.GetRuntimeFunction(MANGLE_MATH_FUNCTION("cexp", ElementType),
                                      ArgType, ValueType);
    break;
  case intrinsic::LOG:
    FuncDecl = CGM.GetRuntimeFunction(MANGLE_MATH_FUNCTION("clog", ElementType),
                                      ArgType, ValueType);
    break;
  case intrinsic::SIN:
    FuncDecl = CGM.GetRuntimeFunction(MANGLE_MATH_FUNCTION("csin", ElementType),
                                      ArgType, ValueType);
    break;
  case intrinsic::COS:
    FuncDecl = CGM.GetRuntimeFunction(MANGLE_MATH_FUNCTION("ccos", ElementType),
                                      ArgType, ValueType);
    break;
  case intrinsic::TAN:
    FuncDecl = CGM.GetRuntimeFunction(MANGLE_MATH_FUNCTION("ctan", ElementType),
                                      ArgType, ValueType);
    break;
  default:
    llvm_unreachable("invalid complex math intrinsic");
  }
  auto Result = EmitRuntimeCall2(FuncDecl, Value.Re, Value.Im);
  if(Func == intrinsic::ABS)
    return Result;
  return ExtractComplexValue(Result);
}

llvm::Value *CodeGenFunction::EmitIntrinsicNumericInquiry(intrinsic::FunctionKind Func,
                                                          const Expr *E, QualType Result) {
  using namespace intrinsic;
  using namespace std;

  auto RetT = ConvertType(Result);
  auto T = E->getType();
  auto TKind = getContext().getArithmeticTypeKind(T.getExtQualsPtrOrNull(), T);
  int IntResult;

#define HANDLE_INT(Result, func) \
    switch(TKind) {  \
    case BuiltinType::Int1: \
      Result = numeric_limits<int8_t>::func; break; \
    case BuiltinType::Int2: \
      Result = numeric_limits<int16_t>::func; break; \
    case BuiltinType::Int4: \
      Result = numeric_limits<int32_t>::func; break; \
    case BuiltinType::Int8: \
      Result = numeric_limits<int64_t>::func; break; \
    default: \
      llvm_unreachable("invalid type kind"); \
      break; \
    }

#define HANDLE_REAL(Result, func) \
    switch(TKind) {  \
    case BuiltinType::Real4: \
      Result = numeric_limits<float>::func; break; \
    case BuiltinType::Real8: \
      Result = numeric_limits<double>::func; break; \
    default: \
      llvm_unreachable("invalid type kind"); \
      break; \
    }

  // FIXME: the float numeric limit is being implicitly converted into a double here..
#define HANDLE_REAL_RET_REAL(func) \
    switch(TKind) {  \
    case BuiltinType::Real4: \
      return llvm::ConstantFP::get(RetT, numeric_limits<float>::func()); \
      break; \
    case BuiltinType::Real8: \
      return llvm::ConstantFP::get(RetT, numeric_limits<double>::func()); \
      break; \
    default: \
      llvm_unreachable("invalid type kind"); \
      break; \
    }

  if(T->isIntegerType()) {
    switch(Func) {
    case RADIX:
      HANDLE_INT(IntResult, radix);
      break;
    case DIGITS:
      HANDLE_INT(IntResult, digits);
      break;
    case RANGE:
      HANDLE_INT(IntResult, digits10);
      break;
    case HUGE: {
      int64_t i64;
      HANDLE_INT(i64, max());
      return llvm::ConstantInt::get(RetT, i64, true);
      break;
    }
    case TINY: {
      int64_t i64;
      HANDLE_INT(i64, min());
      return llvm::ConstantInt::get(RetT, i64, true);
      break;
    }
    default:
      llvm_unreachable("Invalid integer inquiry intrinsic");
    }
  } else {
    switch(Func) {
    case RADIX:
      HANDLE_REAL(IntResult, radix);
      break;
    case DIGITS:
      HANDLE_REAL(IntResult, digits);
      break;
    case MINEXPONENT:
      HANDLE_REAL(IntResult, min_exponent);
      break;
    case MAXEXPONENT:
      HANDLE_REAL(IntResult, max_exponent);
      break;
    case PRECISION:
      HANDLE_REAL(IntResult, digits10);
      break;
    case RANGE:
      HANDLE_REAL(IntResult, min_exponent10);
      IntResult = abs(IntResult);
      break;
    case HUGE:
      HANDLE_REAL_RET_REAL(max);
      break;
    case TINY:
      HANDLE_REAL_RET_REAL(min);
      break;
    case EPSILON:
      HANDLE_REAL_RET_REAL(epsilon);
      break;
    }
  }

#undef HANDLE_INT
#undef HANDLE_REAL

  return llvm::ConstantInt::get(RetT, IntResult, true);
}

RValueTy CodeGenFunction::EmitSystemIntrinsic(intrinsic::FunctionKind Func,
                                              ArrayRef<Expr*> Arguments) {
  using namespace intrinsic;

  switch(Func) {
  case ETIME:
    return CGM.getSystemRuntime().EmitETIME(*this, Arguments);

  default:
    llvm_unreachable("invalid intrinsic");
    break;
  }

  return RValueTy();
}

}
} // end namespace flang
