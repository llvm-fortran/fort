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

#include <string>
#include "fort/Frontend/ASTConsumers.h"
#include "fort/AST/ASTContext.h"
#include "fort/AST/ASTConsumer.h"

using namespace flang;

namespace {

class ASTPrinter : public ASTConsumer {
public:
  std::string FilterString;

  ASTPrinter(StringRef Filter)
    : FilterString(Filter) {
  }

  void HandleTranslationUnit(ASTContext &Ctx) {
    Ctx.getTranslationUnitDecl()->dump();
  }
};

}

ASTConsumer *flang::CreateASTDumper(StringRef FilterString) {
  return new ASTPrinter(FilterString);
}

