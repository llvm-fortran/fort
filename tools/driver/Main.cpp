//===-- Main.cpp - LLVM-based Fortran Compiler ----------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// The Fortran Compiler.
//
//===----------------------------------------------------------------------===//

#include "fort/AST/ASTConsumer.h"
#include "fort/Basic/Diagnostic.h"
#include "fort/Basic/TargetInfo.h"
#include "fort/Basic/Version.h"
#include "fort/CodeGen/BackendUtil.h"
#include "fort/CodeGen/ModuleBuilder.h"
#include "fort/Driver/DriverDiagnostic.h"
#include "fort/Driver/Options.h"
#include "fort/Frontend/ASTConsumers.h"
#include "fort/Frontend/TextDiagnosticPrinter.h"
#include "fort/Frontend/VerifyDiagnosticConsumer.h"
#include "fort/Parse/Parser.h"
#include "fort/Sema/Sema.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/Bitcode/BitcodeWriterPass.h"
#include "llvm/CodeGen/RegAllocRegistry.h"
#include "llvm/CodeGen/SchedulerRegistry.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/IRPrintingPasses.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/MC/SubtargetFeature.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Option/OptTable.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/Timer.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Instrumentation.h"
#include "llvm/Transforms/ObjCARC.h"
#include "llvm/Transforms/Utils.h"
#include "llvm/Transforms/Utils/SymbolRewriter.h"
#include <memory>

using namespace llvm;
using namespace llvm::opt;
using namespace fort;
using namespace fort::driver;

//===----------------------------------------------------------------------===//
// Command line options.
//===----------------------------------------------------------------------===//

static void PrintVersion(raw_ostream &OS) {
  OS << getFortFullVersion() << '\n';
}

std::string GetOutputName(StringRef Filename, BackendAction Action) {
  llvm::SmallString<256> Path(Filename.begin(), Filename.end());
  switch (Action) {
  case Backend_EmitObj:
    llvm::sys::path::replace_extension(Path, ".o");
    break;
  case Backend_EmitAssembly:
    llvm::sys::path::replace_extension(Path, ".s");
    break;
  case Backend_EmitBC:
    llvm::sys::path::replace_extension(Path, ".bc");
    break;
  case Backend_EmitLL:
    llvm::sys::path::replace_extension(Path, ".ll");
    break;
  default:
    assert(false && "No output name for action");
    break;
  }
  return std::string(Path.begin(), Path.size());
}

static bool EmitFile(llvm::raw_pwrite_stream &Out, llvm::Module *Module,
                     llvm::TargetMachine *TM, BackendAction Action) {
  // write instructions to file
  if (Action == Backend_EmitObj || Action == Backend_EmitAssembly) {
    llvm::Module &Mod = *Module;

    llvm::TargetMachine::CodeGenFileType CGFT =
        llvm::TargetMachine::CGFT_AssemblyFile;

    if (Action == Backend_EmitObj)
      CGFT = llvm::TargetMachine::CGFT_ObjectFile;
    else if (Action == Backend_EmitMCNull)
      CGFT = llvm::TargetMachine::CGFT_Null;
    else
      assert(Action == Backend_EmitAssembly && "Invalid action!");

    llvm::legacy::PassManager PM;

    // Target.setAsmVerbosityDefault(true);
    // Target.setMCRelaxAll(true);
    llvm::formatted_raw_ostream FOS(Out);

    // FIXME : add the backend passes
    // Ask the target to add backend passes as necessary.
    // if (Target.addPassesToEmitFile(PM, FOS, FileType, true)) {
    //  return true;
    //}
    if (TM->addPassesToEmitFile(PM, Out, CGFT, true, nullptr)) {
      return false;
    }

    PM.run(Mod);
    return true;
  } else if (Action == Backend_EmitBC) {
    llvm::WriteBitcodeToFile(*Module, Out);
    return true;
  } else if (Action == Backend_EmitLL) {
    Module->print(Out, nullptr);
    return true;
  }

  return false;
}

static bool EmitOutputFile(const std::string &Input, llvm::Module *Module,
                           llvm::TargetMachine *TM, BackendAction Action) {
  std::error_code err;
  llvm::raw_fd_ostream Out(Input.c_str(), err, llvm::sys::fs::F_None);
  if (err) {
    llvm::errs() << "Could not open output file '" << Input
                 << "': " << err.message() << "\n";
    return true;
  }
  return EmitFile(Out, Module, TM, Action);
}

