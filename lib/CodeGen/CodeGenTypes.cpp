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
  if(!Ext) {
    if(const BuiltinType *BTy = dyn_cast<BuiltinType>(T.getTypePtr()))
      return ConvertBuiltInType(BTy);
  }

  return nullptr;
}

llvm::Type *CodeGenTypes::ConvertBuiltInType(const BuiltinType *T) {
  switch(T->getTypeSpec()) {
  case BuiltinType::Integer:
    return CGM.Int32Ty;
    break;
  case BuiltinType::Real:
    return CGM.FloatTy;
    break;
  case BuiltinType::Complex: {
    llvm::Type *Pair[2] = { CGM.FloatTy, CGM.FloatTy };
    return llvm::StructType::get(CGM.getLLVMContext(),
                                 ArrayRef<llvm::Type*>(Pair,2));
  }
  case BuiltinType::Logical:
    return llvm::IntegerType::get(CGM.getLLVMContext(), 1);
    break;
  }
  return nullptr;
}

llvm::Type *CodeGenTypes::ConvertTypeForMem(QualType T) {
    return ConvertType(T);
}

llvm::FunctionType *CodeGenTypes::GetFunctionType(FunctionDecl *FD) {
  return nullptr;
}

} } // end namespace flang


