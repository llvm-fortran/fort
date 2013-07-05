add_flang_library(flangCodeGen
  ModuleBuilder.cpp
  CodeGenModule.cpp
  CodeGenFunction.cpp
  CodeGenTypes.cpp
  CGStmt.cpp
  CGExpr.cpp
  CGExprScalar.cpp
  CGExprComplex.cpp
)

target_link_libraries(flangCodeGen
  flangAST
  )
