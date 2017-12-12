//===-- FrontendAction.h - Generic Frontend Action Interface ----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief Defines the flang::FrontendAction interface and various convenience
/// abstract classes (flang::ASTFrontendAction, flang::PluginASTAction,
/// flang::PreprocessorFrontendAction, and flang::WrapperFrontendAction)
/// derived from it.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_FLANG_FRONTEND_FRONTENDACTION_H
#define LLVM_FLANG_FRONTEND_FRONTENDACTION_H

#include "fort/Basic/LLVM.h"
#include "fort/Basic/LangOptions.h"
#include "fort/Frontend/ASTUnit.h"
#include "fort/Frontend/FrontendOptions.h"
#include "llvm/ADT/StringRef.h"
#include <string>
#include <vector>

namespace flang {
class ASTConsumer;
class CompilerInstance;

/// Abstract base class for actions which can be performed by the frontend.
class FrontendAction {
  std::unique_ptr<ASTUnit> CurrentASTUnit;
  CompilerInstance *Instance;

private:
  ASTConsumer* CreateWrappedASTConsumer(CompilerInstance &CI,
                                        StringRef InFile);

protected:
  /// @name Implementation Action Interface
  /// @{

  /// \brief Create the AST consumer object for this action, if supported.
  ///
  /// This routine is called as part of BeginSourceFile(), which will
  /// fail if the AST consumer cannot be created. This will not be called if the
  /// action has indicated that it only uses the preprocessor.
  ///
  /// \param CI - The current compiler instance, provided as a convenience, see
  /// getCompilerInstance().
  ///
  /// \param InFile - The current input file, provided as a convenience, see
  /// getCurrentFile().
  ///
  /// \return The new AST consumer, or null on failure.
  virtual ASTConsumer *CreateASTConsumer(CompilerInstance &CI,
                                         StringRef InFile) = 0;

  /// \brief Callback before starting processing a single input, giving the
  /// opportunity to modify the CompilerInvocation or do some other action
  /// before BeginSourceFileAction is called.
  ///
  /// \return True on success; on failure BeginSourceFileAction(),
  /// ExecuteAction() and EndSourceFileAction() will not be called.
  virtual bool BeginInvocation(CompilerInstance &CI) { return true; }

  /// \brief Callback at the start of processing a single input.
  ///
  /// \return True on success; on failure ExecutionAction() and
  /// EndSourceFileAction() will not be called.
  virtual bool BeginSourceFileAction(CompilerInstance &CI,
                                     StringRef Filename) {
    return true;
  }

  /// \brief Callback to run the program action, using the initialized
  /// compiler instance.
  ///
  /// This is guaranteed to only be called between BeginSourceFileAction()
  /// and EndSourceFileAction().
  virtual void ExecuteAction() = 0;

  /// \brief Callback at the end of processing a single input.
  ///
  /// This is guaranteed to only be called following a successful call to
  /// BeginSourceFileAction (and BeginSourceFile).
  virtual void EndSourceFileAction() {}

  /// \brief Callback at the end of processing a single input, to determine
  /// if the output files should be erased or not.
  ///
  /// By default it returns true if a compiler error occurred.
  /// This is guaranteed to only be called following a successful call to
  /// BeginSourceFileAction (and BeginSourceFile).
  virtual bool shouldEraseOutputFiles();

  /// @}

public:
  FrontendAction();
  virtual ~FrontendAction();

  /// @name Compiler Instance Access
  /// @{

  CompilerInstance &getCompilerInstance() const {
    assert(Instance && "Compiler instance not registered!");
    return *Instance;
  }

  void setCompilerInstance(CompilerInstance *Value) { Instance = Value; }

  /// @}
  /// @name Current File Information
  /// @{

  bool isCurrentFileAST() const {
    assert(CurrentASTUnit && "No current AST unit!");
    return CurrentASTUnit.get() != nullptr;
  }

  const StringRef getCurrentFile() const {
    CurrentASTUnit.get()->getOriginalSourceFileName();
  }

  InputKind getCurrentFileKind() const {
    return IK_None;
  }

  ASTUnit &getCurrentASTUnit() const {
    assert(CurrentASTUnit && "No current AST unit!");
    return *CurrentASTUnit;
  }

  ASTUnit *takeCurrentASTUnit() {
    return CurrentASTUnit.release();
  }

  /// @}
  /// @name Public Action Interface
  /// @{

  /// \brief Prepare the action for processing the input file \p Input.
  ///
  /// This is run after the options and frontend have been initialized,
  /// but prior to executing any per-file processing.
  ///
  /// \param CI - The compiler instance this action is being run from. The
  /// action may store and use this object up until the matching EndSourceFile
  /// action.
  ///
  /// \param Input - The input filename and kind. Some input kinds are handled
  /// specially, for example AST inputs, since the AST file itself contains
  /// several objects which would normally be owned by the
  /// CompilerInstance. When processing AST input files, these objects should
  /// generally not be initialized in the CompilerInstance -- they will
  /// automatically be shared with the AST file in between
  /// BeginSourceFile() and EndSourceFile().
  ///
  /// \return True on success; on failure the compilation of this file should
  /// be aborted and neither Execute() nor EndSourceFile() should be called.
  bool BeginSourceFile(CompilerInstance &CI, const FrontendInputFile &Input);

  /// \brief Set the source manager's main input file, and run the action.
  bool Execute();

  /// \brief Perform any per-file post processing, deallocate per-file
  /// objects, and run statistics and output file cleanup code.
  void EndSourceFile();

  /// @}
};

/// \brief Abstract base class to use for AST consumer-based frontend actions.
class ASTFrontendAction : public FrontendAction {
protected:
  /// \brief Implement the ExecuteAction interface by running Sema on
  /// the already-initialized AST consumer.
  ///
  /// This will also take care of instantiating a code completion consumer if
  /// the user requested it and the action supports it.
  virtual void ExecuteAction();

};

}  // end namespace flang

#endif
