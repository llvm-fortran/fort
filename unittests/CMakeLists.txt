add_custom_target(FlangUnitTests)
set_target_properties(FlangUnitTests PROPERTIES FOLDER "Flang tests")

# add_flang_unittest(test_dirname file1.cpp file2.cpp)
#
# Will compile the list of files together and link against the flang
# Produces a binary named 'basename(test_dirname)'.
function(add_flang_unittest test_dirname)
  add_unittest(FlangUnitTests ${test_dirname} ${ARGN})
endfunction()

add_subdirectory(AST)
