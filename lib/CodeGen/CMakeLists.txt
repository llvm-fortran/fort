add_flang_library(flangCodeGen
  ModuleBuilder.cpp
  CodeGenModule.cpp
  CodeGenFunction.cpp
  CGStmt.cpp
)

target_link_libraries(flangCodeGen
  flangAST
  )
