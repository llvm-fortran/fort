add_fort_library(fortParse
  Lexer.cpp
  ParseDecl.cpp
  ParseSpecStmt.cpp
  ParseExec.cpp
  ParseExpr.cpp
  ParseFormat.cpp
  Parser.cpp
  FixedForm.cpp
)

add_dependencies(fortParse
  FortDeclNodes
  FortDiagnosticParse
  )

target_link_libraries(fortParse
  fortAST
  )
