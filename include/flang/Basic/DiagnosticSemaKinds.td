//==--- DiagnosticSemaKinds.td - libsema diagnostics ----------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Semantic Analysis
//===----------------------------------------------------------------------===//

let Component = "Sema" in {
let CategoryName = "Semantic Issue" in {

def err_expected_integer_expr : Error<
  "expected an integer expression">;

def err_typecheck_assign_incompatible : Error<
  "assigning to '%0' from incompatible type '%1'">;

def err_typecheck_arith_invalid_operands : Error<
  "invalid operands to an arithmetic binary expression ('%0' and '%1')">;
def err_typecheck_char_invalid_operands : Error<
  "invalid operands to a character binary expression ('%0' and '%1')">;
def err_typecheck_relational_invalid_operands : Error<
  "invalid operands to a relational binary expression ('%0' and '%1')">;
def err_typecheck_logical_invalid_operands : Error<
  "invalid operands to a logical binary expression ('%0' and '%1')">;

def err_typecheck_arith_unary_expr : Error<
  "invalid argument type '%0' to an arithmetic unary expression">;
def err_typecheck_logical_unary_expr : Error<
  "invalid argument type '%0' to a logical unary expression">;

def err_typecheck_expected_logical_expr : Error<
  "expected a logical expression instead of an expression with type '%0'">;
def err_typecheck_expected_do_var : Error<
  "expected a real or an integer variable instead of variable with type '%0'">;
def err_typecheck_expected_do_expr : Error<
  "expected a real or an integer expression">;

def err_array_subscript_dimension_count_mismatch : Error<
  "array subscript must have %0 subscript %plural{1:expression|:expressions}0">;

def warn_array_index_precedes_bounds : Warning<
  "array index %0 is before the beginning of the array">,
  InGroup<ArrayBounds>;
def warn_array_index_exceeds_bounds : Warning<
  "array index %0 is past the end of the array (which contains %1 "
  "element%s2)">, InGroup<ArrayBounds>;
def note_array_index_out_of_bounds : Note<
  "array %0 declared here">;

def err_stmt_label_must_decl_after : Error<
  "the statement label '%0' must be declared after the '%1' statement">;
def err_redefinition_of_stmt_label : Error<"redefinition of statement label '%0'">;
def err_undeclared_stmt_label_use : Error<"use of undeclared statement label '%0'">;
def warn_unused_stmt_label : Warning<"unused statement label '%0'">,
  InGroup<UnusedLabel>, DefaultIgnore;

def err_invalid_do_terminating_stmt : Error<
  "invalid terminating statement for a DO loop">;

}

}
