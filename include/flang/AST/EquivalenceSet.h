//===--- EquivalenceSet.h - A set of objects from one EQUIVALENCE block ---===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines a set of objects which share the same memory block.
//
//===----------------------------------------------------------------------===//

#ifndef FLANG_AST_EQUIVALENCESET_H
#define FLANG_AST_EQUIVALENCESET_H

#include "flang/Basic/LLVM.h"

namespace flang {

class ASTContext;
class VarDecl;
class Expr;

class EquivalenceSet {
public:
  class Object {
  public:
    VarDecl *Var;
    const Expr *E;

    Object() {}
    Object(VarDecl *var, const Expr *e)
      : Var(var), E(e) {}
  };

private:
  Object *Objects;
  unsigned ObjectCount;

  EquivalenceSet(ASTContext &C, ArrayRef<Object> objects);
public:

  static EquivalenceSet *Create(ASTContext &C, ArrayRef<Object> Objects);

  ArrayRef<Object> getObjects() const {
    return ArrayRef<Object>(Objects, ObjectCount);
  }
};

} // end flang namespace

#endif
