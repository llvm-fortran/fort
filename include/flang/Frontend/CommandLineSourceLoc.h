//===--- CommandLineSourceLoc.h - Parsing for source locations-*- C++ -*---===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Command line parsing for source locations.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_FLANG_FRONTEND_COMMANDLINESOURCELOC_H
#define LLVM_FLANG_FRONTEND_COMMANDLINESOURCELOC_H

#include "clang/Frontend/CommandLineSourceLoc.h"

namespace flang {

typedef clang::ParsedSourceLocation ParsedSourceLocation;

} // end namespace flang

#endif
