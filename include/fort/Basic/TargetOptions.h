//===--- TargetOptions.h ----------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief Defines the flang::TargetOptions class.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_FLANG_FRONTEND_TARGETOPTIONS_H
#define LLVM_FLANG_FRONTEND_TARGETOPTIONS_H

#include <string>
#include <vector>
#include "fort/Basic/LLVM.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"

namespace flang {

/// \brief Options for controlling the target.
class TargetOptions : public llvm::RefCountedBase<TargetOptions> {
public:
  /// If given, the name of the target triple to compile for. If not given the
  /// target will be selected to match the host.
  std::string Triple;

  /// If given, the name of the target CPU to generate code for.
  std::string CPU;

  /// If given, the unit to use for floating point math
  std::string FPMath;

  /// If given, the name of the target ABI to use.
  std::string ABI;

  /// The EABI version to use
  llvm::EABI EABIVersion;

  /// If given, the version string of the linker in use.
  std::string LinkerVersion;

  /// \brief The list of target specific features to enable or disable, as written on the command line.
  std::vector<std::string> FeaturesAsWritten;

  /// The list of target specific features to enable or disable -- this should
  /// be a list of strings starting with by '+' or '-'.
  std::vector<std::string> Features;
};

}  // end namespace flang

#endif
