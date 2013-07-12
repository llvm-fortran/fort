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

llvm::Type *CodeGenTypes::ConvertType(QualType T) {
  auto Ext = T.getExtQualsPtrOnNull();
  auto TPtr = T.getTypePtr();
  if(const BuiltinType *BTy = dyn_cast<BuiltinType>(TPtr))
    return ConvertBuiltInType(BTy, Ext);

  return nullptr;
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

llvm::Type *CodeGenTypes::ConvertTypeForMem(QualType T) {
    return ConvertType(T);
}

llvm::Type *CodeGenTypes::ConvertReturnType(QualType T) {
  return ConvertType(T);
}

llvm::Type *CodeGenTypes::ConvertArgumentType(QualType T) {
  return llvm::PointerType::get(ConvertType(T), 0);
}

CGFunctionInfo CodeGenTypes::GetFunctionType(const FunctionDecl *FD) {
  auto ReturnType = FD->getType().isNull() ? CGM.VoidTy :
                                             ConvertReturnType(FD->getType());
  auto Args = FD->getArguments();
  SmallVector<llvm::Type*,8> ArgTypes(Args.size());
  for(size_t I = 0; I < Args.size(); ++I)
    ArgTypes[I] = ConvertArgumentType(Args[I]->getType());

  CGFunctionInfo Result;
  Result.Type = llvm::FunctionType::get(ReturnType, ArgTypes, false);
  Result.CC = llvm::CallingConv::C;
  return Result;
}

} } // end namespace flang


