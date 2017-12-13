//===----- CGIORuntime.h - Interface to IO Runtimes -------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This provides an abstract class for IO statements code generation.
//
//===----------------------------------------------------------------------===//

#include "CGIORuntime.h"
#include "CodeGenFunction.h"

namespace fort {
namespace CodeGen {

CGIORuntime::~CGIORuntime() {
}

}
} // end namespace fort

