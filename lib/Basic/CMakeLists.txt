set(LLVM_LINK_COMPONENTS mc)

find_first_existing_vc_file("${LLVM_MAIN_SRC_DIR}" llvm_vc)
find_first_existing_vc_file("${FORT_SOURCE_DIR}" fort_vc) 

# The VC revision include that we want to generate.
set(version_inc "${CMAKE_CURRENT_BINARY_DIR}/VCSVersion.inc")

set(generate_vcs_version_script "${LLVM_CMAKE_PATH}/GenerateVersionFromVCS.cmake")

if (llvm_vc)
  set(llvm_source_dir ${LLVM_MAIN_SRC_DIR})
endif()
if (fort_vc)
  set(fort_source_dir ${FORT_SOURCE_DIR})
endif()

# Create custom target to generate the VC revision include.
add_custom_command(OUTPUT "${version_inc}"
  DEPENDS "${llvm_vc}" "${fort_vc}" "${generate_vcs_version_script}"
  COMMAND ${CMAKE_COMMAND} "-DNAMES=\"LLVM;FORT\""
                           "-DLLVM_SOURCE_DIR=${llvm_source_dir}"
                           "-DFORT_SOURCE_DIR=${fort_source_dir}"
                           "-DHEADER_FILE=${version_inc}"
                           -P "${generate_vcs_version_script}")

# Mark the generated header as being generated.
set_source_files_properties("${version_inc}"
  PROPERTIES GENERATED TRUE
             HEADER_FILE_ONLY TRUE)

set_property(SOURCE Version.cpp APPEND PROPERTY
             COMPILE_DEFINITIONS "HAVE_VCS_VERSION_INC")

add_fort_library(fortBasic
  CharInfo.cpp
  Diagnostic.cpp
  DiagnosticIDs.cpp
  IdentifierTable.cpp
  TargetInfo.cpp
  Targets.cpp
  Token.cpp
  TokenKinds.cpp
  Version.cpp
  ${version_inc}
)

