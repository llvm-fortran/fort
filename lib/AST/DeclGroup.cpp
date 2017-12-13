//===--- DeclGroup.cpp - Classes for representing groups of Decls ----------==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements the DeclGroup and DeclGroupRef classes.
//
//===----------------------------------------------------------------------===//

#include "fort/AST/DeclGroup.h"
#include "fort/AST/Decl.h"
#include "fort/AST/ASTContext.h"
#include "llvm/Support/Allocator.h"
#include <type_traits>

namespace fort {

DeclGroup *DeclGroup::Create(ASTContext &C, Decl **Decls, unsigned NumDecls) {
  assert(NumDecls > 1 && "Invalid DeclGroup");
  unsigned Size = sizeof(DeclGroup) + sizeof(Decl*) * NumDecls;
  void* Mem = C.Allocate(Size, alignof(DeclGroup));
  new (Mem) DeclGroup(NumDecls, Decls);
  return static_cast<DeclGroup*>(Mem);
}

DeclGroup::DeclGroup(unsigned numdecls, Decl **decls) : NumDecls(numdecls) {
  assert(numdecls > 0);
  assert(decls);
  memcpy(this + 1, decls, numdecls * sizeof(*decls));
}

} //namespace fort
