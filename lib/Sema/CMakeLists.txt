add_flang_library(flangSema
  DeclSpec.cpp
  Scope.cpp
  Sema.cpp
  )

target_link_libraries(flangSema
  flangAST
  )
