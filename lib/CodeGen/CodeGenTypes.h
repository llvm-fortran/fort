//===--- CodeGenTypes.h - Type translation for LLVM CodeGen -----*- C++ -*-===//
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

#ifndef FLANG_CODEGEN_CODEGENTYPES_H
#define FLANG_CODEGEN_CODEGENTYPES_H

#include "CGCall.h"
#include "flang/AST/Decl.h"
#include "flang/AST/Type.h"
#include "flang/Frontend/CodeGenOptions.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/Module.h"
#include <vector>

namespace llvm {
  class FunctionType;
  class Module;
  class DataLayout;
  class Type;
  class LLVMContext;
  class StructType;
  class IntegerType;
}

namespace flang {
  class ASTContext;
  class TargetInfo;

namespace CodeGen {
  class CodeGenModule;

/// CodeGenTypes - This class organizes the cross-module state that is used
/// while lowering AST types to LLVM types.
class CodeGenTypes {
public:
  CodeGenModule &CGM;

public:
  CodeGenTypes(CodeGenModule &cgm);
  ~CodeGenTypes();

  /// ConvertType - Convert type T into a llvm::Type.
  llvm::Type *ConvertType(QualType T);

  /// ConvertTypeForMem - Convert type T into a llvm::Type.  This differs from
  /// ConvertType in that it is used to convert to the memory representation for
  /// a type.  For example, the scalar representation for _Bool is i1, but the
  /// memory representation is usually i8 or i32, depending on the target.
  llvm::Type *ConvertTypeForMem(QualType T);

  CGFunctionInfo GetFunctionType(const FunctionDecl *FD);
  llvm::Type *GetComplexType(llvm::Type *ElementType);

  llvm::Type *ConvertBuiltInType(const BuiltinType *T, const ExtQuals *Ext);
  llvm::Type *ConvertBuiltInType(BuiltinType::TypeSpec Spec,
                                 BuiltinType::TypeKind Kind);

  llvm::Type *ConvertReturnType(QualType T);
  llvm::Type *ConvertArgumentType(QualType T);

};

}  // end namespace CodeGen
}  // end namespace flang

#endif