static bool LinkFiles(ArrayRef<std::string> OutputFiles,
                      ArrayRef<std::string> LinkLibraries,
                      ArrayRef<std::string> LinkDirectories,
                      const std::string &Output) {
  const char *Driver = "gcc";
  std::string Cmd;
  llvm::raw_string_ostream OS(Cmd);
  OS << Driver;
  for (const std::string &I : OutputFiles)
    OS << " " << I;
  for (const std::string &I : LinkDirectories)
    OS << " -L " << I;
  OS << " -l libfort";
  for (const std::string &I : LinkLibraries)
    OS << " -l " << I;
  // Link with the math library.
  OS << " -l m";
  if (Output.size())
    OS << " -o " << Output;
  Cmd = OS.str();
  return system(Cmd.c_str());
}

/// Type of output from single input
enum OutputType { Object, Assembly, LLVM };

static bool ParseFile(const std::string &Filename,
                      const std::vector<std::string> &IncludeDirs,
                      const OutputType OutType, const std::string &OutName,
                      InputArgList &Args,
                      SmallVectorImpl<std::string> &OutputFiles) {
  ErrorOr<std::unique_ptr<MemoryBuffer>> MBOrErr =
      MemoryBuffer::getFileOrSTDIN(Filename);
  if (std::error_code EC = MBOrErr.getError()) {
    llvm::errs() << "Could not open input file '" << Filename
                 << "': " << EC.message() << "\n";
    return true;
  }
  std::unique_ptr<llvm::MemoryBuffer> MB = std::move(MBOrErr.get());

  // Record the location of the include directory so that the lexer can find it
  // later.
  SourceMgr SrcMgr;
  SrcMgr.setIncludeDirs(IncludeDirs);

  // Tell SrcMgr about this buffer, which is what Parser will pick up.
  SrcMgr.AddNewSourceBuffer(std::move(MB), llvm::SMLoc());

  LangOptions Opts;

  if (Args.hasArg(options::OPT_default_real_8)) {
    Opts.DefaultReal8 = true;
    for (auto A : Args.filtered(options::OPT_default_real_8))
      A->claim();
  }

  if (Args.hasArg(options::OPT_default_double_8)) {
    Opts.DefaultDouble8 = true;
    for (auto A : Args.filtered(options::OPT_default_double_8))
      A->claim();
  }

  if (Args.hasArg(options::OPT_default_integer_8)) {
    Opts.DefaultInt8 = true;
    for (auto A : Args.filtered(options::OPT_default_integer_8))
      A->claim();
  }

  if (Args.hasArg(options::OPT_C)) {
    Opts.ReturnComments = true;
    for (auto A : Args.filtered(options::OPT_C))
      A->claim();
  }

  llvm::StringRef Ext = llvm::sys::path::extension(Filename);
  if (!Args.hasArg(options::OPT_free_form, options::OPT_fixed_form)) {
    if (Ext.equals_lower(".f")) {
      Opts.FixedForm = 1;
      Opts.FreeForm = 0;
      Opts.LineLength = 72;
    }
  } else {
    auto A = Args.getLastArg(options::OPT_free_form, options::OPT_fixed_form);
    if (A->getOption().matches(options::OPT_fixed_form)) {
      Opts.FixedForm = 1;
      Opts.FreeForm = 0;
      Opts.LineLength = 72;
    }

    for (auto A :
         Args.filtered(options::OPT_free_form, options::OPT_fixed_form))
      A->claim();
  }

  if (Args.hasArg(options::OPT_Fortran77) || Ext.equals_lower(".f77")) {
    Opts.Fortran77 = 1;
    for (auto A : Args.filtered(options::OPT_Fortran77)) {
      A->claim();
    }
  }

  // Error printing
  TextDiagnosticPrinter TDP(SrcMgr);
  DiagnosticsEngine Diag(new DiagnosticIDs, &SrcMgr, &TDP, false);

  // Process line length arguments
  if (Args.hasArg(options::OPT_free_line_length,
                  options::OPT_fixed_line_length)) {
    // Validate all
    for (Arg *L : Args.filtered(options::OPT_free_line_length,
                                options::OPT_fixed_line_length)) {
      const char *Length = L->getValue();
      bool IsFreeForm = L->getOption().matches(options::OPT_free_line_length) &&
                        Opts.FreeForm;
      bool IsFixedForm =
          L->getOption().matches(options::OPT_fixed_line_length) &&
          Opts.FixedForm;
      L->claim();

      if (!strcmp(Length, "none")) {
        Opts.LineLength = 0;
      } else {
        char *rest;
        unsigned long val = strtoul(Length, &rest, 10);

        if (*rest != '\0') {
          Diag.Report(diag::err_line_length_value)
              << L->getAsString(Args) << std::string(Length);
          return true;
        }
        if (val > std::numeric_limits<unsigned>::max()) {
          // FIXME need a proper diagnostic, like such:
          // fort: for the -ffixed-line-length- option: 1parrot value invalid
          Diag.Report(diag::err_line_length_size)
              << L->getOption().getPrefixedName() << std::string(Length);
          return true;
        }

        if (IsFreeForm || IsFixedForm)
          Opts.LineLength = val;
      }
    }
  }

  bool SyntaxOnly = Args.hasArg(options::OPT_fsyntax_only);
  if (SyntaxOnly) {
    for (auto A : Args.filtered(options::OPT_fsyntax_only)) {
      A->claim();
    }
  }

  // Chain in -verify checker, if requested.
  if (Args.hasArg(options::OPT_verify)) {
    Diag.setClient(new VerifyDiagnosticConsumer(Diag));
    for (auto A : Args.filtered(options::OPT_verify))
      A->claim();
  }

  ASTContext Context(SrcMgr, Opts);
  Sema SA(Context, Diag);
  Parser P(SrcMgr, Opts, Diag, SA);
  Diag.getClient()->BeginSourceFile(Opts, &P.getLexer());
  P.ParseProgramUnits();
  Diag.getClient()->EndSourceFile();

  // Dump
  if (Args.hasArg(options::OPT_ast_print, options::OPT_ast_dump)) {
    for (auto A :
         Args.filtered(options::OPT_ast_print, options::OPT_ast_dump)) {
      A->claim();
    }
    auto Dumper = CreateASTDumper("");
    Dumper->HandleTranslationUnit(Context);
    delete Dumper;
  }

  // Emit
  if (!SyntaxOnly && !Diag.hadErrors()) {
    std::shared_ptr<fort::TargetOptions> TargetOptions =
        std::make_shared<fort::TargetOptions>();

    if (Arg *T = Args.getLastArg(options::OPT_triple)) {
      TargetOptions->Triple = T->getValue();
      for (auto A : Args.filtered(options::OPT_triple))
        A->claim();
    } else {
      TargetOptions->Triple = llvm::sys::getDefaultTargetTriple();
    }

    TargetOptions->CPU = llvm::sys::getHostCPUName();
    std::shared_ptr<LLVMContext> LLContext(new LLVMContext);

    // FIXME data layout is not getting set in the AST context

    auto CG = CreateLLVMCodeGen(
        Diag, Filename == "" ? std::string("module") : Filename,
        CodeGenOptions(), *TargetOptions, *LLContext);
    std::unique_ptr<fort::TargetInfo> TI(
        TargetInfo::CreateTargetInfo(Diag, TargetOptions));
    Context.setTargetInfo(*TI);
    CG->Initialize(Context);
    CG->HandleTranslationUnit(Context);

    BackendAction BA;
    switch (OutType) {
    case Assembly:
      BA = Backend_EmitAssembly;
      break;
    case LLVM:
      BA = Backend_EmitLL;
      break;
    default:
      BA = Backend_EmitObj;
      break;
    }

    const llvm::Target *TheTarget = 0;
    std::string Err;
    TheTarget = llvm::TargetRegistry::lookupTarget(TargetOptions->Triple, Err);

    unsigned OptLevel = 0;
    CodeGenOpt::Level TMOptLevel = CodeGenOpt::Default;
    // TODO legal values for -O flags
    if (Arg *A = Args.getLastArg(options::OPT_O_Group)) {
      if (A->getOption().matches(options::OPT_O0)) {
        TMOptLevel = CodeGenOpt::None;
      } else if (A->getOption().matches(options::OPT_O)) {
        StringRef S(A->getValue());
        OptLevel = 2;
        // TODO -Os, -Oz, and others are ingnored
        if (S.getAsInteger(10, OptLevel) && OptLevel > 2)
          TMOptLevel = CodeGenOpt::Aggressive;
      }
    }

    llvm::TargetOptions Options;

    auto TM = TheTarget->createTargetMachine(
        TargetOptions->Triple, TargetOptions->CPU, "", Options, Reloc::Static,
        llvm::None, TMOptLevel);

    if (!(OutType == LLVM && OptLevel == 0)) {
      auto TheModule = CG->GetModule();
      auto PM = new llvm::legacy::PassManager();
      // llvm::legacy::FunctionPassManager *FPM = new
      // llvm::legacy::FunctionPassManager(TheModule); FPM->add(new
      // DataLayoutPass()); PM->add(new llvm::DataLayoutPass());
      // TM->addAnalysisPasses(*PM);
      PM->add(createPromoteMemoryToRegisterPass());

      PassManagerBuilder PMBuilder;
      PMBuilder.OptLevel = OptLevel;
      PMBuilder.SizeLevel = 0;
      PMBuilder.LoopVectorize = true;
      PMBuilder.SLPVectorize = true;
      unsigned Threshold = 225;
      if (OptLevel > 2)
        Threshold = 275;
      PMBuilder.Inliner = createFunctionInliningPass(Threshold);

      PMBuilder.populateModulePassManager(*PM);
      // llvm::legacy::PassManager *MPM = new llvm::legacy::PassManager();
      // PMBuilder.populateModulePassManager(*MPM);

      PM->run(*TheModule);
      // MPM->run(*TheModule);
      delete PM;
      // delete MPM;
    }

    if (OutName == "-") {
      OutputFiles.push_back(OutName);
    } else {
      OutputFiles.push_back(GetOutputName(Filename, BA));
    }

    EmitOutputFile(OutputFiles.back(), CG->GetModule(), TM, BA);

    delete CG;
  }

  return Diag.hadErrors();
}

