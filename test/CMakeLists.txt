configure_lit_site_cfg(
  ${CMAKE_CURRENT_SOURCE_DIR}/lit.site.cfg.in
  ${CMAKE_CURRENT_BINARY_DIR}/lit.site.cfg
  )

configure_file(TestRunner.sh.in ${CMAKE_CURRENT_BINARY_DIR}/TestRunner.sh)

option(FORT_TEST_USE_VG "Run Fort tests under Valgrind" OFF)
if(FORT_TEST_USE_VG)
  set(FORT_TEST_EXTRA_ARGS ${FORT_TEST_EXTRA_ARGS} "--vg")
endif ()

list(APPEND FORT_TEST_DEPS
  fort
  )

set(FORT_TEST_PARAMS
  fort_site_config=${CMAKE_CURRENT_BINARY_DIR}/lit.site.cfg
  )

if( NOT FORT_BUILT_STANDALONE )
  list(APPEND FORT_TEST_DEPS
    llvm-config
    FileCheck not
    )
endif()

add_lit_testsuite(check-fort "Running Fort regression tests"
  ${CMAKE_CURRENT_BINARY_DIR}
  PARAMS ${FORT_TEST_PARAMS}
  DEPENDS ${FORT_TEST_DEPS}
  ARGS ${FORT_TEST_EXTRA_ARGS}
  )
set_target_properties(check-fort PROPERTIES FOLDER "Fort tests")

