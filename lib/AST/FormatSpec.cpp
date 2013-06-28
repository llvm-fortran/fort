//===--- FormatSpec.cpp - Fortran FormatSpecifier -------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "flang/AST/FormatSpec.h"
#include "flang/AST/ASTContext.h"

namespace flang {

StarFormatSpec::StarFormatSpec(SourceLocation Loc)
  : FormatSpec(FormatSpec::FS_Star, Loc) {}

StarFormatSpec *StarFormatSpec::Create(ASTContext &C, SourceLocation Loc) {
  return new (C) StarFormatSpec(Loc);
}

DefaultCharFormatSpec::DefaultCharFormatSpec(SourceLocation L, ExprResult F)
  : FormatSpec(FormatSpec::FS_DefaultCharExpr, L), Fmt(F) {}

DefaultCharFormatSpec *DefaultCharFormatSpec::Create(ASTContext &C, SourceLocation Loc,
                                                   ExprResult Fmt) {
  return new (C) DefaultCharFormatSpec(Loc, Fmt);
}

LabelFormatSpec::LabelFormatSpec(SourceLocation L, StmtLabelReference Label)
  : FormatSpec(FormatSpec::FS_Label, L), StmtLabel(Label) {}

LabelFormatSpec *LabelFormatSpec::Create(ASTContext &C, SourceLocation Loc,
                                         StmtLabelReference Label) {
  return new (C) LabelFormatSpec(Loc, Label);
}

void LabelFormatSpec::setLabel(StmtLabelReference Label) {
  assert(!StmtLabel.Statement);
  assert(Label.Statement);
  StmtLabel = Label;
}

} //namespace flang
