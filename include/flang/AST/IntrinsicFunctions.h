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

namespace flang {
namespace intrinsic {

/// FunctionKind - This provides a simple uniform namespace for
/// intrinsic functions from all Fortran languages.
enum FunctionKind {
#define INTRINSIC_FUNCTION(NAME, VERSION) NAME,
#include "IntrinsicFunctions.def"
#undef INTRINSIC_FUNCTION
  NUM_FUNCTIONS
};

/// \brief Returns the "simple" name of the functions (The specific
/// typed overloads which may be used in original source code are
/// not taken into consideration)
const char *getFunctionName(FunctionKind Kind);

}  // end namespace intrinsic
}  // end namespace flang

#endif
