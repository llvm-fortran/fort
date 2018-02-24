//===--- TextDiagnosticBuffer.h - Buffer Text Diagnostics -------*- C++ -*-===//
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

#ifndef LLVM_FORT_FRONTEND_TEXT_DIAGNOSTIC_BUFFER_H_
#define LLVM_FORT_FRONTEND_TEXT_DIAGNOSTIC_BUFFER_H_

#include "fort/Basic/Diagnostic.h"
#include <string>
#include <vector>

namespace fort {

class Lexer;

class TextDiagnosticBuffer : public DiagnosticClient {
public:
  typedef std::vector<std::pair<SourceLocation, std::string>> DiagList;
  typedef DiagList::iterator iterator;
  typedef DiagList::const_iterator const_iterator;

private:
  DiagList Errors, Warnings, Notes;

public:
  const_iterator err_begin() const { return Errors.begin(); }
  const_iterator err_end() const { return Errors.end(); }

  const_iterator warn_begin() const { return Warnings.begin(); }
  const_iterator warn_end() const { return Warnings.end(); }

  const_iterator note_begin() const { return Notes.begin(); }
  const_iterator note_end() const { return Notes.end(); }

  virtual void HandleDiagnostic(DiagnosticsEngine::Level DiagLevel,
                                SourceLocation L, const llvm::Twine &Msg,
                                llvm::ArrayRef<SourceRange> Ranges,
                                llvm::ArrayRef<FixItHint> FixIts);

  /// FlushDiagnostics - Flush the buffered diagnostics to an given
  /// diagnostic engine.
  void FlushDiagnostics(DiagnosticsEngine &Diags) const;
};

} // namespace fort

#endif
