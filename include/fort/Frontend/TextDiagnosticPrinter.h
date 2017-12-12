//===--- TextDiagnosticPrinter.h - Text Diagnostic Client -------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This is a concrete diagnostic client, which prints the diagnostics to
// standard error.
//
//===----------------------------------------------------------------------===//

#ifndef FLANG_FRONTEND_TEXT_DIAGNOSTIC_PRINTER_H_
#define FLANG_FRONTEND_TEXT_DIAGNOSTIC_PRINTER_H_

#include "fort/Basic/Diagnostic.h"

namespace llvm {
  class raw_ostream;
  class SourceLocation;
  class SourceMgr;
} // end namespace llvm

namespace flang {

class LangOptions;

class TextDiagnosticPrinter : public DiagnosticClient {
  llvm::SourceMgr &SrcMgr;
public:
  TextDiagnosticPrinter(llvm::SourceMgr &SM) : SrcMgr(SM) {}
  virtual ~TextDiagnosticPrinter();

  // TODO: Emit caret diagnostics and Highlight range.
  virtual void HandleDiagnostic(DiagnosticsEngine::Level DiagLevel, SourceLocation L,
                                const llvm::Twine &Msg,
                                llvm::ArrayRef<SourceRange> Ranges,
                                llvm::ArrayRef<FixItHint> FixIts);
};

} // end namespace flang

#endif
