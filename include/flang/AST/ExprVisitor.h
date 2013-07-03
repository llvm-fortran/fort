//===--- ExprVisitor.h - Visitor for Expr subclasses ------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the ExprVisitor and ConstExprVisitor interfaces.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_FLANG_AST_EXPRVISITOR_H
#define LLVM_FLANG_AST_EXPRVISITOR_H

#include "flang/Basic/MakePtr.h"
#include "flang/AST/Expr.h"

namespace flang {

/// StmtVisitorBase - This class implements a simple visitor for Expr
/// subclasses.
template<template <typename> class Ptr, typename ImplClass, typename RetTy=void>
class ExprVisitorBase {
public:

#define PTR(CLASS) typename Ptr<CLASS>::type
#define DISPATCH(NAME, CLASS) \
 return static_cast<ImplClass*>(this)->Visit ## NAME(static_cast<PTR(CLASS)>(E))

  RetTy Visit(PTR(Expr) E) {

    // Top switch expr: dispatch to VisitFooExpr for each FooExpr.
    switch (E->getExprClass()) {
    default: llvm_unreachable("Unknown expr kind!");
#define ABSTRACT_EXPR(EXPR)
#define EXPR(CLASS, PARENT)                              \
    case Expr::CLASS ## Class: DISPATCH(CLASS, CLASS);
#include "flang/AST/ExprNodes.inc"
    }
  }

  // If the implementation chooses not to implement a certain visit method, fall
  // back on VisitExpr or whatever else is the superclass.
#define ABSTRACT_EXPR(E)
#define EXPR(CLASS, PARENT)                                   \
  RetTy Visit ## CLASS(PTR(CLASS) E) { DISPATCH(PARENT, PARENT); }
#include "flang/AST/ExprNodes.inc"

  // Base case, ignore it. :)
  RetTy VisitExpr(PTR(Expr) Node) { return RetTy(); }

#undef PTR
#undef DISPATCH
};

/// ExprVisitor - This class implements a simple visitor for Expr subclasses.
///
/// This class does not preserve constness of Stmt pointers (see also
/// ConstStmtVisitor).
template<typename ImplClass, typename RetTy=void>
class ExprVisitor
 : public ExprVisitorBase<make_ptr, ImplClass, RetTy> {};

/// ConstExprVisitor - This class implements a simple visitor for Expr
/// subclasses.
///
/// This class preserves constness of Stmt pointers (see also StmtVisitor).
template<typename ImplClass, typename RetTy=void>
class ConstExprVisitor
 : public ExprVisitorBase<make_const_ptr, ImplClass, RetTy> {};

}  // end namespace flang

#endif
