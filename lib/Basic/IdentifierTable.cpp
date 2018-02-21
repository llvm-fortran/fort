//===--- IdentifierTable.cpp - Hash table for identifier lookup -----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the IdentifierInfo, IdentifierVisitor, and
// IdentifierTable interfaces.
//
//===----------------------------------------------------------------------===//

#include "fort/Basic/IdentifierTable.h"
#include "fort/Basic/LangOptions.h"
#include "llvm/ADT/FoldingSet.h"
#include "llvm/ADT/StringRef.h"
#include <cstdio>
using namespace fort;

//===----------------------------------------------------------------------===//
// IdentifierInfo Implementation
//===----------------------------------------------------------------------===//

IdentifierInfo::IdentifierInfo()
    : TokenID(tok::identifier), FETokenInfo(0), Entry(0) {}

//===----------------------------------------------------------------------===//
// IdentifierTable Implementation
//===----------------------------------------------------------------------===//

IdentifierInfoLookup::~IdentifierInfoLookup() {}

IdentifierTable::IdentifierTable(const LangOptions &LangOpts,
                                 IdentifierInfoLookup *externalLookup)
    : IdentifierHashTable(8192), // Start with space for 8K identifiers.
      KeywordHashTable(64),      // Start with space for 64 keywords.
      FormatSpecHashTable(32),   // Start with space for 32 format specs.
      ExternalLookup(externalLookup) {

  // Populate the identifier table with info about keywords for the current
  // language.
  AddPredefineds(LangOpts);
}

//===----------------------------------------------------------------------===//
// Language Keyword Implementation
//===----------------------------------------------------------------------===//

// Constants for TokenKinds.def
namespace {
enum {
  KEYALL = 0x01,
  KEYF77 = 0x02,
  KEYF90 = 0x04,
  KEYF95 = 0x08,
  KEYF2003 = 0x10,
  KEYF2008 = 0x20,
  KEYNOTF77 = 0x40
};
}

/// AddPredefined - This method is used to associate a token ID with specific
/// identifiers because they are language keywords or built-ins. This causes the
/// lexer to automatically map matching identifiers to specialized token codes.
static void AddPredefined(llvm::StringRef ID, tok::TokenKind TokenCode,
                          unsigned Flags, const LangOptions &LangOpts,
                          IdentifierTable &Table, bool isFormatSpec) {
  if ((Flags & KEYALL) || (!LangOpts.Fortran77 && (Flags & KEYNOTF77)) ||
      (LangOpts.Fortran77 && (Flags & KEYF77)) ||
      (LangOpts.Fortran90 && (Flags & KEYF90)) ||
      (LangOpts.Fortran95 && (Flags & KEYF95)) ||
      (LangOpts.Fortran2003 && (Flags & KEYF2003)) ||
      (LangOpts.Fortran2008 && (Flags & KEYF2008))) {
    if (!isFormatSpec)
      Table.getKeyword(ID, TokenCode);
    else
      Table.getFormatSpec(ID, TokenCode);
  }
}

/// AddPredefineds - Add all predefined identifiers to the symbol tables.
void IdentifierTable::AddPredefineds(const LangOptions &LangOpts) {
  // Add keywords and tokens for the current language.
#define KEYWORD(NAME, FLAGS)                                                   \
  AddPredefined(llvm::StringRef(#NAME), tok::kw_##NAME, FLAGS, LangOpts,       \
                *this, false);
#define FORMAT_SPEC(NAME, FLAGS)                                               \
  AddPredefined(llvm::StringRef(#NAME), tok::fs_##NAME, FLAGS, LangOpts,       \
                *this, true);
#undef OPERATOR
#define OPERATOR(NAME, FLAGS)
#include "fort/Basic/TokenKinds.def"
}

//===----------------------------------------------------------------------===//
// Stats Implementation
//===----------------------------------------------------------------------===//

/// PrintStats - Print statistics about how well the identifier table is doing
/// at hashing identifiers.
void IdentifierTable::PrintStats() const {
  unsigned NumBuckets = IdentifierHashTable.getNumBuckets() +
                        KeywordHashTable.getNumBuckets() +
                        FormatSpecHashTable.getNumBuckets();
  unsigned NumIdentifiers = IdentifierHashTable.getNumItems() +
                            KeywordHashTable.getNumItems() +
                            FormatSpecHashTable.getNumItems();
  unsigned NumEmptyBuckets = NumBuckets - NumIdentifiers;
  unsigned AverageIdentifierSize = 0;
  unsigned MaxIdentifierLength = 0;

  // TODO: Figure out maximum times an identifier had to probe for -stats.
  for (llvm::StringMap<IdentifierInfo *, llvm::BumpPtrAllocator>::const_iterator
           I = IdentifierHashTable.begin(),
           E = IdentifierHashTable.end();
       I != E; ++I) {
    unsigned IdLen = I->getKeyLength();
    AverageIdentifierSize += IdLen;
    if (MaxIdentifierLength < IdLen)
      MaxIdentifierLength = IdLen;
  }

  for (llvm::StringMap<IdentifierInfo *, llvm::BumpPtrAllocator>::const_iterator
           I = KeywordHashTable.begin(),
           E = KeywordHashTable.end();
       I != E; ++I) {
    unsigned IdLen = I->getKeyLength();
    AverageIdentifierSize += IdLen;
    if (MaxIdentifierLength < IdLen)
      MaxIdentifierLength = IdLen;
  }

  for (llvm::StringMap<IdentifierInfo *, llvm::BumpPtrAllocator>::const_iterator
           I = FormatSpecHashTable.begin(),
           E = FormatSpecHashTable.end();
       I != E; ++I) {
    unsigned IdLen = I->getKeyLength();
    AverageIdentifierSize += IdLen;
    if (MaxIdentifierLength < IdLen)
      MaxIdentifierLength = IdLen;
  }

  fprintf(stderr, "\n*** Identifier Tables Stats:\n");
  fprintf(stderr, "# Identifiers:   %u\n", NumIdentifiers);
  fprintf(stderr, "# Empty Buckets: %u\n", NumEmptyBuckets);
  fprintf(stderr, "Hash density (#identifiers per bucket): %f\n",
          NumIdentifiers / (double)NumBuckets);
  fprintf(stderr, "Ave identifier length: %f\n",
          (AverageIdentifierSize / (double)NumIdentifiers));
  fprintf(stderr, "Max identifier length: %u\n", MaxIdentifierLength);

  // Compute statistics about the memory allocated for identifiers
  IdentifierHashTable.getAllocator().PrintStats();
  KeywordHashTable.getAllocator().PrintStats();
  FormatSpecHashTable.getAllocator().PrintStats();
}
