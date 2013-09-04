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
#include "flang/AST/StmtVisitor.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"

namespace flang {
namespace CodeGen {

llvm::ArrayType *CodeGenTypes::GetFixedSizeArrayType(const ArrayType *T,
                                                     uint64_t Size) {
  return llvm::ArrayType::get(ConvertTypeForMem(T->getElementType()),
                              Size);
}

llvm::Type *CodeGenTypes::ConvertArrayType(const ArrayType *T) {
  return llvm::PointerType::get(ConvertTypeForMem(T->getElementType()), 0);
}

llvm::ArrayType *CodeGenTypes::ConvertArrayTypeForMem(const ArrayType *T) {
  uint64_t ArraySize;
  if(T->EvaluateSize(ArraySize, Context))
    return GetFixedSizeArrayType(T, ArraySize);
  llvm_unreachable("invalid memory array type");
  return nullptr;
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

llvm::Value *CodeGenFunction::CreateTempHeapArrayAlloca(QualType T,
                                                        const ArrayValueTy &Value) {
  auto ETy = getTypes().ConvertTypeForMem(T.getSelfOrArrayElementType());
  auto PTy = llvm::PointerType::get(ETy, 0);
  auto Size = EmitArraySize(Value);
  Size = Builder.CreateMul(Size, llvm::ConstantInt::get(Size->getType(), CGM.getDataLayout().getTypeStoreSize(ETy)));
  return CreateTempHeapAlloca(Size, PTy);
}

void CodeGenFunction::GetArrayDimensionsInfo(QualType T, SmallVectorImpl<ArrayDimensionValueTy> &Dims) {
  auto ATy = cast<ArrayType>(T.getTypePtr());
  auto Dimensions = ATy->getDimensions();
  llvm::Value *Stride = llvm::ConstantInt::get(ConvertType(getContext().IntegerTy), 1);

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
    Dims.push_back(ArrayDimensionValueTy(LB, UB, I == 0? nullptr : Stride));
    if(I != Dimensions.size() - 1)
      Stride = Builder.CreateMul(Stride, EmitDimSize(Dims.back()));
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

llvm::Value *CodeGenFunction::EmitDimOffset(llvm::Value *Subscript,
                                            const ArrayDimensionValueTy &Dim) {
  // (S - LB) * Stride
  auto LB = Dim.hasLowerBound()? Dim.LowerBound :
                                 llvm::ConstantInt::get(Subscript->getType(), 1);
  auto Offset = Builder.CreateSub(Subscript, LB);
  if(Dim.hasStride())
    return Builder.CreateMul(Offset, Dim.Stride);
  return Offset;
}

llvm::Value *CodeGenFunction::EmitArrayOffset(ArrayRef<llvm::Value*> Subscripts,
                                              const ArrayValueTy &Value) {
  // return offset + (Subscript[i] - LowerBound[i] * Stride[i] for i in 0..Subscripts.size())
  assert(Subscripts.size() == Value.Dimensions.size());
  auto Offset = EmitDimOffset(Subscripts[0], Value.Dimensions[0]);
  for(size_t I = 1; I < Subscripts.size(); ++I)
    Offset = Builder.CreateAdd(Offset,
                               EmitDimOffset(Subscripts[I], Value.Dimensions[I]));
  return Value.hasOffset()? Builder.CreateAdd(Offset, Value.Offset) : Offset;
}

llvm::Value *CodeGenFunction::EmitArrayElementPtr(ArrayRef<llvm::Value*> Subscripts,
                                                  const ArrayValueTy &Value) {
  return Builder.CreateGEP(Value.Ptr, EmitArrayOffset(Subscripts, Value));
}

llvm::Value *CodeGenFunction::EmitSectionSize(const ArrayValueTy &Value, int I) {
  if(Value.Sections[I].isRangeSection())
    return EmitDimSize(Value.Dimensions[I]);
  else if(Value.Sections[I].isVectorSection())
    return Value.Sections[I].getVectorSection().Size;
  return nullptr;
}

llvm::Value *CodeGenFunction::EmitArraySize(const ArrayValueTy &Value) {
  llvm::Value *Size = nullptr;
  for(size_t I = 0; I < Value.Dimensions.size(); ++I) {
    auto DimSize = EmitSectionSize(Value, I);
    if(DimSize)
      Size = Size? Builder.CreateMul(Size, DimSize) : DimSize;
  }
  return Size;
}

llvm::Value *CodeGenFunction::EmitSliceFlatDifference(const ArrayDimensionValueTy &Dim,
                                                      llvm::Value *SliceLowerBound) {
  // (SliceLowerBound - LowerBound) * Stride
  auto LB = Dim.hasLowerBound()? Dim.LowerBound :
                                 llvm::ConstantInt::get(SliceLowerBound->getType(), 1);
  auto LBDiff = Builder.CreateSub(SliceLowerBound, LB);
  return Dim.hasStride()? Builder.CreateMul(LBDiff, Dim.Stride) : LBDiff;
}

// FIXME: (UB:LB:-Stride), (::Stride)
ArrayDimensionValueTy CodeGenFunction::
EmitArrayRangeSection(const ArrayDimensionValueTy &Dim,
                      llvm::Value *&Ptr, llvm::Value *&Offset,
                      llvm::Value *LB, llvm::Value *UB, llvm::Value *Stride) {
  if(!LB && !UB && !Stride)
    return Dim;
  if(LB) {
    auto Diff = EmitSliceFlatDifference(Dim, LB);
    Offset = Offset? Builder.CreateAdd(Offset, Diff) : Diff;
    auto NewUB = UB? Builder.CreateAdd(Builder.CreateSub(UB, LB),
                                       llvm::ConstantInt::get(LB->getType(), 1)) :
                   Builder.CreateSub(Builder.CreateAdd(EmitDimSize(Dim),
                                     llvm::ConstantInt::get(LB->getType(), 1)), LB);
    return ArrayDimensionValueTy(nullptr, NewUB, Dim.Stride);
  } else if(UB)
    return ArrayDimensionValueTy(nullptr, Dim.hasLowerBound()?
                                   Builder.CreateAdd(Builder.CreateSub(UB, Dim.LowerBound),
                                                     llvm::ConstantInt::get(UB->getType(), 1)) :
                                   UB, Dim.Stride);
}

ArrayValueExprEmitter::ArrayValueExprEmitter(CodeGenFunction &cgf, bool getPointer)
  : CGF(cgf), Builder(cgf.getBuilder()),
    VMContext(cgf.getLLVMContext()), GetPointer(getPointer), Ptr(nullptr),
    Offset(nullptr) { }

void ArrayValueExprEmitter::EmitExpr(const Expr *E) {
  Visit(E);
}

void ArrayValueExprEmitter::EmitSections() {
  Sections.resize(Dims.size());
}

void ArrayValueExprEmitter::VisitVarExpr(const VarExpr *E) {
  auto VD = E->getVarDecl();
  if(CGF.IsInlinedArgument(VD))
    return EmitExpr(CGF.GetInlinedArgumentValue(VD));
  if(VD->isParameter())
    return EmitExpr(VD->getInit());

  CGF.GetArrayDimensionsInfo(VD->getType(), Dims);
  if(GetPointer) {
    if(VD->isArgument())
      Ptr = CGF.GetVarPtr(VD);
    else
      Ptr = Builder.CreateConstInBoundsGEP2_32(CGF.GetVarPtr(VD), 0, 0);
  }
  EmitSections();
}

void ArrayValueExprEmitter::VisitArrayConstructorExpr(const ArrayConstructorExpr *E) {
  CGF.GetArrayDimensionsInfo(E->getType(), Dims);
  if(GetPointer)
    Ptr = CGF.EmitArrayConstructor(E);
  EmitSections();
}

void ArrayValueExprEmitter::IncrementOffset(llvm::Value *OffsetDelta) {
  Offset = Offset? Builder.CreateAdd(Offset, OffsetDelta) : OffsetDelta;
}

void ArrayValueExprEmitter::VisitArraySectionExpr(const ArraySectionExpr *E) {
  //FIXME
  ArrayValueExprEmitter TargetEmitter(CGF, GetPointer);
  TargetEmitter.EmitExpr(E->getTarget());
  Offset = TargetEmitter.Offset;
  Ptr = TargetEmitter.Ptr;

  auto Subscripts = E->getSubscripts();
  auto TargetDims = TargetEmitter.getDimensions();
  for(size_t I = 0; I < Subscripts.size(); ++I) {
    if(auto Range = dyn_cast<RangeExpr>(Subscripts[I])) {
      Sections.push_back(ArraySection());
      Dims.push_back(CGF.EmitArrayRangeSection(TargetDims[I], Ptr, Offset,
                       Range->hasFirstExpr()? CGF.EmitScalarExpr(Range->getFirstExpr()) :
                                              nullptr,
                       Range->hasSecondExpr()? CGF.EmitScalarExpr(Range->getSecondExpr()) :
                                               nullptr));
    }
    // FIXME: the rest
  }
}

StandaloneArrayValueSectionGatherer::StandaloneArrayValueSectionGatherer(CodeGenFunction &cgf,
                                                                         ArrayOperation &Op)
  : CGF(cgf), Gathered(false), Operation(Op) {
}

void StandaloneArrayValueSectionGatherer::EmitExpr(const Expr *E) {
  if(Gathered) return;
  if(E->getType()->isArrayType())
    Visit(E);
}

void StandaloneArrayValueSectionGatherer::GatherSections(const Expr *E) {
  auto Value = Operation.EmitArrayExpr(CGF, E);
  for(auto D : Value.Dimensions)
    Dims.push_back(D);
  for(auto S : Value.Sections)
    Sections.push_back(S);
  Gathered = true;
}

void StandaloneArrayValueSectionGatherer::VisitVarExpr(const VarExpr *E) {
  GatherSections(E);
}

void StandaloneArrayValueSectionGatherer::VisitArrayConstructorExpr(const ArrayConstructorExpr *E) {
  GatherSections(E);
}

void StandaloneArrayValueSectionGatherer::VisitBinaryExpr(const BinaryExpr *E) {
  EmitExpr(E->getLHS());
  EmitExpr(E->getRHS());
}

void StandaloneArrayValueSectionGatherer::VisitUnaryExpr(const UnaryExpr *E) {
  EmitExpr(E->getExpression());
}

void StandaloneArrayValueSectionGatherer::VisitImplicitCastExpr(const ImplicitCastExpr *E) {
  EmitExpr(E->getExpression());
}

void StandaloneArrayValueSectionGatherer::VisitIntrinsicCallExpr(const IntrinsicCallExpr *E) {
  // FIXME
  EmitExpr(E->getArguments()[0]);
}

void StandaloneArrayValueSectionGatherer::VisitArraySectionExpr(const ArraySectionExpr *E) {
  GatherSections(E);
}

//
// Scalar values and array sections emmitter for an array operations.
//

ArrayValueTy ArrayOperation::getArrayValue(const Expr *E) {
  auto Arr = Arrays[E];
  auto DimCount = E->getType()->asArrayType()->getDimensionCount();
  return ArrayValueTy(llvm::makeArrayRef(Dims.begin() + Arr.DataOffset, DimCount),
                      llvm::makeArrayRef(Sections.begin() + Arr.DataOffset, DimCount),
                        Arr.Ptr, Arr.Offset);
}

void ArrayOperation::EmitArraySections(CodeGenFunction &CGF, const Expr *E) {
  if(Arrays.find(E) != Arrays.end())
    return;

  ArrayValueExprEmitter EV(CGF);
  EV.EmitExpr(E);

  StoredArrayValue ArrayValue;
  ArrayValue.DataOffset = Sections.size();
  ArrayValue.Ptr = EV.getResult().Ptr;
  ArrayValue.Offset = EV.getResult().Offset;
  Arrays[E] = ArrayValue;

  for(auto S : EV.getSections())
    Sections.push_back(S);
  for(auto D : EV.getDimensions())
    Dims.push_back(D);
}

RValueTy ArrayOperation::getScalarValue(const Expr *E) {
  return Scalars[E];
}

void ArrayOperation::EmitScalarValue(CodeGenFunction &CGF, const Expr *E) {
  if(Scalars.find(E) != Scalars.end())
    return;

  Scalars[E] = CGF.EmitRValue(E);
}

class ScalarEmitterAndSectionGatherer : public ConstExprVisitor<ScalarEmitterAndSectionGatherer> {
  CodeGenFunction &CGF;
  ArrayOperation &ArrayOp;
  const Expr *LastArrayEmmitted;
public:

  ScalarEmitterAndSectionGatherer(CodeGenFunction &cgf, ArrayOperation &ArrOp)
    : CGF(cgf), ArrayOp(ArrOp), LastArrayEmmitted(nullptr) {}

  void Emit(const Expr *E);
  void VisitVarExpr(const VarExpr *E);
  void VisitImplicitCastExpr(const ImplicitCastExpr *E);
  void VisitUnaryExpr(const UnaryExpr *E);
  void VisitBinaryExpr(const BinaryExpr *E);
  void VisitArrayConstructorExpr(const ArrayConstructorExpr *E);
  void VisitArraySectionExpr(const ArraySectionExpr *E);

  const Expr *getLastEmmittedArray() const {
    return LastArrayEmmitted;
  }
};

void ScalarEmitterAndSectionGatherer::Emit(const Expr *E) {
  if(E->getType()->isArrayType())
    Visit(E);
  else ArrayOp.EmitScalarValue(CGF, E);
}

void ScalarEmitterAndSectionGatherer::VisitVarExpr(const VarExpr *E) {
  ArrayOp.EmitArraySections(CGF, E);
  LastArrayEmmitted = E;
}

void ScalarEmitterAndSectionGatherer::VisitImplicitCastExpr(const ImplicitCastExpr *E) {
  Emit(E->getExpression());
}

void ScalarEmitterAndSectionGatherer::VisitUnaryExpr(const UnaryExpr *E) {
  Emit(E->getExpression());
}

void ScalarEmitterAndSectionGatherer::VisitBinaryExpr(const BinaryExpr *E) {
  Emit(E->getLHS());
  Emit(E->getRHS());
}

void ScalarEmitterAndSectionGatherer::VisitArrayConstructorExpr(const ArrayConstructorExpr *E) {
  ArrayOp.EmitArraySections(CGF, E);
  LastArrayEmmitted = E;
}

void ScalarEmitterAndSectionGatherer::VisitArraySectionExpr(const ArraySectionExpr *E) {
  //FIXME
  ArrayOp.EmitArraySections(CGF, E);
  LastArrayEmmitted = E;
}

void ArrayOperation::EmitAllScalarValuesAndArraySections(CodeGenFunction &CGF, const Expr *E) {
  ScalarEmitterAndSectionGatherer EV(CGF, *this);
  EV.Emit(E);
}

ArrayValueTy ArrayOperation::EmitArrayExpr(CodeGenFunction &CGF, const Expr *E) {
  ScalarEmitterAndSectionGatherer EV(CGF, *this);
  EV.Emit(E);
  return getArrayValue(EV.getLastEmmittedArray());
}

//
// Foreach element in given sections loop emmitter for array operations
//

ArrayLoopEmitter::ArrayLoopEmitter(CodeGenFunction &cgf)
  : CGF(cgf), Builder(cgf.getBuilder())
{ }

void ArrayLoopEmitter::EmitArrayIterationBegin(const ArrayValueTy &Array) {
  auto IndexType = CGF.ConvertType(CGF.getContext().IntegerTy);

  auto Sections = Array.Sections;
  Elements.resize(Sections.size());
  Loops.resize(Sections.size());

  // Foreach section from back to front (column major
  // order for efficient memory access).
  for(auto I = Sections.size(); I!=0;) {
    --I;
    if(Sections[I].isRangeSection() ||
       Sections[I].isVectorSection()) {
      auto Var = CGF.CreateTempAlloca(IndexType,"array-dim-loop-counter");
      Builder.CreateStore(llvm::ConstantInt::get(IndexType, 0), Var);
      auto LoopCond = CGF.createBasicBlock("array-dim-loop");
      auto LoopBody = CGF.createBasicBlock("array-dim-loop-body");
      auto LoopEnd = CGF.createBasicBlock("array-dim-loop-end");
      CGF.EmitBlock(LoopCond);
      Builder.CreateCondBr(Builder.CreateICmpULT(Builder.CreateLoad(Var), CGF.EmitSectionSize(Array, I)),
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

void ArrayLoopEmitter::EmitArrayIterationEnd() {
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

llvm::Value *ArrayLoopEmitter::EmitSectionOffset(const ArrayValueTy &Array,
                                                int I) {
  if(Array.Sections[I].isRangeSection())
    return Array.Dimensions[I].hasStride()?
             Builder.CreateMul(Elements[I], Array.Dimensions[I].Stride) : Elements[I];
  // FIXME: element sections + vector sections.
  return nullptr;
}

llvm::Value *ArrayLoopEmitter::EmitElementOffset(const ArrayValueTy &Array) {
  auto Offset = EmitSectionOffset(Array, 0);
  for(size_t I = 1; I < Array.Dimensions.size(); ++I)
    Offset = Builder.CreateAdd(EmitSectionOffset(Array, I), Offset);
  return Array.hasOffset()? Builder.CreateAdd(Array.Offset, Offset) : Offset;
}

llvm::Value *ArrayLoopEmitter::EmitElementOneDimensionalIndex(const ArrayValueTy &Array) {
  auto Offset = EmitSectionOffset(Array, 0);
  return Builder.CreateAdd(Offset, llvm::ConstantInt::get(Offset->getType(), 1));
}

llvm::Value *ArrayLoopEmitter::EmitElementPointer(const ArrayValueTy &Array) {
  return Builder.CreateGEP(Array.Ptr, EmitElementOffset(Array));
}

//
// Multidimensional loop body emmitter for array operations.
//

ArrayOperationEmitter::
ArrayOperationEmitter(CodeGenFunction &cgf, ArrayOperation &Op,
                       ArrayLoopEmitter &Loop)
  : CGF(cgf), Builder(cgf.getBuilder()), Operation(Op),
    Looper(Loop) {}

RValueTy ArrayOperationEmitter::Emit(const Expr *E) {
  if(E->getType()->isArrayType())
    return ConstExprVisitor::Visit(E);
  return Operation.getScalarValue(E);
}

RValueTy ArrayOperationEmitter::VisitVarExpr(const VarExpr *E) {
  return CGF.EmitLoad(Looper.EmitElementPointer(Operation.getArrayValue(E)), ElementType(E));
}

RValueTy ArrayOperationEmitter::VisitImplicitCastExpr(const ImplicitCastExpr *E) {
  return CGF.EmitImplicitConversion(Emit(E->getExpression()), E->getType().getSelfOrArrayElementType());
}

RValueTy ArrayOperationEmitter::VisitUnaryExpr(const UnaryExpr *E) {
  return CGF.EmitUnaryExpr(E->getOperator(), Emit(E->getExpression()));
}

RValueTy ArrayOperationEmitter::VisitBinaryExpr(const BinaryExpr *E) {
  return CGF.EmitBinaryExpr(E->getOperator(), Emit(E->getLHS()), Emit(E->getRHS()));
}

RValueTy ArrayOperationEmitter::VisitArrayConstructorExpr(const ArrayConstructorExpr *E) {
  return CGF.EmitLoad(Looper.EmitElementPointer(Operation.getArrayValue(E)), ElementType(E));
}

RValueTy ArrayOperationEmitter::VisitArraySectionExpr(const ArraySectionExpr *E) {
  return CGF.EmitLoad(Looper.EmitElementPointer(Operation.getArrayValue(E)), ElementType(E));
}

LValueTy ArrayOperationEmitter::EmitLValue(const Expr *E) {
  return Looper.EmitElementPointer(Operation.getArrayValue(E));
}

static void EmitArrayAssignment(CodeGenFunction &CGF, ArrayOperation &Op,
                                ArrayLoopEmitter &Looper, ArrayValueTy LHS,
                                const Expr *RHS) {
  ArrayOperationEmitter EV(CGF, Op, Looper);
  auto Val = EV.Emit(RHS);
  CGF.EmitStore(Val, Looper.EmitElementPointer(LHS), RHS->getType());
}

static void EmitArrayAssignment(CodeGenFunction &CGF, ArrayOperation &Op,
                                ArrayLoopEmitter &Looper, const Expr *LHS,
                                const Expr *RHS) {
  ArrayOperationEmitter EV(CGF, Op, Looper);
  auto Val = EV.Emit(RHS);
  CGF.EmitStore(Val, EV.EmitLValue(LHS), RHS->getType());
}

static llvm::Value *EmitArrayConditional(CodeGenFunction &CGF, ArrayOperation &Op,
                                         ArrayLoopEmitter &Looper, const Expr *Condition) {
  ArrayOperationEmitter EV(CGF, Op, Looper);
  auto Val = EV.Emit(Condition).asScalar();
  if(Val->getType() != CGF.getModule().Int1Ty)
    return CGF.ConvertLogicalValueToInt1(Val);
  return Val;
}

//
//
//

llvm::Value *CodeGenFunction::EmitArrayElementPtr(const Expr *Target,
                                                  const ArrayRef<Expr*> Subscripts) {
  ArrayValueExprEmitter EV(*this);
  EV.EmitExpr(Target);
  llvm::SmallVector<llvm::Value*, 8> Subs(Subscripts.size());
  for(size_t I = 0; I < Subs.size(); ++I)
    Subs[I] = EmitScalarExpr(Subscripts[I]);
  return EmitArrayElementPtr(Subs, EV.getResult());
}

llvm::Value *CodeGenFunction::EmitArrayArgumentPointerValueABI(const Expr *E) {
  if(auto Temp = dyn_cast<ImplicitTempArrayExpr>(E)) {
    E = Temp->getExpression();
    ArrayOperation OP;
    StandaloneArrayValueSectionGatherer EV(*this, OP);
    EV.EmitExpr(E);
    auto Value = EV.getResult();
    auto DestPtr = CreateTempHeapArrayAlloca(E->getType(), Value);
    llvm::SmallVector<ArraySection, 8> DestSections;
    for(auto I : Value.Dimensions)
      DestSections.push_back(ArraySection());
    auto Dest = ArrayValueTy(Value.Dimensions, DestSections, DestPtr);
    OP.EmitAllScalarValuesAndArraySections(*this, E);
    ArrayLoopEmitter Looper(*this);
    Looper.EmitArrayIterationBegin(Value);
    CodeGen::EmitArrayAssignment(*this, OP, Looper, Dest, E);
    Looper.EmitArrayIterationEnd();
    return DestPtr;
  }
  else if(auto Pack = dyn_cast<ImplicitArrayPackExpr>(E)) {
    // FIXME strided array - allocate memory and pack / unpack
  }

  ArrayValueExprEmitter EV(*this);
  EV.EmitExpr(E);
  return EV.getPointer();
}

llvm::Value *CodeGenFunction::EmitConstantArrayConstructor(const ArrayConstructorExpr *E) {
  auto Items = E->getItems();
  SmallVector<llvm::Constant*, 16> Values(Items.size());
  for(size_t I = 0; I < Items.size(); ++I)
    Values[I] = EmitConstantExpr(Items[I]);
  auto Arr = llvm::ConstantArray::get(getTypes().ConvertArrayTypeForMem(E->getType()->asArrayType()),
                                      Values);
  return Builder.CreateConstGEP2_64(CGM.EmitConstantArray(Arr), 0, 0);
}

llvm::Value *CodeGenFunction::EmitTempArrayConstructor(const ArrayConstructorExpr *E) {
  // FIXME: implied-do, heap allocations

  auto Items = E->getItems();
  auto ATy = E->getType()->asArrayType();
  auto VMATy = getTypes().ConvertArrayTypeForMem(ATy);
  auto Arr = Builder.CreateConstGEP2_64(CreateTempAlloca(VMATy, "array-constructor-temp"), 0, 0);
  for(uint64_t I = 0, Size = VMATy->getArrayNumElements(); I < Size; ++I) {
    auto Dest = Builder.CreateConstInBoundsGEP1_64(Arr, I);
    EmitStore(EmitRValue(Items[I]), LValueTy(Dest), ATy->getElementType());
  }
  return Arr;
}

llvm::Value *CodeGenFunction::EmitArrayConstructor(const ArrayConstructorExpr *E) {
  if(E->isEvaluatable(getContext()))
    return EmitConstantArrayConstructor(E);
  return EmitTempArrayConstructor(E);
}

void CodeGenFunction::EmitArrayAssignment(const Expr *LHS, const Expr *RHS) {  
  ArrayOperation OP;
  auto LHSArray = OP.EmitArrayExpr(*this, LHS);
  OP.EmitAllScalarValuesAndArraySections(*this, RHS);
  ArrayLoopEmitter Looper(*this);
  Looper.EmitArrayIterationBegin(LHSArray);
  // Array = array / scalar
  CodeGen::EmitArrayAssignment(*this, OP, Looper, LHS, RHS);
  Looper.EmitArrayIterationEnd();
}

//
// Masked array assignment emmitter
//

class WhereBodyPreOperationEmmitter : public ConstStmtVisitor<WhereBodyPreOperationEmmitter> {
  CodeGenFunction &CGF;
  ArrayOperation  &Operation;
public:

  WhereBodyPreOperationEmmitter(CodeGenFunction &cgf, ArrayOperation &Op)
    : CGF(cgf), Operation(Op) {}

  void VisitBlockStmt(const BlockStmt *S) {
    for(auto I : S->getStatements())
      Visit(I);
  }
  void VisitAssignmentStmt(const AssignmentStmt *S) {
    Operation.EmitAllScalarValuesAndArraySections(CGF, S->getLHS());
    Operation.EmitAllScalarValuesAndArraySections(CGF, S->getRHS());
  }
  void VisitConstructPartStmt(const ConstructPartStmt*) {}
  void VisitStmt(const Stmt*) {
    llvm_unreachable("invalid where statement!");
  }
};

class WhereBodyEmmitter : public ConstStmtVisitor<WhereBodyEmmitter> {
  CodeGenFunction &CGF;
  ArrayOperation  &Operation;
  ArrayLoopEmitter &Looper;
public:

  WhereBodyEmmitter(CodeGenFunction &cgf, ArrayOperation &Op,
                    ArrayLoopEmitter &Loop)
    : CGF(cgf), Operation(Op), Looper(Loop) {}

  void VisitBlockStmt(const BlockStmt *S) {
    for(auto I : S->getStatements())
      Visit(I);
  }
  void VisitAssignmentStmt(const AssignmentStmt *S) {
    EmitArrayAssignment(CGF, Operation, Looper, S->getLHS(), S->getRHS());
  }
};

void CodeGenFunction::EmitWhereStmt(const WhereStmt *S) {
  // FIXME: evaluate the mask array before the loop (only if required?)
  // FIXME: evaluation of else scalars and sections must strictly follow the then body?

  ArrayOperation OP;
  auto MaskArray = OP.EmitArrayExpr(*this, S->getMask());
  WhereBodyPreOperationEmmitter BodyPreEmmitter(*this, OP);
  BodyPreEmmitter.Visit(S->getThenStmt());
  if(S->getElseStmt())
    BodyPreEmmitter.Visit(S->getElseStmt());

  ArrayLoopEmitter Looper(*this);
  Looper.EmitArrayIterationBegin(MaskArray);
  auto ThenBB = createBasicBlock("where-true");
  auto EndBB  = createBasicBlock("where-end");
  auto ElseBB = S->hasElseStmt()? createBasicBlock("where-else") : EndBB;
  Builder.CreateCondBr(EmitArrayConditional(*this, OP, Looper, S->getMask()), ThenBB, ElseBB);
  WhereBodyEmmitter BodyEmmitter(*this, OP, Looper);
  EmitBlock(ThenBB);
  BodyEmmitter.Visit(S->getThenStmt());
  EmitBranch(EndBB);
  if(S->hasElseStmt()) {
    EmitBlock(ElseBB);
    BodyEmmitter.Visit(S->getElseStmt());
    EmitBranch(EndBB);
  }
  EmitBlock(EndBB);
  Looper.EmitArrayIterationEnd();
}

}
} // end namespace flang
