//===--- CodeGenTypes.cpp - Type translation for LLVM CodeGen -------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This is the code that handles AST -> LLVM type lowering.
//
//===----------------------------------------------------------------------===//

#include "CodeGenTypes.h"
#include "CodeGenModule.h"
#include "flang/AST/ASTContext.h"
#include "flang/AST/Decl.h"
#include "flang/AST/Expr.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Module.h"

namespace flang {
namespace CodeGen {

CodeGenTypes::CodeGenTypes(CodeGenModule &cgm)
  : CGM(cgm), Context(cgm.getContext()) {
}

CodeGenTypes::~CodeGenTypes() { }

uint64_t CodeGenTypes::GetCharacterTypeLength(QualType T) {
  auto Ext = T.getExtQualsPtrOrNull();
  return Ext && Ext->hasLengthSelector()?
           Ext->getLengthSelector() : 1;
}

llvm::Type *CodeGenTypes::ConvertType(QualType T) {
  auto Ext = T.getExtQualsPtrOrNull();
  auto TPtr = T.getTypePtr();
  if(const BuiltinType *BTy = dyn_cast<BuiltinType>(TPtr))
    return ConvertBuiltInType(BTy, Ext);
  else if(const ArrayType *ATy = dyn_cast<ArrayType>(TPtr))
    return ConvertArrayType(ATy);
  else if(const FunctionType *FTy = dyn_cast<FunctionType>(TPtr))
    return ConvertFunctionType(FTy);

  return nullptr;
}

llvm::Type *CodeGenTypes::ConvertBuiltInTypeForMem(const BuiltinType *T,
                                                   const ExtQuals *Ext) {
  if(T->getTypeSpec() != BuiltinType::Character)
    return ConvertBuiltInType(T, Ext);
  return llvm::ArrayType::get(CGM.Int8Ty, Ext && Ext->hasLengthSelector()?
                                            Ext->getLengthSelector() : 1);
}

llvm::Type *CodeGenTypes::ConvertBuiltInType(const BuiltinType *T,
                                             const ExtQuals *Ext) {
  BuiltinType::TypeKind Kind;
  switch(T->getTypeSpec()) {
  case BuiltinType::Integer:
    Kind = CGM.getContext().getIntTypeKind(Ext);
    break;
  case BuiltinType::Real:
    Kind = CGM.getContext().getRealTypeKind(Ext);
    break;
  case BuiltinType::Complex:
    Kind = CGM.getContext().getComplexTypeKind(Ext);
    break;
  case BuiltinType::Logical:
    Kind = CGM.getContext().getLogicalTypeKind(Ext);
    break;

  case BuiltinType::Character:
    llvm::Type *Pair[2] = { CGM.Int8PtrTy, CGM.SizeTy };
    return llvm::StructType::get(CGM.getLLVMContext(),
                                 ArrayRef<llvm::Type*>(Pair,2));
  }
  return ConvertBuiltInType(T->getTypeSpec(),
                            Kind);
}

llvm::Type *CodeGenTypes::ConvertBuiltInType(BuiltinType::TypeSpec Spec,
                                             BuiltinType::TypeKind Kind) {
  llvm::Type *Type;
  switch(Kind) {
  case BuiltinType::Int1:
    Type = CGM.Int8Ty;
    break;
  case BuiltinType::Int2:
    Type = CGM.Int16Ty;
    break;
  case BuiltinType::Int4:
    Type = CGM.Int32Ty;
    break;
  case BuiltinType::Int8:
    Type = CGM.Int64Ty;
    break;
  case BuiltinType::Real4:
    Type = CGM.FloatTy;
    break;
  case BuiltinType::Real8:
    Type = CGM.DoubleTy;
    break;
  case BuiltinType::Real16:
    Type = llvm::Type::getFP128Ty(CGM.getLLVMContext());
    break;
  }

  if(Spec == BuiltinType::Complex)
    return GetComplexType(Type);
  return Type;
}

llvm::Type *CodeGenTypes::GetComplexType(llvm::Type *ElementType) {
  llvm::Type *Pair[2] = { ElementType, ElementType };
  return llvm::StructType::get(CGM.getLLVMContext(),
                               ArrayRef<llvm::Type*>(Pair,2));
}

llvm::Type *CodeGenTypes::GetCharacterType(llvm::Type *PtrType) {
  llvm::Type *Pair[2] = { PtrType, CGM.SizeTy };
  return llvm::StructType::get(CGM.getLLVMContext(),
                               ArrayRef<llvm::Type*>(Pair,2));
}

llvm::Type *CodeGenTypes::GetComplexTypeAsVector(llvm::Type *ElementType) {
  return llvm::VectorType::get(ElementType, 2);
}

llvm::Type *CodeGenTypes::ConvertTypeForMem(QualType T) {
  auto Ext = T.getExtQualsPtrOrNull();
  auto TPtr = T.getTypePtr();
  if(const BuiltinType *BTy = dyn_cast<BuiltinType>(TPtr))
    return ConvertBuiltInTypeForMem(BTy, Ext);
  else if(const ArrayType *ATy = dyn_cast<ArrayType>(TPtr))
    return ConvertArrayTypeForMem(ATy);

  llvm_unreachable("invalid type");
  return nullptr;
}

llvm::Type *CodeGenTypes::ConvertFunctionType(const FunctionType *T) {
  return llvm::PointerType::get(GetFunctionType(T->getPrototype())->getFunctionType(), 0);
}

const CGFunctionInfo *CodeGenTypes::GetFunctionType(const FunctionDecl *FD) {
  CGFunctionInfo::RetInfo ReturnInfo;
  ReturnInfo.ABIInfo = DefaultABI.GetRetABI(FD->getType());
  auto ReturnType = ConvertReturnType(FD->getType(), ReturnInfo);

  auto Args = FD->getArguments();
  SmallVector<CGFunctionInfo::ArgInfo, 16> ArgInfo;
  SmallVector<llvm::Type*, 16> ArgTypes;
  SmallVector<llvm::Type*, 4> AdditionalArgTypes;
  for(size_t I = 0; I < Args.size(); ++I) {
    auto ArgType = Args[I]->getType();
    CGFunctionInfo::ArgInfo Info;
    Info.ABIInfo = DefaultABI.GetArgABI(ArgType);
    ConvertArgumentType(ArgTypes, AdditionalArgTypes, ArgType, Info);
    ArgInfo.push_back(Info);
  }
  if(ReturnInfo.ABIInfo.getKind() == ABIRetInfo::CharacterValueAsArg) {
    ArgInfo.push_back(ReturnInfo.ReturnArgInfo);
    ConvertArgumentType(ArgTypes, AdditionalArgTypes,
                        FD->getType(), ReturnInfo.ReturnArgInfo);
  }
  for(auto I : AdditionalArgTypes) ArgTypes.push_back(I);

  // FIXME: fold same infos into one?
  auto Result = CGFunctionInfo::Create(Context, llvm::CallingConv::C,
                                       llvm::FunctionType::get(ReturnType, ArgTypes, false),
                                       ArgInfo,
                                       ReturnInfo);
  return Result;
}

const CGFunctionInfo *
CodeGenTypes::GetFunctionType(FortranABI &ABI,
                              ArrayRef<CGType> Args,
                              CGType ReturnType) {
  CGFunctionInfo::RetInfo ReturnInfo;
  llvm::Type *RetType;
  if(ReturnType.isQualType()) {
    ReturnInfo.ABIInfo = ABI.GetRetABI(ReturnType.asQualType());
    RetType = ConvertReturnType(ReturnType.asQualType(), ReturnInfo);
  }
  else {
    ReturnInfo.ABIInfo = ABIRetInfo(ABIRetInfo::Value);
    ReturnInfo.Kind = CGFunctionInfo::RetInfo::ScalarValue;
    RetType = ReturnType.asLLVMType();
  }

  SmallVector<CGFunctionInfo::ArgInfo, 8> ArgInfo;
  SmallVector<llvm::Type*, 8> ArgTypes;
  SmallVector<llvm::Type*, 4> AdditionalArgTypes;
  for(size_t I = 0; I < Args.size(); ++I) {
    CGFunctionInfo::ArgInfo Info;
    if(Args[I].isQualType()) {
      Info.ABIInfo = ABI.GetArgABI(Args[I].asQualType());
      ConvertArgumentType(ArgTypes, AdditionalArgTypes,
                          Args[I].asQualType(), Info);
    } else {
      Info.ABIInfo = ABIArgInfo(ABIArgInfo::Value);
      ArgTypes.push_back(Args[I].asLLVMType());
    }
    ArgInfo.push_back(Info);
  }
  for(auto I : AdditionalArgTypes) ArgTypes.push_back(I);

  auto Result = CGFunctionInfo::Create(Context, llvm::CallingConv::C,
                                       llvm::FunctionType::get(RetType, ArgTypes, false),
                                       ArgInfo,
                                       ReturnInfo);
  return Result;
}

} } // end namespace flang


