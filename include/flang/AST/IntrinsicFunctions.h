//===----- IntrinsicFunctions.h - enum values for intrinsic functions -----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the FunctionKind enum and support functions.
//
//===----------------------------------------------------------------------===//

#ifndef FLANG_AST_INTRINSICFUNCTIONS_H__
#define FLANG_AST_INTRINSICFUNCTIONS_H__

#include "flang/Basic/IdentifierTable.h"
#include "flang/Basic/LangOptions.h"
#include "llvm/ADT/StringMap.h"

namespace flang {
namespace intrinsic {

enum FunctionArgumentCountKind {
  ArgumentCount1,
  ArgumentCount2,
  ArgumentCount1or2,
  ArgumentCount2orMore
};

/// FunctionKind - This provides a simple uniform namespace for
/// intrinsic functions from all Fortran languages.
enum FunctionKind {
#define INTRINSIC_FUNCTION(NAME, NUMARGS, VERSION) NAME,
#include "IntrinsicFunctions.def"
  NUM_FUNCTIONS
};

/// \brief Returns the "simple" name of the functions (The specific
/// typed overloads which may be used in original source code are
/// not taken into consideration)
const char *getFunctionName(FunctionKind Kind);

/// \brief Returns the number of arguments that the function accepts.
FunctionArgumentCountKind getFunctionArgumentCount(FunctionKind Function);

/// Maps the intrinsic function identifiers to function IDs
class FunctionMapping {
  llvm::StringMap<FunctionKind> Mapping;
public:
  FunctionMapping(const LangOptions &Options);

  struct Result {
    FunctionKind Function;
    bool IsInvalid;
  };

  Result Resolve(const IdentifierInfo *IDInfo);
};

}  // end namespace intrinsic
}  // end namespace flang

#endif
