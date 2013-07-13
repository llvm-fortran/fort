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
  : CGM(cgm) {
}

CodeGenTypes::~CodeGenTypes() { }

uint64_t CodeGenTypes::GetCharacterTypeLength(QualType T) {
  auto Ext = T.getExtQualsPtrOnNull();
  return 1; //FIXME
}

llvm::Type *CodeGenTypes::ConvertType(QualType T) {
  auto Ext = T.getExtQualsPtrOnNull();
  auto TPtr = T.getTypePtr();
  if(const BuiltinType *BTy = dyn_cast<BuiltinType>(TPtr))
    return ConvertBuiltInType(BTy, Ext);

  return nullptr;
}

llvm::Type *CodeGenTypes::ConvertBuiltInTypeForMem(const BuiltinType *T,
                                                   const ExtQuals *Ext) {
  if(T->getTypeSpec() != BuiltinType::Character)
    return ConvertBuiltInType(T, Ext);
  return llvm::ArrayType::get(CGM.Int8Ty, 1);
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
    return llvm::IntegerType::get(CGM.getLLVMContext(), 1);

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
  auto Ext = T.getExtQualsPtrOnNull();
  auto TPtr = T.getTypePtr();
  if(const BuiltinType *BTy = dyn_cast<BuiltinType>(TPtr))
    return ConvertBuiltInTypeForMem(BTy, Ext);

  return nullptr;
}

llvm::Type *CodeGenTypes::ConvertReturnType(QualType T, ABIRetInfo &RetInfo) {
  if(T->isCharacterType()) {
    RetInfo = ABIRetInfo(ABIRetInfo::CharacterValueAsArg, ABIArgInfo::Value);
    return CGM.VoidTy;
  }
  else if(T->isComplexType())
    RetInfo = ABIRetInfo(ABIRetInfo::ComplexValue);
  else
    RetInfo = ABIRetInfo(ABIRetInfo::ScalarValue);
  return ConvertType(T);
}

llvm::Type *CodeGenTypes::ConvertArgumentType(QualType T) {
  return llvm::PointerType::get(ConvertType(T), 0);
}

const CGFunctionInfo *CodeGenTypes::GetFunctionType(ASTContext &C,
                                                    const FunctionDecl *FD) {
  ABIRetInfo ReturnInfo;
  llvm::Type *ReturnType;
  if(FD->getType().isNull())
    ReturnType = CGM.VoidTy;
  else
    ReturnType = ConvertReturnType(FD->getType(), ReturnInfo);

  auto Args = FD->getArguments();
  SmallVector<CGFunctionInfo::ArgInfo, 8> ArgInfo;
  SmallVector<llvm::Type*, 8> ArgTypes;
  for(size_t I = 0; I < Args.size(); ++I) {
    auto ArgType = Args[I]->getType();
    //FIXME: arrays
    CGFunctionInfo::ArgInfo Info;
    llvm::Type *Type;
    if(ArgType->isCharacterType()) {
      Info.ABIInfo = ABIArgInfo(ABIArgInfo::Value);
      Type = ConvertType(ArgType);
    } else {
      Info.ABIInfo = ABIArgInfo(ABIArgInfo::Reference);
      Type = ConvertArgumentType(ArgType);
    }
    ArgInfo.push_back(Info);
    ArgTypes.push_back(Type);
  }
  if(ReturnInfo.getKind() == ABIRetInfo::CharacterValueAsArg) {
    CGFunctionInfo::ArgInfo Info;
    Info.ABIInfo = ABIArgInfo(ABIArgInfo::Value);
    ArgInfo.push_back(Info);
    ArgTypes.push_back(ConvertType(FD->getType()));
  }

  // FIXME: fold same infos into one?
  auto Result = CGFunctionInfo::Create(C, llvm::CallingConv::C,
                                       llvm::FunctionType::get(ReturnType, ArgTypes, false),
                                       ArgInfo,
                                       ReturnInfo);
  return Result;
}

const CGFunctionInfo *
CodeGenTypes::GetRuntimeFunctionType(ASTContext &C,
                                     ArrayRef<QualType> Args,
                                     QualType ReturnType) {
  ABIRetInfo ReturnInfo;
  llvm::Type *RetType;
  if(ReturnType.isNull())
    RetType = CGM.VoidTy;
  else
    RetType = ConvertReturnType(ReturnType, ReturnInfo);

  SmallVector<CGFunctionInfo::ArgInfo, 8> ArgInfo(Args.size());
  SmallVector<llvm::Type*, 8> ArgTypes;
  for(size_t I = 0; I < Args.size(); ++I) {
    if(Args[I]->isCharacterType()) {
      ArgInfo[I].ABIInfo = ABIArgInfo(ABIArgInfo::Expand);
      ArgTypes.push_back(CGM.Int8PtrTy); //FIXME: character kinds
      ArgTypes.push_back(CGM.SizeTy);
    } else if(Args[I]->isComplexType()) {
      ArgInfo[I].ABIInfo = ABIArgInfo(ABIArgInfo::Expand);
      auto ElementType = ConvertType(C.getComplexTypeElementType(Args[I]));
      ArgTypes.push_back(ElementType);
      ArgTypes.push_back(ElementType);
    } else {
      ArgInfo[I].ABIInfo = ABIArgInfo(ABIArgInfo::Value);
      ArgTypes.push_back(ConvertType(Args[I]));
    }
  }

  auto Result = CGFunctionInfo::Create(C, llvm::CallingConv::C,
                                       llvm::FunctionType::get(RetType, ArgTypes, false),
                                       ArgInfo,
                                       ReturnInfo);
  return Result;
}

} } // end namespace flang


