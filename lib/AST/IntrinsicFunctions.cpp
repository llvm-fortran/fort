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
  nullptr
};

const char *intrinsic::getFunctionName(FunctionKind Kind) {
  assert(Kind < NUM_FUNCTIONS && "Invalid function kind!");
  return FunctionNames[Kind];
}

namespace intrinsic {

FunctionMapping::FunctionMapping(const LangOptions &) {
  for(unsigned I = 0; I < NUM_FUNCTIONS; ++I)
    Mapping[getFunctionName(FunctionKind(I))] = FunctionKind(I);
}

FunctionMapping::Result FunctionMapping::Resolve(const IdentifierInfo *IDInfo) {
  auto It = Mapping.find(IDInfo->getName());
  if(It == Mapping.end()) {
    Result Res = { NUM_FUNCTIONS, true };
    return Res;
  }
  Result Res = { It->getValue(), false };
  return Res;
}

} // end namespace intrinsic
} // end namespace flang
