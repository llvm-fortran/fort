//===--- TextDiagnosticBuffer.cpp - Buffer Text Diagnostics ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This is a concrete diagnostic client, which buffers the diagnostic messages.
//
//===----------------------------------------------------------------------===//

#include "flang/Frontend/TextDiagnosticBuffer.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/ADT/Twine.h"
using namespace flang;

/// HandleDiagnostic - Store the errors, warnings, and notes that are
/// reported.
///
void TextDiagnosticBuffer::HandleDiagnostic(Diagnostic::Level DiagLevel, llvm::SMLoc L,
                                            const llvm::Twine &Msg) {
  // Default implementation (Warnings/errors count).
  DiagnosticClient::HandleDiagnostic(DiagLevel, L, Msg);

  switch (DiagLevel) {
  default: llvm_unreachable(
                         "Diagnostic not handled during diagnostic buffering!");
  case Diagnostic::Note:
    Notes.push_back(std::make_pair(L, Msg.str()));
    break;
  case Diagnostic::Warning:
    Warnings.push_back(std::make_pair(L, Msg.str()));
    break;
  case Diagnostic::Error:
  case Diagnostic::Fatal:
    Errors.push_back(std::make_pair(L, Msg.str()));
    break;
  }
}

void TextDiagnosticBuffer::FlushDiagnostics(Diagnostic &Diags) const {
  // FIXME: Flush the diagnostics in order.
  for (const_iterator it = err_begin(), ie = err_end(); it != ie; ++it)
    Diags.ReportError(it->first,it->second);
  for (const_iterator it = warn_begin(), ie = warn_end(); it != ie; ++it)
    Diags.ReportWarning(it->first,it->second);
  for (const_iterator it = note_begin(), ie = note_end(); it != ie; ++it)
    Diags.ReportNote(it->first,it->second);
}

