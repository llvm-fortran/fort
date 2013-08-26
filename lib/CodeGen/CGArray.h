//===--- CGArray.cpp - Emit LLVM Code for Array operations and Expr -----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef FLANG_CODEGEN_CGARRAY_H
#define FLANG_CODEGEN_CGARRAY_H

#include "CGValue.h"
#include "CodeGenFunction.h"
#include "flang/AST/ExprVisitor.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/DenseMap.h"

namespace flang {
namespace CodeGen {

class ArrayValueTy {
public:
  ArrayRef<ArraySection> Sections;
  llvm::Value *Ptr;

  ArrayValueTy(ArrayRef<ArraySection> sections, llvm::Value *P)
    : Sections(sections), Ptr(P) {}
};

/// ArrayOperation - Represents an array expression / statement.
class ArrayOperation {
  struct StoredArrayValue {
    size_t SectionsOffset;
    llvm::Value *Ptr;
  };
  llvm::SmallDenseMap<const Expr*, StoredArrayValue, 8> Arrays;

  SmallVector<ArraySection, 32> Sections;
public:

  /// \brief Returns the array value used for the given expression.
  ArrayValueTy getArrayValue(const Expr *E);

  /// \brief Emits the array sections used for the given expression.
  void EmitArraySections(CodeGenFunction &CGF, const Expr *E);

  /// \brief Walks the entire array expression AST and emits array sections
  /// for all relevant array expressions.
  void EmitAllArraySections(CodeGenFunction &CGF, const Expr *E);
};

/// ArrayLoopEmmitter - Emits the multidimensional loop which
/// is used to iterate over array sections in an array expression.
class ArrayLoopEmmitter {
private:
  /// Loop - stores some information about the generated loops.
  struct Loop {
    llvm::BasicBlock *EndBlock;
    llvm::BasicBlock *TestBlock;
    llvm::Value *Counter;
  };

  CodeGenFunction &CGF;
  CGBuilderTy &Builder;
  ArrayRef<ArraySection> Sections;
  /// ElementInfo - stores the current loop index for all
  /// dimensions, or null if the loop index doesn't apply
  /// (i.e. element section).
  SmallVector<llvm::Value *, 8> Elements;
  SmallVector<Loop, 8> Loops;
public:

  ArrayLoopEmmitter(CodeGenFunction &cgf, ArrayRef<ArraySection> LHS);

  /// EmitSectionIndex - computes the index of the element during
  /// the current iteration of the multidimensional loop
  /// for the given dimension.
  llvm::Value *EmitSectionIndex(const ArrayRangeSection &Range,
                                int Dimension);

  llvm::Value *EmitSectionIndex(const ArraySection &Section,
                                int Dimension);

  /// EmitElementOffset - computes the offset of the
  /// current element in the given array.
  llvm::Value *EmitElementOffset(ArrayRef<ArraySection> Sections);

  /// EmitElelementPointer - returns the pointer to the current
  /// element in the given array.
  llvm::Value *EmitElementPointer(ArrayValueTy Array);

  /// EmitArrayIterationBegin - Emits the beginning of a
  /// multidimensional loop which iterates over the given array section.
  void EmitArrayIterationBegin();

  /// EmitArrayIterationEnd - Emits the end of a
  /// multidimensional loop which iterates over the given array section.
  void EmitArrayIterationEnd();
};

}
}  // end namespace flang

#endif
