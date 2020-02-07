//===- Version.cpp - Fort Version Number -----------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines several version-related utility functions for Fort.
//
//===----------------------------------------------------------------------===//

#include "fort/Basic/Version.h"
#include "fort/Basic/LLVM.h"
#include "fort/Config/config.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdlib>
#include <cstring>

#ifdef HAVE_VCS_VERSION_INC
#include "VCSVersion.inc"
#endif

namespace fort {

std::string getFortRepositoryPath() {
#if defined(FORT_REPOSITORY_STRING)
  return FORT_REPOSITORY_STRING;
#else
#ifdef FORT_REPOSITORY
  return FORT_REPOSITORY;
#else
  return "";
#endif
#endif
}

std::string getLLVMRepositoryPath() {
#ifdef LLVM_REPOSITORY
  return LLVM_REPOSITORY;
#else
  return "";
#endif
}

std::string getFortRevision() {
#ifdef FORT_REVISION
  return FORT_REVISION;
#else
  return "";
#endif
}

std::string getLLVMRevision() {
#ifdef LLVM_REVISION
  return LLVM_REVISION;
#else
  return "";
#endif
}

std::string getFortFullRepositoryVersion() {
  std::string buf;
  llvm::raw_string_ostream OS(buf);
  std::string Path = getFortRepositoryPath();
  std::string Revision = getFortRevision();
  if (!Path.empty() || !Revision.empty()) {
    OS << '(';
    if (!Path.empty())
      OS << Path;
    if (!Revision.empty()) {
      if (!Path.empty())
        OS << ' ';
      OS << Revision;
    }
    OS << ')';
  }
  // Support LLVM in a separate repository.
  std::string LLVMRev = getLLVMRevision();
  if (!LLVMRev.empty() && LLVMRev != Revision) {
    OS << " (";
    std::string LLVMRepo = getLLVMRepositoryPath();
    if (!LLVMRepo.empty())
      OS << LLVMRepo << ' ';
    OS << LLVMRev << ')';
  }
  return OS.str();
}

std::string getFortFullVersion() { return getFortToolFullVersion("fort"); }

std::string getFortToolFullVersion(StringRef ToolName) {
  std::string buf;
  llvm::raw_string_ostream OS(buf);
#ifdef FORT_VENDOR
  OS << FORT_VENDOR;
#endif
  OS << ToolName << " version " FORT_VERSION_STRING " "
     << getFortFullRepositoryVersion();

  // If vendor supplied, include the base LLVM version as well.
#ifdef FORT_VENDOR
  OS << " (based on " << BACKEND_PACKAGE_STRING << ")";
#endif

  return OS.str();
}

std::string getFortFullCPPVersion() {
  // The version string we report in __VERSION__ is just a compacted version of
  // the one we report on the command line.
  std::string buf;
  llvm::raw_string_ostream OS(buf);
#ifdef FORT_VENDOR
  OS << FORT_VENDOR;
#endif
  OS << "Fort " FORT_VERSION_STRING " " << getFortFullRepositoryVersion();
  return OS.str();
}

} // end namespace fort
