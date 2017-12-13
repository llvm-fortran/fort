//===--- SemaDiagnostic.h - Diagnostics for semantics --------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_FORT_SEMADIAGNOSTIC_H
#define LLVM_FORT_SEMADIAGNOSTIC_H

#include "fort/Basic/Diagnostic.h"

namespace fort {
  namespace diag {
    enum {
#define DIAG(ENUM,FLAGS,DEFAULT_MAPPING,DESC,GROUP,\
             SFINAE,ACCESS,NOWERROR,SHOWINSYSHEADER,CATEGORY) ENUM,
#define SEMASTART
#include "fort/Basic/DiagnosticSemaKinds.inc"
#undef DIAG
      NUM_BUILTIN_SEMA_DIAGNOSTICS
    };
  }  // end namespace diag
}  // end namespace fort

#endif
