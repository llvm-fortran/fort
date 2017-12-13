//===--- ASTConsumers.h - ASTConsumer implementations -----------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// AST Consumers.
//
//===----------------------------------------------------------------------===//

#ifndef FORT_FRONTEND_ASTCONSUMERS_H
#define FORT_FRONTEND_ASTCONSUMERS_H

#include "fort/Basic/LLVM.h"

namespace llvm {
  namespace sys { class Path; }
}

namespace fort {

class ASTConsumer;

// AST dumper: dumps the raw AST in human-readable form to stderr; this is
// intended for debugging.
ASTConsumer *CreateASTDumper(StringRef FilterString);

} // end namespace fort

#endif
