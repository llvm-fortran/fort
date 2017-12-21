set( LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  asmparser
  bitreader
  bitwriter
  codegen
  ipo
  selectiondag
  interpreter
  linker
  )

add_fort_executable(fort
  Main.cpp
  )

target_link_libraries(fort
  PRIVATE
  fortAST
  fortFrontend
  fortParse
  fortSema
  fortBasic
  fortCodeGen
  )

set_target_properties(fort PROPERTIES VERSION ${FORT_EXECUTABLE_VERSION})

# TODO open issue
#add_dependencies(fort fort-headers)

if(UNIX)
  set(fort_binary "fort${CMAKE_EXECUTABLE_SUFFIX}")
else()
  set(fort_binary "${LLVM_BINARY_DIR}/bin/${CMAKE_CFG_INTDIR}/fort${CMAKE_EXECUTABLE_SUFFIX}")
endif()

set_property(DIRECTORY APPEND
  PROPERTY ADDITIONAL_MAKE_CLEAN_FILES ${fort_pp})

install(TARGETS fort
  RUNTIME DESTINATION bin)

