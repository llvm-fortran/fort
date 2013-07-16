//===--- CGArray.cpp - Emit LLVM Code for Array operations and Expr -----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This contains code to emit Array subscript expressions and operations.
//
//===----------------------------------------------------------------------===//

#include "CodeGenFunction.h"
#include "CodeGenModule.h"
#include "flang/AST/ASTContext.h"
#include "flang/AST/ExprVisitor.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"

namespace flang {
namespace CodeGen {

llvm::Type *CodeGenTypes::GetFixedSizeArrayType(const ArrayType *T,
                                                uint64_t Size) {
  return llvm::ArrayType::get(ConvertTypeForMem(T->getElementType()),
                              Size);
}

llvm::Value *CodeGenFunction::CreateArrayAlloca(QualType T,
                                                const llvm::Twine &Name,
                                                bool IsTemp) {
  auto ATy = cast<ArrayType>(T.getTypePtr());
  uint64_t ArraySize;
  if(ATy->EvaluateSize(ArraySize, getContext())) {
    auto Ty = getTypes().GetFixedSizeArrayType(ATy, ArraySize);
    if(IsTemp)
      return CreateTempAlloca(Ty, Name);
    else
      return Builder.CreateAlloca(Ty, nullptr, Name);
  }
  // FIXME variable size stack/heap allocation
  return nullptr;
}

void CodeGenFunction::GetArrayDimensionsInfo(QualType T, SmallVectorImpl<ArrayDimensionValueTy> &Dims) {
  auto ATy = cast<ArrayType>(T.getTypePtr());
  auto Dimensions = ATy->getDimensions();

  for(size_t I = 0; I < Dimensions.size(); ++I) {
    llvm::Value *LB = nullptr;
    llvm::Value *UB = nullptr;
    auto LowerBound = Dimensions[I]->getLowerBoundOrNull();
    auto UpperBound = Dimensions[I]->getUpperBoundOrNull();
    if(LowerBound) {
      int64_t ConstantLowerBound;
      if(LowerBound->EvaluateAsInt(ConstantLowerBound, getContext())) {
        LB = llvm::ConstantInt::get(ConvertType(getContext().IntegerTy),
                                                ConstantLowerBound);
      } else LB = EmitScalarExpr(LowerBound);
    }
    // Don't need the upper bound for the last dim
    if(UpperBound && (I+1) != Dimensions.size()) {
      int64_t ConstantUpperBound;
      if(UpperBound->EvaluateAsInt(ConstantUpperBound, getContext())) {
        UB = llvm::ConstantInt::get(ConvertType(getContext().IntegerTy),
                                                ConstantUpperBound);
      } else UB = EmitScalarExpr(UpperBound);
    }
    Dims.push_back(ArrayDimensionValueTy(LB, UB));
  }
}

llvm::Value *CodeGenFunction::EmitDimSize(const ArrayDimensionValueTy &Dim) {
  // UB - LB + 1
  if(Dim.hasLowerBound()) {
    return Builder.CreateAdd(Builder.CreateSub(Dim.UpperBound,
                                               Dim.LowerBound),
                             llvm::ConstantInt::get(Dim.LowerBound->getType(),
                                                    1));
  }
  // UB - LB + 1 => UB - 1 + 1 => UB
  return Dim.UpperBound;
}

llvm::Value *CodeGenFunction::EmitDimSubscript(llvm::Value *Subscript,
                                               const ArrayDimensionValueTy &Dim) {
  // S - LB
  auto LB = Dim.hasLowerBound()? Dim.LowerBound :
                                 llvm::ConstantInt::get(Subscript->getType(), 1);
  return Builder.CreateSub(Subscript, LB);
}

llvm::Value *CodeGenFunction::EmitNthDimSubscript(llvm::Value *Subscript,
                                                  const ArrayDimensionValueTy &Dim,
                                                  llvm::Value *DimSizeProduct) {
  // (Sn - LBn) * product of sizes of previous dimensions.
  return Builder.CreateMul(EmitDimSubscript(Subscript, Dim),
                           DimSizeProduct);
}

class ArrayValueExprEmitter
  : public ConstExprVisitor<ArrayValueExprEmitter, void> {
  CodeGenFunction &CGF;
  CGBuilderTy &Builder;
  llvm::LLVMContext &VMContext;

  SmallVector<ArrayDimensionValueTy, 8> Dims;
  llvm::Value *Ptr;
public:

  ArrayValueExprEmitter(CodeGenFunction &cgf);

  void EmitExpr(const Expr *E);
  void VisitVarExpr(const VarExpr *E);

  ArrayRef<ArrayDimensionValueTy> getResultInfo() const {
    return Dims;
  }
  llvm::Value *getResultPtr() const {
    return Ptr;
  }
};

ArrayValueExprEmitter::ArrayValueExprEmitter(CodeGenFunction &cgf)
  : CGF(cgf), Builder(cgf.getBuilder()),
    VMContext(cgf.getLLVMContext()) {
}

void ArrayValueExprEmitter::EmitExpr(const Expr *E) {
  Visit(E);
}

void ArrayValueExprEmitter::VisitVarExpr(const VarExpr *E) {
  auto VD = E->getVarDecl();
  if(VD->isParameter()) {
    return; //FIXME?
  }
  if(VD->isArgument()) {
    return; //FIXME: TODO.
  }
  CGF.GetArrayDimensionsInfo(VD->getType(), Dims);
  Ptr = Builder.CreateConstInBoundsGEP2_32(CGF.GetVarPtr(VD), 0, 0);
}

llvm::Value *CodeGenFunction::EmitArrayElementPtr(const Expr *Target,
                                                  const ArrayRef<Expr*> Subscripts) {
  ArrayValueExprEmitter EV(*this);
  EV.EmitExpr(Target);
  auto ResultDims = EV.getResultInfo();

  llvm::Value *Offset = EmitDimSubscript(EmitScalarExpr(Subscripts[0]), ResultDims[0]);
  if(Subscripts.size() > 1) {
    llvm::Value *SizeProduct = EmitDimSize(ResultDims[0]);
    for(size_t I = 1; I < Subscripts.size(); ++I) {
      auto Sub = EmitNthDimSubscript(EmitScalarExpr(Subscripts[I]),
                                     ResultDims[I], SizeProduct);
      Offset = Builder.CreateAdd(Offset, Sub);
      if((I + 1) != Subscripts.size())
        SizeProduct = Builder.CreateMul(SizeProduct, EmitDimSize(ResultDims[I]));
    }
  }
  return Builder.CreateGEP(EV.getResultPtr(), Offset);
}

RValueTy CodeGenFunction::EmitArrayElementExpr(const ArrayElementExpr *E) {
  auto Ptr = EmitArrayElementPtr(E->getTarget(), E->getSubscriptList());

  auto ReturnType = E->getType();
  if(ReturnType->isIntegerType() ||
     ReturnType->isRealType() ||
     ReturnType->isLogicalType())
    return Builder.CreateLoad(Ptr);
  else if(ReturnType->isComplexType())
    return EmitComplexLoad(Ptr);
  else if(ReturnType->isCharacterType())
    return GetCharacterValueFromPtr(Ptr, ReturnType);

  return RValueTy();
}

}
} // end namespace flang
