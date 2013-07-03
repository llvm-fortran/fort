//===--- ASTDumper.h - Dump Fortran AST ------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file declares the functions that dump the AST.
//
//===----------------------------------------------------------------------===//

#ifndef FLANG_AST_STMTDUMPER_H__
#define FLANG_AST_STMTDUMPER_H__

#include "flang/Sema/Ownership.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/Support/raw_ostream.h"

namespace flang {

  /// print - Prints an expression.
  void print(llvm::raw_ostream &OS, const Expr *E);

  /// dump - Dump a statement.
  void dump(StmtResult S);

  /// dump - Dump an array of statements.
  void dump(llvm::ArrayRef<StmtResult> S);

} // end flang namespace

#endif