int main(int argc_, char **argv_) {
  sys::PrintStackTraceOnErrorSignal(llvm::StringRef(argv_[0]));
  PrettyStackTraceProgram X(argc_, argv_);
  cl::SetVersionPrinter(PrintVersion);

  SmallVector<std::string, 32> InputFiles;
  // TODO hash out errors
  auto OptTable = createDriverOptTable();
  SmallVector<const char *, 256> argv(argv_, argv_ + argc_);

  ArrayRef<const char *> argvRef = argv;
  unsigned MissingArgIndex, MissingArgCount;
  InputArgList Args =
      OptTable->ParseArgs(argvRef.slice(1), MissingArgIndex, MissingArgCount);
  for (auto A : Args) {
    if (A->getOption().getKind() == Option::InputClass)
      InputFiles.push_back(A->getValue());
  }

  // FIXME seemingly dead option
  bool CanonicalPrefixes = !Args.hasArg(options::OPT_no_canonical_prefixes);
  if (!CanonicalPrefixes)
    for (auto A : Args.filtered(options::OPT_no_canonical_prefixes)) {
      A->claim();
    }

  // Output of compilation for each input
  OutputType OutType = Object;
  // -S or --assemble implies assembly output
  if (Args.hasArg(options::OPT_S)) {
    OutType = Assembly;
    for (auto A : Args.filtered(options::OPT_S)) {
      A->claim();
    }
  }
  // But it can be overriden by -emit-llvm
  if (Args.hasArg(options::OPT_emit_llvm)) {
    OutType = LLVM;
    for (auto A : Args.filtered(options::OPT_emit_llvm)) {
      A->claim();
    }
  }

  // Set output name
  std::string OutName;
  if (Args.hasArg(options::OPT_o)) {
    OutName = Args.getLastArg(options::OPT_o)->getValue();
    for (auto A : Args.filtered(options::OPT_o))
      A->claim();
  }

  // Collect include directories
  std::vector<std::string> IncludeDirs;
  for (Arg *A : Args.filtered(options::OPT_I)) {
    IncludeDirs.push_back(A->getValue());
  }

  bool CompileOnly = Args.hasArg(options::OPT_c);
  if (CompileOnly)
    for (auto A : Args.filtered(options::OPT_c))
      A->claim();

  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmPrinters();
  llvm::InitializeAllAsmParsers();

  // Parse input files
  bool HadErrors = false;
  SmallVector<std::string, 32> OutputFiles;
  OutputFiles.reserve(1);

  if (InputFiles.empty())
    InputFiles.push_back("-");
  for (auto I : InputFiles) {
    llvm::StringRef Ext = llvm::sys::path::extension(I);
    if (Ext.equals_lower(".o") || Ext.equals_lower(".obj") ||
        Ext.equals_lower(".a") || Ext.equals_lower(".lib"))
      OutputFiles.push_back(I);
    else if (ParseFile(I, IncludeDirs, OutType, OutName, Args, OutputFiles))
      HadErrors = true;
  }

  SmallVector<std::string, 32> LinkDirectories;
  for (Arg *A : Args.filtered(options::OPT_L)) {
    LinkDirectories.push_back(A->getValue());
  }

  SmallVector<std::string, 32> LinkLibraries;
  for (Arg *A : Args.filtered(options::OPT_l)) {
    LinkLibraries.push_back(A->getValue());
  }

  if (OutputFiles.size() && !HadErrors && !CompileOnly && OutType == Object)
    LinkFiles(OutputFiles, LinkLibraries, LinkDirectories, OutName);

  // If any timers were active but haven't been destroyed yet, print their
  // results now. This happens in -disable-free mode.
  llvm::TimerGroup::printAll(llvm::errs());

  llvm::llvm_shutdown();
  return HadErrors;
}
