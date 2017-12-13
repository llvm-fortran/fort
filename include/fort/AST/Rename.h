//===-- Rename.h - Renamed Objects ------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//

#ifndef FORT_AST_RENAME_H__
#define FORT_AST_RENAME_H__

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_stream.h"

namespace fort {

//===----------------------------------------------------------------------===//
/// Rename - Used in a 'USE' statement to rename a module name to a local name.
///
class Rename {
  llvm::StringRef LocalName;
  llvm::StringRef UseName;
public:
  Rename(llvm::StringRef LN, llvm::StringRef UN)
    : LocalName(LN), UseName(UN) {}

  friend llvm::raw_ostream &operator<<(llvm::raw_ostream &O, const Rename &RN) {
    return O << RN.LocalName << " => " << RN.UseName;
  }
};

} // end fort namespace

#endif // FORT_AST_RENAME_H__
