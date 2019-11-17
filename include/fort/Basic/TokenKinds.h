//===--- TokenKinds.h - Enum values for Fortran Token Kinds -----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the TokenKind enum and support functions.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_FORT_TOKENKINDS_H__
#define LLVM_FORT_TOKENKINDS_H__

#include "llvm/Support/Compiler.h"

namespace fort {
namespace tok {

/// TokenKind - This provides a simple uniform namespace for tokens from all
/// Fortran languages.
enum TokenKind : unsigned short {
#define TOK(X) X,
#include "fort/Basic/TokenKinds.def"
  NUM_TOKENS
};

/// \brief Determines the name of a token as used within the front end.
///
/// The name of a token will be an internal name (such as "l_paren") and should
/// not be used as part of diagnostic messages.
const char *getTokenName(enum TokenKind Kind);

/// Determines the spelling of simple punctuation tokens like
/// '!' or '%', and returns NULL for literal and annotation tokens.
///
/// This routine only retrieves the "simple" spelling of the token,
/// and will not produce any alternative spellings (e.g., a
/// digraph). For the actual spelling of a given Token, use
/// Preprocessor::getSpelling().
const char *getPunctuatorSpelling(TokenKind Kind) LLVM_READNONE;

/// Determines the spelling of simple keyword and contextual keyword
/// tokens like 'int' and 'dynamic_cast'. Returns NULL for other token kinds.
const char *getKeywordSpelling(TokenKind Kind) LLVM_READNONE;

/// \brief Determines the spelling of simple punctuation tokens like '**' or
/// '(', and returns NULL for literal and annotation tokens.
///
/// This routine only retrieves the "simple" spelling of the token, and will not
/// produce any alternative spellings.
const char *getTokenSimpleSpelling(enum TokenKind Kind);

} // end namespace tok
} // end namespace fort

#endif
