//==--- DiagnosticDriverKinds.td - driver diagnostics ---------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

let Component = "Driver" in {

def err_line_length_value : Error<
  "'%0': invalid value '%1',"
  " should be a non-negative integer or 'none'">;
def err_line_length_size : Error<
  "'%0' option: value '%1' is too big">;

}
