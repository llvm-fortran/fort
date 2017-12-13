//===- Version.h - Fort Version Number -------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief Defines version macros and version-related utility functions
/// for Fort.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_FORT_BASIC_VERSION_H
#define LLVM_FORT_BASIC_VERSION_H

#include "fort/Basic/Version.inc"
#include "llvm/ADT/StringRef.h"

namespace fort {
  /// \brief Retrieves the repository path (e.g., Subversion path) that
  /// identifies the particular Fort branch, tag, or trunk from which this
  /// Fort was built.
  std::string getFortRepositoryPath();

  /// \brief Retrieves the repository path from which LLVM was built.
  ///
  /// This supports LLVM residing in a separate repository from fort.
  std::string getLLVMRepositoryPath();

  /// \brief Retrieves the repository revision number (or identifer) from which
  /// this Fort was built.
  std::string getFortRevision();

  /// \brief Retrieves the repository revision number (or identifer) from which
  /// LLVM was built.
  ///
  /// If Fort and LLVM are in the same repository, this returns the same
  /// string as getFortRevision.
  std::string getLLVMRevision();

  /// \brief Retrieves the full repository version that is an amalgamation of
  /// the information in getFortRepositoryPath() and getFortRevision().
  std::string getFortFullRepositoryVersion();

  /// \brief Retrieves a string representing the complete fort version,
  /// which includes the fort version number, the repository version,
  /// and the vendor tag.
  std::string getFortFullVersion();

  /// \brief Like getFortFullVersion(), but with a custom tool name.
  std::string getFortToolFullVersion(llvm::StringRef ToolName);

  /// \brief Retrieves a string representing the complete fort version suitable
  /// for use in the CPP __VERSION__ macro, which includes the fort version
  /// number, the repository version, and the vendor tag.
  std::string getFortFullCPPVersion();
}

#endif // LLVM_FORT_BASIC_VERSION_H
