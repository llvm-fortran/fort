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
#include "CGArray.h"
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

llvm::Type *CodeGenTypes::ConvertArrayType(const ArrayType *T) {
  return llvm::PointerType::get(ConvertTypeForMem(T->getElementType()), 0);
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
    if(UpperBound) {
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

ArraySection CodeGenFunction::EmitDimSection(const ArrayDimensionValueTy &Dim) {
  auto Offset = Dim.hasOffset()? Dim.Offset : nullptr;
  auto Size = EmitDimSize(Dim);
  return ArraySection(ArrayRangeSection(Offset, Size,
                        Dim.hasStride()? Dim.Stride : nullptr),
                      Size);
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
  if(CGF.IsInlinedArgument(VD))
    return EmitExpr(CGF.GetInlinedArgumentValue(VD));
  if(VD->isParameter()) {
    return; //FIXME?
  }
  if(VD->isArgument()) {
    CGF.GetArrayDimensionsInfo(VD->getType(), Dims);
    Ptr = CGF.GetVarPtr(VD);
    return;
  }
  CGF.GetArrayDimensionsInfo(VD->getType(), Dims);
  Ptr = Builder.CreateConstInBoundsGEP2_32(CGF.GetVarPtr(VD), 0, 0);
}

class ArraySectionsEmmitter
  : public ConstExprVisitor<ArraySectionsEmmitter, void> {
  CodeGenFunction &CGF;
  SmallVector<ArraySection, 8> Sections;
  llvm::Value *Ptr;
public:
  ArraySectionsEmmitter(CodeGenFunction &cgf);

  void EmitExpr(const Expr *E);
  void VisitExpr(const Expr *E);

  ArrayRef<ArraySection> getSections() const {
    return Sections;
  }
  llvm::Value *getPointer() const {
    return Ptr;
  }
};

ArraySectionsEmmitter::ArraySectionsEmmitter(CodeGenFunction &cgf)
  : CGF(cgf) {}

void ArraySectionsEmmitter::EmitExpr(const Expr *E) {
  Visit(E);
}

void ArraySectionsEmmitter::VisitExpr(const Expr *E) {
  ArrayValueExprEmitter EV(CGF);
  EV.EmitExpr(E);
  Ptr = EV.getResultPtr();
  for(auto I : EV.getResultInfo())
    Sections.push_back(CGF.EmitDimSection(I));
}

//
// Scalar values and array sections emmitter for an array operations.
//

ArrayValueTy ArrayOperation::getArrayValue(const Expr *E) {
  auto Arr = Arrays[E];
  return ArrayValueTy(llvm::makeArrayRef(Sections.begin() + Arr.SectionsOffset,
                        E->getType()->asArrayType()->getDimensionCount()),
                      Arr.Ptr);
}

void ArrayOperation::EmitArraySections(CodeGenFunction &CGF, const Expr *E) {
  if(Arrays.find(E) != Arrays.end())
    return;

  ArraySectionsEmmitter EV(CGF);
  EV.EmitExpr(E);

  StoredArrayValue ArrayValue;
  ArrayValue.SectionsOffset = Sections.size();
  ArrayValue.Ptr = EV.getPointer();
  Arrays[E] = ArrayValue;

  for(auto S : EV.getSections())
    Sections.push_back(S);
}

RValueTy ArrayOperation::getScalarValue(const Expr *E) {
  return Scalars[E];
}

void ArrayOperation::EmitScalarValue(CodeGenFunction &CGF, const Expr *E) {
  if(Scalars.find(E) != Scalars.end())
    return;

  Scalars[E] = CGF.EmitRValue(E);
}

class ScalarEmmitterAndSectionGatherer : public ConstExprVisitor<ScalarEmmitterAndSectionGatherer> {
  CodeGenFunction &CGF;
  ArrayOperation &ArrayOp;
  const Expr *LastArrayEmmitted;
public:

  ScalarEmmitterAndSectionGatherer(CodeGenFunction &cgf, ArrayOperation &ArrOp)
    : CGF(cgf), ArrayOp(ArrOp) {}

  void Emit(const Expr *E);
  void VisitVarExpr(const VarExpr *E);
  void VisitImplicitCastExpr(const ImplicitCastExpr *E);
  void VisitUnaryExpr(const UnaryExpr *E);
  void VisitBinaryExpr(const BinaryExpr *E);

  const Expr *getLastEmmittedArray() const {
    return LastArrayEmmitted;
  }
};

void ScalarEmmitterAndSectionGatherer::Emit(const Expr *E) {
  if(E->getType()->isArrayType())
    Visit(E);
  else ArrayOp.EmitScalarValue(CGF, E);
}

void ScalarEmmitterAndSectionGatherer::VisitVarExpr(const VarExpr *E) {
  ArrayOp.EmitArraySections(CGF, E);
  LastArrayEmmitted = E;
}

void ScalarEmmitterAndSectionGatherer::VisitImplicitCastExpr(const ImplicitCastExpr *E) {
  Emit(E->getExpression());
}

void ScalarEmmitterAndSectionGatherer::VisitUnaryExpr(const UnaryExpr *E) {
  Emit(E->getExpression());
}

void ScalarEmmitterAndSectionGatherer::VisitBinaryExpr(const BinaryExpr *E) {
  Emit(E->getLHS());
  Emit(E->getRHS());
}

void ArrayOperation::EmitAllScalarValuesAndArraySections(CodeGenFunction &CGF, const Expr *E) {
  ScalarEmmitterAndSectionGatherer EV(CGF, *this);
  EV.Emit(E);
}

ArrayValueTy ArrayOperation::EmitLHSArray(CodeGenFunction &CGF, const Expr *E) {
  ScalarEmmitterAndSectionGatherer EV(CGF, *this);
  EV.Emit(E);
  return getArrayValue(EV.getLastEmmittedArray());
}

//
// Foreach element in given sections loop emmitter for array operations
//

ArrayLoopEmmitter::ArrayLoopEmmitter(CodeGenFunction &cgf,
                                             ArrayRef<ArraySection> LHS)
  : CGF(cgf), Builder(cgf.getBuilder()), Sections(LHS) { }


llvm::Value *ArrayLoopEmmitter::EmitSectionIndex(const ArrayRangeSection &Range,
                                                 int Dimension) {
  // compute dimension index -> index = base + loop_index * stride
  auto StridedIndex = !Range.hasStride()? Elements[Dimension] :
                        Builder.CreateMul(Elements[Dimension], Range.Stride);
  return Range.hasOffset()? Builder.CreateAdd(Range.Offset, StridedIndex) :
                            StridedIndex;
}

llvm::Value *ArrayLoopEmmitter::EmitSectionIndex(const ArraySection &Section,
                                                 int Dimension) {
  if(Section.isRangeSection())
    return EmitSectionIndex(Section.getRangeSection(), Dimension);
  else
    return Section.getElementSection().Index;
}

// FIXME: add support for vector sections.
void ArrayLoopEmmitter::EmitArrayIterationBegin() {
  auto IndexType = CGF.ConvertType(CGF.getContext().IntegerTy);

  Elements.resize(Sections.size());
  Loops.resize(Sections.size());

  // Foreach section from back to front (column major
  // order for efficient memory access).
  for(auto I = Sections.size(); I!=0;) {
    --I;
    if(Sections[I].isRangeSection()) {
      auto Range = Sections[I].getRangeSection();
      auto Var = CGF.CreateTempAlloca(IndexType,"array-dim-loop-counter");
      Builder.CreateStore(llvm::ConstantInt::get(IndexType, 0), Var);
      auto LoopCond = CGF.createBasicBlock("array-dim-loop");
      auto LoopBody = CGF.createBasicBlock("array-dim-loop-body");
      auto LoopEnd = CGF.createBasicBlock("array-dim-loop-end");
      CGF.EmitBlock(LoopCond);
      Builder.CreateCondBr(Builder.CreateICmpULT(Builder.CreateLoad(Var), Range.Size),
                           LoopBody, LoopEnd);
      CGF.EmitBlock(LoopBody);
      Elements[I] = Builder.CreateLoad(Var);

      Loops[I].EndBlock = LoopEnd;
      Loops[I].TestBlock = LoopCond;
      Loops[I].Counter = Var;
    } else {
      Elements[I] = nullptr;
      Loops[I].EndBlock = nullptr;
    }
  }
}

void ArrayLoopEmmitter::EmitArrayIterationEnd() {
  auto IndexType = CGF.ConvertType(CGF.getContext().IntegerTy);

  // foreach loop from front to back.
  for(auto Loop : Loops) {
    if(Loop.EndBlock) {
      Builder.CreateStore(Builder.CreateAdd(Builder.CreateLoad(Loop.Counter),
                                            llvm::ConstantInt::get(IndexType, 1)),
                          Loop.Counter);
      CGF.EmitBranch(Loop.TestBlock);
      CGF.EmitBlock(Loop.EndBlock);
    }
  }
}

llvm::Value *ArrayLoopEmmitter::EmitElementOffset(ArrayRef<ArraySection> Sections) {
  auto Offset = EmitSectionIndex(Sections[0], 0);
  if(Sections.size() > 1) {
    auto SizeProduct = Sections[0].getDimensionSize();
    for(size_t I = 1; I < Sections.size(); ++I) {
      auto Sub = Builder.CreateMul(EmitSectionIndex(Sections[I], I),
                                   SizeProduct);
      Offset = Builder.CreateAdd(Offset, Sub);
      if((I + 1) < Sections.size())
        SizeProduct = Builder.CreateMul(SizeProduct, Sections[I].getDimensionSize());
    }
  }
  return Offset;
}

llvm::Value *ArrayLoopEmmitter::EmitElementPointer(ArrayValueTy Array) {
  return Builder.CreateGEP(Array.Ptr, EmitElementOffset(Array.Sections));
}

//
// Multidimensional loop body emmitter for array operations.
//

/// ArrayOperationEmmitter - Emits the array expression for the current
/// iteration of the multidimensional array loop.
class ArrayOperationEmmitter : public ConstExprVisitor<ArrayOperationEmmitter, RValueTy> {
  CodeGenFunction   &CGF;
  CGBuilderTy       &Builder;
  ArrayOperation    &Operation;
  ArrayLoopEmmitter &Looper;
public:

  ArrayOperationEmmitter(CodeGenFunction &cgf, ArrayOperation &Op,
                         ArrayLoopEmmitter &Loop)
    : CGF(cgf), Builder(cgf.getBuilder()), Operation(Op),
      Looper(Loop) {}

  RValueTy Emit(const Expr *E);
  RValueTy VisitVarExpr(const VarExpr *E);
  RValueTy VisitImplicitCastExpr(const ImplicitCastExpr *E);
  RValueTy VisitUnaryExpr(const UnaryExpr *E);
  RValueTy VisitBinaryExpr(const BinaryExpr *E);

  static QualType ElementType(const Expr *E) {
    return cast<ArrayType>(E->getType().getTypePtr())->getElementType();
  }
};

RValueTy ArrayOperationEmmitter::Emit(const Expr *E) {
  if(E->getType()->isArrayType())
    return ConstExprVisitor::Visit(E);
  return Operation.getScalarValue(E);
}

RValueTy ArrayOperationEmmitter::VisitVarExpr(const VarExpr *E) {
  return CGF.EmitLoad(Looper.EmitElementPointer(Operation.getArrayValue(E)), ElementType(E));
}

RValueTy ArrayOperationEmmitter::VisitImplicitCastExpr(const ImplicitCastExpr *E) {
  return CGF.EmitImplicitConversion(Emit(E->getExpression()), E->getType());
}

RValueTy ArrayOperationEmmitter::VisitUnaryExpr(const UnaryExpr *E) {
  return CGF.EmitUnaryExpr(E->getOperator(), Emit(E->getExpression()));
}

RValueTy ArrayOperationEmmitter::VisitBinaryExpr(const BinaryExpr *E) {
  return CGF.EmitBinaryExpr(E->getOperator(), Emit(E->getLHS()), Emit(E->getRHS()));
}

static void EmitArrayAssignment(CodeGenFunction &CGF, ArrayOperation &Op,
                                ArrayLoopEmmitter &Looper, ArrayValueTy LHS,
                                const Expr *RHS) {
  ArrayOperationEmmitter EV(CGF, Op, Looper);
  auto Val = EV.Emit(RHS);
  CGF.EmitStore(Val, Looper.EmitElementPointer(LHS), RHS->getType());
}

//
//
//

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

llvm::Value *CodeGenFunction::EmitArrayPtr(const Expr *E) {
  ArrayValueExprEmitter EV(*this);
  EV.EmitExpr(E);
  // FIXME strided array - allocate memory and pack / unpack
  return EV.getResultPtr();
}

void CodeGenFunction::EmitArrayConstructorToKnownSizeAssignment(const ArrayType *LHSType,
                                                                uint64_t LHSSize,
                                                                llvm::Value *LHSPtr,
                                                                ArrayRef<Expr*> RHS) {
  assert(RHS.size() == LHSSize);
  for(uint64_t I = 0; I < LHSSize; ++I) {
    auto Dest = Builder.CreateConstInBoundsGEP1_64(LHSPtr, I);
    EmitAssignment(LValueTy(Dest, LHSType->getElementType()),
                   EmitRValue(RHS[I]));
  }
}

void CodeGenFunction::EmitArrayAssignment(const Expr *LHS, const Expr *RHS) {  
  auto LHSType = cast<ArrayType>(LHS->getType().getTypePtr());

  if(auto AC = dyn_cast<ArrayConstructorExpr>(RHS)) {
    uint64_t LHSSize;
    if(LHSType->EvaluateSize(LHSSize, getContext())) {
      ArrayValueExprEmitter EV(*this);
      EV.EmitExpr(LHS);
      auto Ptr = EV.getResultPtr();
      EmitArrayConstructorToKnownSizeAssignment(LHSType, LHSSize,
                                                Ptr, AC->getItems());
      return;
    }
  }

  ArrayOperation OP;
  auto LHSArray = OP.EmitLHSArray(*this, LHS);
  OP.EmitAllScalarValuesAndArraySections(*this, RHS);
  ArrayLoopEmmitter Looper(*this, LHSArray.Sections);
  Looper.EmitArrayIterationBegin();
  // Array = array / scalar
  CodeGen::EmitArrayAssignment(*this, OP, Looper, LHSArray, RHS);
  Looper.EmitArrayIterationEnd();
}

}
} // end namespace flang
