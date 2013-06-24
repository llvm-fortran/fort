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
def err_typecheck_passing_incompatible : Error<
  "passing %0 to parameter of incompatible type %1">;

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

def err_expected_stmt_label_end_do : Error<
  "expected a statement with a statement label '%0' to mark the end of a do loop">;

def err_invalid_do_terminating_stmt : Error<
  "invalid terminating statement for a DO loop">;

def err_stmt_not_in_if : Error<
  "'%0' statement must be a part of an if construct">;
def err_end_do_without_do : Error<
  "use of 'END DO' without the do statement">;

def err_use_implicit_none_stmt : Error<
  "use of 'IMPLICIT NONE' after 'IMPLICIT'">;
def err_use_implicit_stmt_after_none : Error<
  "use of 'IMPLICIT' after 'IMPLICIT NONE'">;
def err_redefinition_of_implicit_stmt_rule : Error<
  "redefinition of implicit rule %0">;
def err_redefinition_of_implicit_stmt_rule_range : Error<
  "redefinition of implicit rule in the range %0 - %1">;
def err_implicit_invalid_range : Error<
  "the range %0 - %1 isn't alphabetically ordered">;

def err_parameter_requires_const_init : Error<
  "parameter %0 must be initialized by a constant expression">;
def note_parameter_value_invalid_expr : Note<
  "this expression is not allowed in a constant expression">;

def err_intrinsic_invalid_func : Error<
  "invalid function name %0 in an intrinsic statement">;

def err_expect_stmt_name : Error<
  "expected name %0 for '%1' statement">;

def err_redefinition : Error<"redefinition of %0">;
def err_undeclared_var_use : Error<"use of undeclared identifier %0">;

def err_typecheck_call_too_few_args : Error<
  "too few arguments to "
  "%select{intrinsic function|function|subprogram}0 call, "
  "expected %1, have %2">;
def err_typecheck_call_too_few_args_one : Error<
  "too few arguments to "
  "%select{intrinsic function|function|subprogram}0 call, "
  "single argument %1 was not specified">;
def err_typecheck_call_too_few_args_at_least : Error<
  "too few arguments to "
  "%select{intrinsic function|function|subprogram}0 call, "
  "expected at least %1, have %2">;
def err_typecheck_call_too_few_args_at_least_one : Error<
  "too few arguments to "
  "%select{intrinsic function|function|subprogram}0 call, "
  "at least argument %1 must be specified">;
def err_typecheck_call_too_many_args : Error<
  "too many arguments to "
  "%select{intrinsic function|function|subprogram}0 call, "
  "expected %1, have %2">;
def err_typecheck_call_too_many_args_one : Error<
  "too many arguments to "
  "%select{intrinsic function|function|subprogram}0 call, "
  "expected single argument %1, have %2 arguments">;
def err_typecheck_call_too_many_args_at_most : Error<
  "too many arguments to "
  "%select{intrinsic function|function|subprogram}0 call, "
  "expected at most %1, have %2">;
def err_typecheck_call_too_many_args_at_most_one : Error<
  "too many arguments to "
  "%select{intrinsic function|function|subprogram}0 call, "
  "expected at most single argument %1, have %2 arguments">;

}

}
