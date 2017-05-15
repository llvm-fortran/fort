message("Making Flang test configuration files")

set(FLANG_PATH_TO_LLVM_LIT ${FLANG_PATH_TO_LLVM_BUILD}/bin/llvm-lit)
set(LLVM_SOURCE_DIR ${FLANG_PATH_TO_LLVM_SOURCE})
set(LLVM_BINARY_DIR ${FLANG_PATH_TO_LLVM_BUILD})
set(LLVM_TOOLS_DIR ${FLANG_PATH_TO_LLVM_BUILD}/tools)
set(LLVM_LIBS_DIR ${FLANG_PATH_TO_LLVM_BUILD}/lib)

#configure_file(lit.site.cfg.in  ${CMAKE_CURRENT_BINARY_DIR}/lit.site.cfg)
configure_lit_site_cfg(
  ${CMAKE_CURRENT_SOURCE_DIR}/lit.site.cfg.in
  ${CMAKE_CURRENT_BINARY_DIR}/lit.site.cfg
  )

configure_file(TestRunner.sh.in ${CMAKE_CURRENT_BINARY_DIR}/TestRunner.sh)

option(FLANG_TEST_USE_VG "Run Flang tests under Valgrind" OFF)
if(FLANG_TEST_USE_VG)
  set(FLANG_TEST_EXTRA_ARGS ${FLANG_TEST_EXTRA_ARGS} "--vg")
endif ()

list(APPEND FLANG_TEST_DEPS
  flang
  )

set(FLANG_TEST_PARAMS
  flang_site_config=${CMAKE_CURRENT_BINARY_DIR}/lit.site.cfg
  )

if( NOT FLANG_BUILT_STANDALONE )
  list(APPEND FLANG_TEST_DEPS
    llvm-config
    FileCheck
    )
endif()

add_lit_testsuite(check-flang "Running Flang regression tests"
  ${CMAKE_CURRENT_BINARY_DIR}
  PARAMS ${FLANG_TEST_PARAMS}
  DEPENDS ${FLANG_TEST_DEPS}
  ARGS ${FLANG_TEST_EXTRA_ARGS}
  )
set_target_properties(check-flang PROPERTIES FOLDER "Flang tests")

