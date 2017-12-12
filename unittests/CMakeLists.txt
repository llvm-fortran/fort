add_custom_target(FortUnitTests)
set_target_properties(FortUnitTests PROPERTIES FOLDER "Fort tests")

# add_fort_unittest(test_dirname file1.cpp file2.cpp)
#
# Will compile the list of files together and link against the fort
# Produces a binary named 'basename(test_dirname)'.
function(add_fort_unittest test_dirname)
  add_unittest(FortUnitTests ${test_dirname} ${ARGN})
endfunction()

add_subdirectory(AST)
