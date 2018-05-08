//===--- DriverDiagnostic.h - Diagnostics for driver ------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_FORT_DRIVERDIAGNOSTIC_H
#define LLVM_FORT_DRIVERDIAGNOSTIC_H

#include "fort/Basic/Diagnostic.h"

namespace fort {
namespace diag {
enum {
#define DIAG(ENUM, FLAGS, DEFAULT_MAPPING, DESC, GROUP, SFINAE, ACCESS,        \
             NOWERROR, SHOWINSYSHEADER, CATEGORY)                              \
  ENUM,
#define DRIVERSTART
#include "fort/Basic/DiagnosticDriverKinds.inc"
#undef DIAG
  NUM_BUILTIN_DRIVER_DIAGNOSTICS
};
} // end namespace diag
} // end namespace fort

#endif
