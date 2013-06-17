//==--- DiagnosticParseKinds.td - libparse diagnostics --------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Parser Diagnostics
//===----------------------------------------------------------------------===//

let Component = "Parse" in {

let CategoryName = "Parse Issue" in {

// Generic errors
def err_parse_error : Error<"parse error">;
def err_duplicate_declspec : Error<"duplicate '%0' declaration specifier">;
def err_unexpected_lparen : Error<"unexpected '('">;

def err_expected_expression : Error<"expected expression">;
def err_expected_type : Error<"expected a type">;
def err_expected_external_declaration : Error<"expected external declaration">;
def err_extraneous_closing_brace : Error<"extraneous closing brace ('}')">;
def err_expected_ident : Error<"expected identifier">;
def err_expected_ident_lparen : Error<"expected identifier or '('">;
def err_expected_ident_lbrace : Error<"expected identifier or '{'">;
def err_expected_lbrace : Error<"expected '{'">;
def err_expected_lparen : Error<"expected '('">;
def err_expected_lparen_or_lbrace : Error<"expected '('or '{'">;
def err_expected_rparen : Error<"expected ')'">;
def err_expected_lsquare : Error<"expected '['">;
def err_expected_rsquare : Error<"expected ']'">;
def err_expected_rbrace : Error<"expected '}'">;
def err_expected_colon : Error<"expected ':'">;
def err_expected_semi_declaration : Error<
  "expected ';' at end of declaration">;
def err_expected_semi_decl_list : Error<
  "expected ';' at end of declaration list">;
def ext_expected_semi_decl_list : ExtWarn<
  "expected ';' at end of declaration list">;
def err_expected_member_name_or_semi : Error<
  "expected member name or ';' after declaration specifiers">;

def err_expected_fn_body : Error<
  "expected function body after function declarator">;

// Declarations
def err_invalid_decl_spec_combination : Error<
  "cannot combine with previous '%0' declaration specifier">;


} // end of Parse Issue category.
} // end of Parser diagnostics
