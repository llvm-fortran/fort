//===-- FixedForm.h - Fortran Fixed-form Parsing Utilities ------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef FLANG_PARSER_FIXEDFORM_H
#define FLANG_PARSER_FIXEDFORM_H

#include "flang/Basic/LLVM.h"
#include "flang/Basic/TokenKinds.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/ADT/ArrayRef.h"

namespace flang {
namespace fixedForm {

/// KeywordFilter - a set of given keyword(s).
class KeywordFilter {
  ArrayRef<tok::TokenKind> Keywords;
  tok::TokenKind SmallArray[3];
public:

  KeywordFilter(ArrayRef<tok::TokenKind> KW)
    : Keywords(KW) {}
  KeywordFilter(tok::TokenKind K1, tok::TokenKind K2,
                tok::TokenKind K3 = tok::unknown);

  ArrayRef<tok::TokenKind> getKeywords() const {
    return Keywords;
  }
};

/// KeywordMatcher - represents a set of keywords that
/// can be matched.
class KeywordMatcher {
  llvm::StringSet<llvm::BumpPtrAllocator> Keywords;
public:

  KeywordMatcher() {}
  KeywordMatcher(ArrayRef<KeywordFilter> Filters);

  void Register(tok::TokenKind Keyword);
  bool Matches(StringRef Identifier) const;
};



} // end namespace fixedForm

} // end namespace flang

#endif
