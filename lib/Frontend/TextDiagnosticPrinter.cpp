//===--- TextDiagnosticPrinter.cpp - Diagnostic Printer -------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This diagnostic client prints out their diagnostic messages.
//
//===----------------------------------------------------------------------===//

#include "fort/Frontend/TextDiagnosticPrinter.h"
#include "llvm/ADT/None.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>

namespace fort {

TextDiagnosticPrinter::~TextDiagnosticPrinter() {}

void TextDiagnosticPrinter::HandleDiagnostic(DiagnosticsEngine::Level Level,
                                             SourceLocation L,
                                             const llvm::Twine &Msg,
                                             llvm::ArrayRef<SourceRange> Ranges,
                                             llvm::ArrayRef<FixItHint> FixIts) {
  // Default implementation (Warnings/errors count).
  DiagnosticClient::HandleDiagnostic(Level, L, Msg, Ranges, FixIts);
  llvm::SourceMgr::DiagKind MsgTy;
  switch (Level) {
  case DiagnosticsEngine::Ignored:
    return;
  case DiagnosticsEngine::Note:
    MsgTy = llvm::SourceMgr::DK_Note;
    break;
  case DiagnosticsEngine::Warning:
    MsgTy = llvm::SourceMgr::DK_Warning;
    break;
  case DiagnosticsEngine::Error:
    MsgTy = llvm::SourceMgr::DK_Error;
    break;
  case DiagnosticsEngine::Fatal:
    MsgTy = llvm::SourceMgr::DK_Error;
    break;
  }

  SrcMgr.PrintMessage(L, MsgTy, Msg, Ranges, FixIts, true);
}

} // namespace fort
