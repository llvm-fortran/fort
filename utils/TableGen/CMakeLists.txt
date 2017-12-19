set(LLVM_LINK_COMPONENTS Support)

add_tablegen(fort-tblgen FORT
  FortASTNodesEmitter.cpp
  FortDiagnosticsEmitter.cpp
  TableGen.cpp
  )
set_target_properties(fort-tblgen PROPERTIES FOLDER "Fort tablegenning")
