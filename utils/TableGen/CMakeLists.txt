set(LLVM_LINK_COMPONENTS Support)

add_tablegen(fort-tblgen FORT
  FortASTNodesEmitter.cpp
  FortDiagnosticsEmitter.cpp
  TableGen.cpp
  )
