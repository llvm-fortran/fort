macro(fort_diag_gen component)
  fort_tablegen(Diagnostic${component}Kinds.inc
    -gen-fort-diags-defs -fort-component=${component}
    SOURCE Diagnostic.td
    TARGET FortDiagnostic${component})
endmacro(fort_diag_gen)


fort_diag_gen(Common)
fort_diag_gen(Lex)
fort_diag_gen(Parse)
fort_diag_gen(Frontend)
fort_diag_gen(Sema)
fort_tablegen(DiagnosticGroups.inc -gen-fort-diag-groups
  SOURCE Diagnostic.td
  TARGET FortDiagnosticGroups)
