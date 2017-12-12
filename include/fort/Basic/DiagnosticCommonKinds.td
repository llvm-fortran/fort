//==--- DiagnosticCommonKinds.td - common diagnostics ---------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Common Helpers
//===----------------------------------------------------------------------===//

let Component = "Common" in {

// Basic.

def fatal_too_many_errors
  : Error<"too many errors emitted, stopping now">, DefaultFatal;

def note_declared_at : Note<"declared here">;
def note_stmt_label_declared_at : Note<"statement label was declared here">;
def note_previous_definition : Note<"previous definition is here">;
def note_previous_declaration : Note<"previous declaration is here">;
def note_previous_definition_kind : Note<"%0 is %select{an argument|a parameter constant}1 defined here">;
def note_previous_implicit_declaration : Note<
  "previous implicit declaration is here">;
def note_previous_use : Note<"previous use is here">;
def note_duplicate_case_prev : Note<"previous case defined here">;
def note_forward_declaration : Note<"forward declaration of %0">;
def note_type_being_defined : Note<
  "definition of %0 is not complete until the closing '}'">;
/// note_matching - this is used as a continuation of a previous diagnostic,
/// e.g. to specify the '(' when we expected a ')'.
def note_matching : Note<"to match this '%0'">;
def note_matching_ident : Note<"to match this %0">;

def note_using : Note<"using">;
def note_possibility : Note<"one possibility">;
def note_also_found : Note<"also found">;

// Source manager
def err_cannot_open_file : Error<"cannot open file '%0': %1">, DefaultFatal;
def err_file_modified : Error<
  "file '%0' modified since it was first processed">, DefaultFatal;

// Unsupported statement
def err_unsupported_stmt : Error<"unsupported statement">;

// Targets

def err_target_unknown_triple : Error<
  "unknown target triple '%0', please use -triple or -arch">;
def err_target_unknown_cpu : Error<"unknown target CPU '%0'">;
def err_target_unknown_abi : Error<"unknown target ABI '%0'">;
def err_target_unsupported_abi : Error<"ABI '%0' is not supported on CPU '%1'">;
def err_target_unsupported_abi_for_triple : Error<
  "ABI '%0' is not supported for '%1'">;
def err_target_unknown_fpmath : Error<"unknown FP unit '%0'">;
def err_target_unsupported_fpmath : Error<
    "the '%0' unit is not supported with this instruction set">;
def err_target_unsupported_unaligned : Error<
  "the %0 sub-architecture does not support unaligned accesses">;
def err_target_unsupported_execute_only : Error<
  "execute only is not supported for the %0 sub-architecture">;
def err_opt_not_valid_with_opt : Error<
  "option '%0' cannot be specified with '%1'">;

}
