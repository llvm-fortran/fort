//===--- FormatDesc.cpp - Fortran Format Items and Descriptors ------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "flang/AST/FormatItem.h"
#include "flang/AST/ASTContext.h"

namespace flang {

IntegerDataEditDesc::IntegerDataEditDesc(SourceLocation Loc, tok::TokenKind Descriptor,
                                         IntegerConstantExpr *RepeatCount,
                                         IntegerConstantExpr *w,
                                         IntegerConstantExpr *m)
  : DataEditDesc(Loc, Descriptor, RepeatCount, w), M(m) {
}

IntegerDataEditDesc *IntegerDataEditDesc::Create(ASTContext &C, SourceLocation Loc,
                                                 tok::TokenKind Descriptor,
                                                 IntegerConstantExpr *RepeatCount,
                                                 IntegerConstantExpr *W,
                                                 IntegerConstantExpr *M) {
  return new(C) IntegerDataEditDesc(Loc, Descriptor, RepeatCount, W, M);
}

RealDataEditDesc::RealDataEditDesc(SourceLocation Loc, tok::TokenKind Descriptor,
                                   IntegerConstantExpr *RepeatCount,
                                   IntegerConstantExpr *w,
                                   IntegerConstantExpr *d,
                                   IntegerConstantExpr *e)
  : DataEditDesc(Loc, Descriptor, RepeatCount, w), D(d), E(e) {
}

RealDataEditDesc *RealDataEditDesc::Create(ASTContext &C, SourceLocation Loc,
                                           tok::TokenKind Descriptor,
                                           IntegerConstantExpr *RepeatCount,
                                           IntegerConstantExpr *W,
                                           IntegerConstantExpr *D,
                                           IntegerConstantExpr *E) {

  return new(C) RealDataEditDesc(Loc, Descriptor, RepeatCount, W, D, E);
}

CharacterDataEditDesc::CharacterDataEditDesc(SourceLocation Loc, tok::TokenKind Descriptor,
                                             IntegerConstantExpr *RepeatCount,
                                             IntegerConstantExpr *w)
  : DataEditDesc(Loc, Descriptor, RepeatCount, w) {
}

CharacterDataEditDesc *CharacterDataEditDesc::Create(ASTContext &C, SourceLocation Loc,
                                                     tok::TokenKind Descriptor,
                                                     IntegerConstantExpr *RepeatCount,
                                                     IntegerConstantExpr *W) {
  return new(C) CharacterDataEditDesc(Loc, Descriptor, RepeatCount, W);
}

PositionEditDesc::PositionEditDesc(tok::TokenKind Descriptor, SourceLocation Loc,
                                   IntegerConstantExpr *N)
  : ControlEditDesc(Descriptor, Loc, N) {
}

PositionEditDesc *PositionEditDesc::Create(ASTContext &C, SourceLocation Loc,
                                           tok::TokenKind Descriptor,
                                           IntegerConstantExpr *N) {
  return new(C) PositionEditDesc(Descriptor, Loc, N);
}

CharacterStringEditDesc::CharacterStringEditDesc(CharacterConstantExpr *S)
  : FormatItem(fs_CharacterStringEditDesc, S->getLocation()), Str(S) {
}

CharacterStringEditDesc *CharacterStringEditDesc::Create(ASTContext &C,
                                                         CharacterConstantExpr *Str) {
  return new(C) CharacterStringEditDesc(Str);
}

FormatItemList::FormatItemList(ASTContext &C, SourceLocation Loc,
                               IntegerConstantExpr *Repeat,
                               ArrayRef<FormatItem*> Items)
  : FormatItem(fs_FormatItems, Loc), RepeatCount(Repeat) {
  N = Items.size();
  if(!N) {
    ItemList = nullptr;
    return;
  }
  ItemList = new (C) FormatItem *[N];
  for (unsigned I = 0; I != N; ++I)
    ItemList[I] = Items[I];
}

FormatItemList *FormatItemList::Create(ASTContext &C,
                                       SourceLocation Loc,
                                       IntegerConstantExpr *RepeatCount,
                                       ArrayRef<FormatItem*> Items) {
  return new(C) FormatItemList(C, Loc, RepeatCount, Items);
}

} // end namespace flang
