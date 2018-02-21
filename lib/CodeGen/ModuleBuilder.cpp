//===--- ModuleBuilder.cpp - Emit LLVM Code from ASTs ---------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This builds an AST and converts it to LLVM Code.
//
//===----------------------------------------------------------------------===//

#include "fort/CodeGen/ModuleBuilder.h"
#include "CodeGenModule.h"
#include "fort/AST/ASTContext.h"
#include "fort/AST/Decl.h"
#include "fort/AST/Expr.h"
#include "fort/Basic/Diagnostic.h"
#include "fort/Basic/TargetInfo.h"
#include "fort/Frontend/CodeGenOptions.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
using namespace fort;

namespace {

using std::unique_ptr;

class CodeGeneratorImpl : public CodeGenerator {
  DiagnosticsEngine &Diags;
  std::unique_ptr<const llvm::DataLayout> TD;
  ASTContext *Ctx;
  const CodeGenOptions CodeGenOpts; // Intentionally copied in.
  const TargetOptions Target;

protected:
  std::unique_ptr<llvm::Module> M;
  std::unique_ptr<CodeGen::CodeGenModule> Builder;

public:
  CodeGeneratorImpl(DiagnosticsEngine &diags, const std::string &ModuleName,
                    const CodeGenOptions &CGO, const TargetOptions &TO,
                    llvm::LLVMContext &C)
      : Diags(diags), CodeGenOpts(CGO), Target(TO),
        M(new llvm::Module(ModuleName, C)) {}

  virtual ~CodeGeneratorImpl() {}

  virtual llvm::Module *GetModule() { return M.get(); }

  virtual llvm::Module *ReleaseModule() { return M.release(); }

  virtual void Initialize(ASTContext &Context) {
    Ctx = &Context;

    M->setTargetTriple(Ctx->getTargetInfo().getTriple().getTriple());
    M->setDataLayout(Ctx->getTargetInfo().getDataLayout());
    TD.reset(new llvm::DataLayout(M.get()));

    Builder.reset(
        new CodeGen::CodeGenModule(Context, CodeGenOpts, *M, *TD, Diags));
  }

  virtual void HandleTranslationUnit(ASTContext &Ctx) {
    if (Diags.hadErrors()) {
      M.reset();
      return;
    }

    auto TranslationUnit = Ctx.getTranslationUnitDecl();
    auto I = TranslationUnit->decls_begin();
    for (auto E = TranslationUnit->decls_end(); I != E; ++I) {
      if ((*I)->getDeclContext() == TranslationUnit)
        Builder->EmitTopLevelDecl(*I);
    }

    if (Builder)
      Builder->Release();
  }
};
} // namespace

void CodeGenerator::anchor() {}

CodeGenerator *fort::CreateLLVMCodeGen(DiagnosticsEngine &Diags,
                                       const std::string &ModuleName,
                                       const CodeGenOptions &CGO,
                                       const TargetOptions &TO,
                                       llvm::LLVMContext &C) {
  return new CodeGeneratorImpl(Diags, ModuleName, CGO, TO, C);
}
