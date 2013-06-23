//===--- IntrinsicFunctions.cpp - Intrinsic Function Kinds Support -------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements the FunctionKind enum and support functions.
//
//===----------------------------------------------------------------------===//

#include "flang/AST/IntrinsicFunctions.h"
#include <cassert>

namespace flang {

static char const * const FunctionNames[] = {
#define INTRINSIC_FUNCTION(NAME, VERSION) #NAME,
#include "flang/AST/IntrinsicFunctions.def"
#undef INTRINSIC_FUNCTION
  nullptr
};

const char *intrinsic::getFunctionName(FunctionKind Kind) {
  assert(Kind < NUM_FUNCTIONS && "Invalid function kind!");
  return FunctionNames[Kind];
}

} // end namespace flang
