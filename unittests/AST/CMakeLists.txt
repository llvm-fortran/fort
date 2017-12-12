add_fort_unittest(exprEvaluatorTest
  ExprEvaluator.cpp
  )

target_link_libraries(exprEvaluatorTest
  fortAST
  fortFrontend
  fortParse
  fortSema
  fortBasic
  fortCodeGen
  )
