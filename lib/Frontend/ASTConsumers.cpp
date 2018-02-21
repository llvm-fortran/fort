//===--- ASTConsumers.cpp - ASTConsumer implementations -------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// AST Consumer Implementations.
//
//===----------------------------------------------------------------------===//

#include "fort/Frontend/ASTConsumers.h"
#include "fort/AST/ASTConsumer.h"
#include "fort/AST/ASTContext.h"
#include <string>

using namespace fort;

namespace {

class ASTPrinter : public ASTConsumer {
public:
  std::string FilterString;

  ASTPrinter(StringRef Filter) : FilterString(Filter) {}

  void HandleTranslationUnit(ASTContext &Ctx) {
    Ctx.getTranslationUnitDecl()->dump();
  }
};

} // namespace

ASTConsumer *fort::CreateASTDumper(StringRef FilterString) {
  return new ASTPrinter(FilterString);
}
