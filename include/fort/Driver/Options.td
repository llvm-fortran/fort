//===--- Options.td - Options for fort ------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the options accepted by fort.
//
//===----------------------------------------------------------------------===//

// Include the common option parsing interfaces.
include "llvm/Option/OptParser.td"

/////////
// Flags

// DriverOption - The option is a "driver" option, and should not be forwarded
// to other tools.
def DriverOption : OptionFlag;

// LinkerInput - The option is a linker input.
def LinkerInput : OptionFlag;

// NoArgumentUnused - Don't report argument unused warnings for this option; this
// is useful for options like -static or -dynamic which a user may always end up
// passing, even if the platform defaults to (or only supports) that option.
def NoArgumentUnused : OptionFlag;

// Unsupported - The option is unsupported, and the driver will reject command
// lines that use it.
def Unsupported : OptionFlag;

// Ignored - The option is unsupported, and the driver will silently ignore it.
def Ignored : OptionFlag;

// FC1Option - This option should be accepted by fort -fc1.
def FC1Option : OptionFlag;

// FC1AsOption - This option should be accepted by fort -fc1as.
def FC1AsOption : OptionFlag;

// NoDriverOption - This option should not be accepted by the driver.
def NoDriverOption : OptionFlag;

// A short name to show in documentation. The name will be interpreted as rST.
class DocName<string name> { string DocName = name; }

// A brief description to show in documentation, interpreted as rST.
class DocBrief<code descr> { code DocBrief = descr; }

// Indicates that this group should be flattened into its parent when generating
// documentation.
class DocFlatten { bit DocFlatten = 1; }

// Indicates that this warning is ignored, but accepted with a warning for
// GCC compatibility.
class IgnoredGCCCompat : Flags<[HelpHidden]> {}

/////////
// Groups

def Action_Group : OptionGroup<"<action group>">, DocName<"Actions">,
                   DocBrief<[{The action to perform on the input.}]>;

// Meta-group for options which are only used for compilation,
// and not linking etc.
def CompileOnly_Group : OptionGroup<"<CompileOnly group>">,
                        DocName<"Compilation flags">, DocBrief<[{
Flags controlling the behavior of Fort during compilation. These flags have
no effect during actions that do not perform compilation.}]>;

def Preprocessor_Group : OptionGroup<"<Preprocessor group>">,
                         Group<CompileOnly_Group>,
                         DocName<"Preprocessor flags">, DocBrief<[{
Flags controlling the behavior of the Fort preprocessor.}]>;

