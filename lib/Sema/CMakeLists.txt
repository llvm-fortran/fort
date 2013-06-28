add_flang_library(flangSema
  DeclSpec.cpp
  Scope.cpp
  Sema.cpp
  SemaExpr.cpp
  SemaFormat.cpp
  SemaIO.cpp
  Spec.cpp
  )

target_link_libraries(flangSema
  flangAST
  )
