//===-- CGBuilder.h - Choose IRBuilder implementation  ----------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef FORT_CODEGEN_CGBUILDER_H
#define FORT_CODEGEN_CGBUILDER_H

#include "llvm/IR/IRBuilder.h"

namespace fort {
namespace CodeGen {

typedef llvm::IRBuilder<> CGBuilderTy; // FIXME more explicit type?

} // end namespace CodeGen
} // end namespace fort

#endif
