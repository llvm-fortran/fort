set(LLVM_LINK_COMPONENTS Support)

add_tablegen(fort-tblgen FORT
  FlangASTNodesEmitter.cpp
  FlangDiagnosticsEmitter.cpp
  TableGen.cpp
  )
