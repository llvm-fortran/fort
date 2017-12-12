set(LLVM_LINK_COMPONENTS support)

add_fort_library(fortAST
  ASTContext.cpp
  Decl.cpp
  DeclGroup.cpp
  DeclarationName.cpp
  Expr.cpp
  ExprConstant.cpp
  ExprArray.cpp
  FormatSpec.cpp
  FormatItem.cpp
  IOSpec.cpp
  Stmt.cpp
  ASTDumper.cpp
  Type.cpp
  IntrinsicFunctions.cpp
  )

add_dependencies(fortAST
  FortDiagnosticCommon
  FortDeclNodes
  FortStmtNodes
  FortExprNodes
  )

target_link_libraries(fortAST
  PRIVATE
  fortBasic
  )