def IncludePath_Group : OptionGroup<"<I/i group>">, Group<Preprocessor_Group>,
                        DocName<"Include path management">,
                        DocBrief<[{
Flags controlling how ``#include``\s are resolved to files.}]>;

def I_Group : OptionGroup<"<I group>">, Group<IncludePath_Group>, DocFlatten;
def i_Group : OptionGroup<"<i group>">, Group<IncludePath_Group>, DocFlatten;
def fort_i_Group : OptionGroup<"<fort i group>">, Group<i_Group>, DocFlatten;

def M_Group : OptionGroup<"<M group>">, Group<Preprocessor_Group>,
              DocName<"Dependency file generation">, DocBrief<[{
Flags controlling generation of a dependency file for ``make``-like build
systems.}]>;

def d_Group : OptionGroup<"<d group>">, Group<Preprocessor_Group>,
              DocName<"Dumping preprocessor state">, DocBrief<[{
Flags allowing the state of the preprocessor to be dumped in various ways.}]>;

def Diag_Group : OptionGroup<"<W/R group>">, Group<CompileOnly_Group>,
                 DocName<"Diagnostic flags">, DocBrief<[{
Flags controlling which warnings, errors, and remarks Fort will generate.
See the :doc:`full list of warning and remark flags <DiagnosticsReference>`.}]>;

def R_Group : OptionGroup<"<R group>">, Group<Diag_Group>, DocFlatten;
def R_value_Group : OptionGroup<"<R (with value) group>">, Group<R_Group>,
                    DocFlatten;
def W_Group : OptionGroup<"<W group>">, Group<Diag_Group>, DocFlatten;
def W_value_Group : OptionGroup<"<W (with value) group>">, Group<W_Group>,
                    DocFlatten;

def f_Group : OptionGroup<"<f group>">, Group<CompileOnly_Group>,
              DocName<"Target-independent compilation options">;

def f_fort_Group : OptionGroup<"<f (fort-only) group>">,
                    Group<CompileOnly_Group>, DocFlatten;
def pedantic_Group : OptionGroup<"<pedantic group>">, Group<f_Group>,
                     DocFlatten;
def opencl_Group : OptionGroup<"<opencl group>">, Group<f_Group>,
                   DocName<"OpenCL flags">;

def m_Group : OptionGroup<"<m group>">, Group<CompileOnly_Group>,
              DocName<"Target-dependent compilation options">;

def O_Group : OptionGroup<"<O group>">, Group<CompileOnly_Group>,
              DocName<"Optimization level">, DocBrief<[{
Flags controlling how much optimization should be performed.}]>;

def DebugInfo_Group : OptionGroup<"<g group>">, Group<CompileOnly_Group>,
                      DocName<"Debug information generation">, DocBrief<[{
Flags controlling how much and what kind of debug information should be
generated.}]>;

def g_Group : OptionGroup<"<g group>">, Group<DebugInfo_Group>,
              DocName<"Kind and level of debug information">;
def gN_Group : OptionGroup<"<gN group>">, Group<g_Group>,
               DocName<"Debug level">;
def ggdbN_Group : OptionGroup<"<ggdbN group>">, Group<gN_Group>, DocFlatten;
def gTune_Group : OptionGroup<"<gTune group>">, Group<g_Group>,
                  DocName<"Debugger to tune debug information for">;
def g_flags_Group : OptionGroup<"<g flags group>">, Group<DebugInfo_Group>,
                    DocName<"Debug information flags">;

def StaticAnalyzer_Group : OptionGroup<"<Static analyzer group>">,
                           DocName<"Static analyzer flags">, DocBrief<[{
Flags controlling the behavior of the Fort Static Analyzer.}]>;

// gfortran options that we recognize in the driver
def gfortran_Group : OptionGroup<"<gfortran group>">,
                     DocName<"GNU Fortran compilation flags">, DocBrief<[{
Flags available in ``gfortran`` that Fort recongizes.}]>;

def Link_Group : OptionGroup<"<T/e/s/t/u group>">, DocName<"Linker flags">,
                 DocBrief<[{Flags that are passed on to the linker}]>;
def T_Group : OptionGroup<"<T group>">, Group<Link_Group>, DocFlatten;
def u_Group : OptionGroup<"<u group>">, Group<Link_Group>, DocFlatten;

def reserved_lib_Group : OptionGroup<"<reserved libs group>">,
                         Flags<[Unsupported]>;

// Temporary groups for fort options which we know we don't support,
// but don't want to verbosely warn the user about.
def fort_ignored_f_Group : OptionGroup<"<fort ignored f group>">,
  Group<f_Group>, Flags<[Ignored]>;
def fort_ignored_m_Group : OptionGroup<"<fort ignored m group>">,
  Group<m_Group>, Flags<[Ignored]>;

// Group for fort options in the process of deprecation.
// Please include the version that deprecated the flag as comment to allow
// easier garbage collection.
def fort_ignored_legacy_options_Group : OptionGroup<"<fort legacy flags>">,
  Group<f_Group>, Flags<[Ignored]>;

// Group that ignores all gcc optimizations that won't be implemented
def fort_ignored_gcc_optimization_f_Group : OptionGroup<
  "<fort_ignored_gcc_optimization_f_Group>">, Group<f_Group>, Flags<[Ignored]>;

/////////
// Options

// The internal option ID must be a valid C++ identifier and results in a
// fort::driver::options::OPT_XX enum constant for XX.
//
// We want to unambiguously be able to refer to options from the driver source
// code, for this reason the option name is mangled into an ID. This mangling
// isn't guaranteed to have an inverse, but for practical purposes it does.
//
// The mangling scheme is to ignore the leading '-', and perform the following
// substitutions:
//   _ => __
//   - => _
//   / => _SLASH
//   # => _HASH
//   ? => _QUESTION
//   , => _COMMA
//   = => _EQ
//   C++ => CXX
//   . => _

// Developer Driver Options

def internal_Group : OptionGroup<"<fort internal options>">, Flags<[HelpHidden]>;
def internal_driver_Group : OptionGroup<"<fort driver internal options>">,
  Group<internal_Group>, HelpText<"DRIVER OPTIONS">;
def internal_debug_Group :
  OptionGroup<"<fort debug/development internal options>">,
  Group<internal_Group>, HelpText<"DEBUG/DEVELOPMENT OPTIONS">;

class InternalDriverOpt : Group<internal_driver_Group>,
  Flags<[DriverOption, HelpHidden]>;

// FIXME Implement driver mode and other internal driver options
// Example for dirver mode:
// def driver_mode : Joined<["--"], "driver-mode=">, Group<internal_driver_Group>,
//   Flags<[DriverOption, HelpHidden]>,
//   HelpText<"Set the driver mode to either 'gcc', 'g++', 'cpp', or 'cl'">;

class InternalDebugOpt : Group<internal_debug_Group>,
  Flags<[DriverOption, HelpHidden]>;

def gen_reproducer: Flag<["-"], "gen-reproducer">, InternalDebugOpt,
  HelpText<"Auto-generates preprocessed source files and a reproduction script">;

// TODO needed for anything?
def _migrate : Flag<["--"], "migrate">, Flags<[DriverOption]>,
  HelpText<"Run the migrator">;

// Standard Options

def _HASH_HASH_HASH : Flag<["-"], "###">, Flags<[DriverOption]>,
    HelpText<"Print (but do not run) the commands to run for this compilation">;
def _DASH_DASH : Option<["--"], "", KIND_REMAINING_ARGS>,
    Flags<[DriverOption]>;

// TODO Add Fort options here
//      Equivalents of commented out options are not present in this file

def ast_print : Flag<["-"], "ast-print">,
  HelpText<"Build ASTs and then pretty-print them">;
def ast_dump : Flag<["-"], "ast-dump">,
  HelpText<"Build ASTs and then debug dump them">;
def verify : Flag<["-"], "verify">,
  HelpText<"Run the verifier">;
def triple : Separate<["-"], "triple">,
  Flags<[FC1Option,FC1AsOption,NoDriverOption]>,
  HelpText<"Specify target triple (e.g. i686-apple-darwin9)">;


def I : JoinedOrSeparate<["-"], "I">, Group<I_Group>,
    Flags<[FC1Option,FC1AsOption]>, MetaVarName<"<dir>">,
    HelpText<"Add directory to include search path">;

def C : Flag<["-"], "C">, Flags<[FC1Option]>, Group<Preprocessor_Group>,
    HelpText<"Include comments in preprocessed output">;

def fsyntax_only : Flag<["-"], "fsyntax-only">,
  Flags<[DriverOption,FC1Option]>, Group<Action_Group>,
  HelpText<"Stop after checking syntax">;

def emit_llvm : Flag<["-"], "emit-llvm">, Flags<[FC1Option]>, Group<Action_Group>,
  HelpText<"Use the LLVM representation for assembler and object files">;

def S : Flag<["-"], "S">, Flags<[DriverOption,FC1Option]>, Group<Action_Group>,
  HelpText<"Emit assembly -- only run preprocess and compilation steps">;
def _assemble : Flag<["--"], "assemble">, Alias<S>;

def o : JoinedOrSeparate<["-"], "o">,
  Flags<[DriverOption, RenderAsInput, FC1Option, FC1AsOption]>,
  HelpText<"Write output to <file>">, MetaVarName<"<file>">;

def O0 : Flag<["-"], "O0">, Group<O_Group>, Flags<[FC1Option, HelpHidden]>;
def O4 : Flag<["-"], "O4">, Group<O_Group>, Flags<[FC1Option, HelpHidden]>;
def O : Joined<["-"], "O">, Group<O_Group>, Flags<[FC1Option]>;
def O_flag : Flag<["-"], "O">, Flags<[FC1Option]>, Alias<O>, AliasArgs<["2"]>;
def Ofast : Joined<["-"], "Ofast">, Group<O_Group>, Flags<[FC1Option]>;

def g_Flag : Flag<["-"], "g">, Group<g_Group>,
  HelpText<"Generate source-level debug information">;

def L : JoinedOrSeparate<["-"], "L">, Flags<[RenderJoined]>, Group<Link_Group>,
    MetaVarName<"<dir>">, HelpText<"Add directory to library search path">;

def l : JoinedOrSeparate<["-"], "l">, Flags<[LinkerInput, RenderJoined]>,
        Group<Link_Group>;

def c : Flag<["-"], "c">, Flags<[DriverOption]>, Group<Action_Group>,
  HelpText<"Compile only -- only run preprocess, compile, and assemble steps">;

def no_canonical_prefixes : Flag<["-"], "no-canonical-prefixes">, Flags<[HelpHidden]>,
  HelpText<"Use relative instead of canonical paths">;

def default_real_8 : Flag<["-"], "fdefault-real-8">, Flags<[FC1Option]>,
  HelpText<"set the kind of the default real type to 8">;

def default_double_8 : Flag<["-"], "fdefault-double-8">, Flags<[FC1Option]>,
  HelpText<"set the kind of the default double type to 8">;

def default_integer_8 : Flag<["-"], "fdefault-integer-8">, Flags<[FC1Option]>,
  HelpText<"set the kind of the default integer type to 8">;

def free_form : Flag<["-"], "ffree-form">, Flags<[FC1Option]>,
  HelpText<"the source files are using free form layout">;

def fixed_form : Flag<["-"], "ffixed-form">, Flags<[FC1Option]>,
  HelpText<"the source files are using fixed form layout">;

def Fortran77 : Flag<["-"], "f77">, Flags<[FC1Option]>,
  HelpText<"compile with Fortran77 features">;

def fixed_line_length : Joined<["-"], "ffixed-line-length-">,
  Flags<[FC1Option]>, HelpText<"maximum allowed line length in fixed form, "
                               "0 or 'none' to disable the limit">;

def free_line_length : Joined<["-"], "ffree-line-length-">,
  Flags<[FC1Option]>, HelpText<"maximum allowed line length in free form, "
                               "0 or 'none' to disable the limit">;

