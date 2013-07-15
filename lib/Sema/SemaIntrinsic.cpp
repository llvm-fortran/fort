//===--- SemaIntrinsic.cpp - Intrinsic call Semantic Checking -------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "flang/Sema/Sema.h"
#include "flang/Sema/DeclSpec.h"
#include "flang/Sema/SemaDiagnostic.h"
#include "flang/AST/ASTContext.h"
#include "flang/AST/Decl.h"
#include "flang/AST/Expr.h"
#include "flang/Basic/Diagnostic.h"
#include "llvm/Support/raw_ostream.h"

namespace flang {

using namespace intrinsic;

bool Sema::CheckIntrinsicCallArgumentCount(intrinsic::FunctionKind Function,
                                           ArrayRef<Expr*> Args,
                                           SourceLocation Loc) {
  unsigned ArgCountDiag = 0;
  int ExpectedCount = 0;
  const char *ExpectedString = nullptr;

  switch(getFunctionArgumentCount(Function)) {
  case ArgumentCount1:
    ExpectedCount = 1;
    if(Args.size() < 1)
      ArgCountDiag = diag::err_typecheck_call_too_few_args;
    else if(Args.size() > 1)
      ArgCountDiag = diag::err_typecheck_call_too_many_args;
    break;
  case ArgumentCount2:
    ExpectedCount = 2;
    if(Args.size() < 2)
      ArgCountDiag = diag::err_typecheck_call_too_few_args;
    else if(Args.size() > 2)
      ArgCountDiag = diag::err_typecheck_call_too_many_args;
    break;
  case ArgumentCount1or2:
    ExpectedString = "1 or 2";
    if(Args.size() < 1)
      ArgCountDiag = diag::err_typecheck_call_too_few_args;
    else if(Args.size() > 2)
      ArgCountDiag = diag::err_typecheck_call_too_many_args;
    break;
  case ArgumentCount2orMore:
    ExpectedCount = 2;
    if(Args.size() < 2)
      ArgCountDiag = diag::err_typecheck_call_too_few_args_at_least;
    break;
  default:
    llvm_unreachable("invalid arg count");
  }
  if(ArgCountDiag) {
    auto Reporter = Diags.Report(Loc, ArgCountDiag)
                      << /*intrinsic function=*/ 0;
    if(ExpectedString)
      Reporter << ExpectedString;
    else
      Reporter << ExpectedCount;
    Reporter << unsigned(Args.size());
    return true;
  }
  return false;
}

// FIXME: add support for kind parameter
bool Sema::CheckIntrinsicConversionFunc(intrinsic::FunctionKind Function,
                                        ArrayRef<Expr*> Args,
                                        QualType &ReturnType) {
  auto FirstArg = Args[0];
  switch(Function) {
  case INT: case IFIX: case IDINT:
    if(Function == IFIX) CheckStrictlyRealArgument(FirstArg);
    else if(Function == IDINT) CheckDoublePrecisionRealArgument(FirstArg);
    else CheckIntegerOrRealOrComplexArgument(FirstArg);
    ReturnType = Context.IntegerTy;
    break;

  case REAL: case FLOAT: case SNGL:
    if(Function == FLOAT) CheckIntegerArgument(FirstArg);
    else if(Function == SNGL) CheckDoublePrecisionRealArgument(FirstArg);
    else CheckIntegerOrRealOrComplexArgument(FirstArg);
    ReturnType = Context.RealTy;
    break;

  case DBLE:
    CheckIntegerOrRealOrComplexArgument(FirstArg);
    ReturnType = Context.DoublePrecisionTy;
    break;

  case CMPLX:
  case DCMPLX:
    CheckIntegerOrRealOrComplexArgument(FirstArg);
    if(Args.size() > 1) {
      if(!CheckIntegerOrRealOrComplexArgument(Args[1]))
        CheckExpressionListSameTypeKind(Args);
    }
    ReturnType = Function == CMPLX? Context.ComplexTy :
                                    Context.DoubleComplexTy;
    break;

  case ICHAR:
    CheckCharacterArgument(FirstArg);
    ReturnType = Context.IntegerTy;
    break;

  case CHAR:
    CheckIntegerArgument(FirstArg);
    ReturnType = Context.CharacterTy;
    break;
  }
  return false;
}

bool Sema::CheckIntrinsicTruncationFunc(intrinsic::FunctionKind Function,
                                        ArrayRef<Expr*> Args,
                                        QualType &ReturnType) {
  auto Arg = Args[0];
  auto GenericFunction = getGenericFunctionKind(Function);

  if(GenericFunction != Function)
    CheckDoublePrecisionRealArgument(Arg);
  else CheckRealArgument(Arg);

  switch(GenericFunction) {
  case AINT:
  case ANINT:
    ReturnType = Arg->getType();
    break;
  case NINT:
    ReturnType = Context.IntegerTy;
    break;
  }
  return false;
}

bool Sema::CheckIntrinsicComplexFunc(intrinsic::FunctionKind Function,
                                     ArrayRef<Expr*> Args,
                                     QualType &ReturnType) {
  auto Arg = Args[0];
  auto GenericFunction = getGenericFunctionKind(Function);

  if(GenericFunction != Function) {
    if(CheckDoubleComplexArgument(Arg)) return true;
  } else {
    if(CheckComplexArgument(Arg)) return true;
  }

  switch(GenericFunction) {
  case AIMAG:
    ReturnType = Context.getComplexTypeElementType(Arg->getType());
    break;
  case CONJG:
    ReturnType = Arg->getType();
    ReturnType.dump();
    break;
  }
  return false;
}

static QualType TypeWithKind(ASTContext &C, QualType T, QualType TKind) {
  const ExtQuals *AExt = T.getExtQualsPtrOrNull();
  const ExtQuals *BExt = TKind.getExtQualsPtrOrNull();
  auto AK = C.getArithmeticTypeKind(AExt, T);
  auto BK = C.getArithmeticTypeKind(BExt, TKind);
  if(AK == BK) return T;
  return C.getQualTypeOtherKind(T, TKind);
}

bool Sema::CheckIntrinsicMathsFunc(intrinsic::FunctionKind Function,
                                   ArrayRef<Expr*> Args,
                                   QualType &ReturnType) {
  auto FirstArg = Args[0];
  auto SecondArg = Args.size() > 1? Args[1] : nullptr;
  auto GenericFunction = getGenericFunctionKind(Function);

  switch(GenericFunction) {
  case ABS:
    if(GenericFunction != Function) {
      switch(Function) {
      case IABS: CheckIntegerArgument(FirstArg); break;
      case DABS: CheckDoublePrecisionRealArgument(FirstArg); break;
      case CABS: CheckComplexArgument(FirstArg); break;
      case CDABS:
        CheckDoubleComplexArgument(FirstArg);
        ReturnType = Context.DoublePrecisionTy;
        return false;
      }
    }
    else CheckIntegerOrRealOrComplexArgument(FirstArg);
    if(FirstArg->getType()->isComplexType()) {
      ReturnType = TypeWithKind(Context, Context.RealTy,
                                FirstArg->getType());
    } else
      ReturnType = FirstArg->getType();
    break;

  // 2 integer/real/complex
  case MOD:
  case ATAN2:
    if(GenericFunction != Function) {
      switch(Function) {
      case AMOD:
        CheckRealArgument(FirstArg);
        CheckRealArgument(SecondArg);
        break;
      case DMOD:
      case DATAN2:
        CheckDoublePrecisionRealArgument(FirstArg);
        CheckDoublePrecisionRealArgument(SecondArg);
        break;
      }
    }
    else {
      if(GenericFunction == ATAN2) {
        CheckRealArgument(FirstArg);
        CheckRealArgument(SecondArg);
      } else {
        CheckIntegerOrRealArgument(FirstArg);
        CheckIntegerOrRealArgument(SecondArg);
      }
    }
    CheckExpressionListSameTypeKind(Args);
    ReturnType = FirstArg->getType();
    break;

  // 1 real/double/complex
  case SQRT:
  case EXP:
  case LOG:
  case SIN:
  case COS:
  case TAN:
    if(GenericFunction != Function) {
      switch(Function) {
      case ALOG:
        if(CheckStrictlyRealArgument(FirstArg))
          return true;
        break;
      case DSQRT: case DEXP: case DLOG:
      case DSIN: case DCOS: case DTAN:
        if(CheckDoublePrecisionRealArgument(FirstArg))
          return true;
        break;
      case CSQRT: case CEXP: case CLOG:
      case CSIN:  case CCOS: case CTAN:
        if(CheckComplexArgument(FirstArg))
          return true;
        break;
      }
    }
    else if(CheckRealOrComplexArgument(FirstArg))
      return true;
    ReturnType = FirstArg->getType();
    break;

  // 1 real/double
  case LOG10:
  case ASIN:
  case ACOS:
  case ATAN:
  case SINH:
  case COSH:
  case TANH:
    if(GenericFunction != Function) {
      switch(Function) {
      case ALOG10:
        if(CheckStrictlyRealArgument(FirstArg))
          return true;
        break;
      default:
        if(CheckDoublePrecisionRealArgument(FirstArg))
          return true;
      }
    }
    else if(CheckRealArgument(FirstArg))
      return true;
    ReturnType = FirstArg->getType();
    break;

  }
  return false;
}

bool Sema::CheckIntrinsicCharacterFunc(intrinsic::FunctionKind Function,
                                       ArrayRef<Expr*> Args,
                                       QualType &ReturnType) {
  auto FirstArg = Args[0];
  auto SecondArg = Args.size() > 1? Args[1] : nullptr;

  CheckCharacterArgument(FirstArg);
  if(SecondArg) {
    if(!CheckCharacterArgument(SecondArg))
      CheckExpressionListSameTypeKind(Args);
  }

  switch(Function) {
  case LEN:
  case LEN_TRIM:
  case INDEX:
    ReturnType = Context.IntegerTy;
    break;

  case LGE: case LGT: case LLE: case LLT:
    ReturnType = Context.LogicalTy;
    break;
  }
  return false;
}

} // end namespace flang
