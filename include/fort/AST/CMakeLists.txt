fort_tablegen(DeclNodes.inc -gen-fort-decl-nodes
  SOURCE ../Basic/DeclNodes.td
  TARGET FortDeclNodes)

fort_tablegen(StmtNodes.inc -gen-fort-stmt-nodes
  SOURCE ../Basic/StmtNodes.td
  TARGET FortStmtNodes)

fort_tablegen(ExprNodes.inc -gen-fort-expr-nodes
  SOURCE ../Basic/ExprNodes.td
  TARGET FortExprNodes)
