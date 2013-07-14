//===----- ABIInfo.h - ABI information access & encapsulation ---*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef FLANG_CODEGEN_ABIINFO_H
#define FLANG_CODEGEN_ABIINFO_H

#include "flang/AST/Type.h"

namespace flang {

/// ABIArgInfo - Helper class to encapsulate information about how a
/// specific Fortran type should be passed to a function.
class ABIArgInfo {
public:
  enum Kind {
    /// Passes by value.
    /// scalar - simple value.
    /// complex - aggregate value (real, im)
    /// character - aggregate value (ptr, len)
    Value,

    /// Passes a scalar/complex by reference
    Reference,

    /// Passes an aggregate as separate arguments
    /// complex - two arguments (real, im)
    /// character - two arguments (ptr, len)
    Expand,

    /// Passes a complex by value using a vector type.
    ComplexValueAsVector
  };

private:
  Kind TheKind;
public:

  ABIArgInfo(Kind K) :
    TheKind(K) {}

  Kind getKind() const { return TheKind; }
};

/// ABIRetInfo - Helper class to encapsulate information about how a
/// specific Fortran type should be returned from a function.
class ABIRetInfo {
public:
  enum Kind {
    /// Returns void
    Nothing,

    /// Returns a value
    /// scalar - simple value
    /// complex - aggregate value (real, im)
    Value,

    /// Returns a character value using an argument
    CharacterValueAsArg
  };
private:
  Kind TheKind;
public:

  ABIRetInfo(Kind K = Nothing) :
    TheKind(K) {}

  Kind getKind() const { return TheKind; }
};

}  // end namespace flang

#endif
