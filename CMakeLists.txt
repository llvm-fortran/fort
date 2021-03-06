cmake_minimum_required(VERSION 3.4.3)

if(POLICY CMP0077)
  cmake_policy(SET CMP0077 NEW)
endif()

# If we are not building as a part of LLVM, build Fort as an
# standalone project, using LLVM as an external library:
if( CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR )
  project(Fort)

  # Rely on llvm-config.
  set(CONFIG_OUTPUT)
  find_program(LLVM_CONFIG "llvm-config")
  if(LLVM_CONFIG)
    message(STATUS "Found LLVM_CONFIG as ${LLVM_CONFIG}")
    set(CONFIG_COMMAND ${LLVM_CONFIG}
      "--assertion-mode"
      "--bindir"
      "--libdir"
      "--includedir"
      "--prefix"
      "--src-root"
      "--cmakedir")
    execute_process(
      COMMAND ${CONFIG_COMMAND}
      RESULT_VARIABLE HAD_ERROR
      OUTPUT_VARIABLE CONFIG_OUTPUT
    )
    if(NOT HAD_ERROR)
      string(REGEX REPLACE
        "[ \t]*[\r\n]+[ \t]*" ";"
        CONFIG_OUTPUT ${CONFIG_OUTPUT})
    else()
      string(REPLACE ";" " " CONFIG_COMMAND_STR "${CONFIG_COMMAND}")
      message(STATUS "${CONFIG_COMMAND_STR}")
      message(FATAL_ERROR "llvm-config failed with status ${HAD_ERROR}")
    endif()
  else()
    message(FATAL_ERROR "llvm-config not found -- ${LLVM_CONFIG}")
  endif()

  list(GET CONFIG_OUTPUT 0 ENABLE_ASSERTIONS)
  list(GET CONFIG_OUTPUT 1 TOOLS_BINARY_DIR)
  list(GET CONFIG_OUTPUT 2 LIBRARY_DIR)
  list(GET CONFIG_OUTPUT 3 INCLUDE_DIR)
  list(GET CONFIG_OUTPUT 4 LLVM_OBJ_ROOT)
  list(GET CONFIG_OUTPUT 5 MAIN_SRC_DIR)
  list(GET CONFIG_OUTPUT 6 LLVM_CONFIG_CMAKE_PATH)

  if(NOT MSVC_IDE)
    set(LLVM_ENABLE_ASSERTIONS ${ENABLE_ASSERTIONS}
      CACHE BOOL "Enable assertions")
    # Assertions should follow llvm-config's.
    mark_as_advanced(LLVM_ENABLE_ASSERTIONS)
  endif()

  set(LLVM_TOOLS_BINARY_DIR ${TOOLS_BINARY_DIR} CACHE PATH "Path to llvm/bin")
  set(LLVM_LIBRARY_DIR ${LIBRARY_DIR} CACHE PATH "Path to llvm/lib")
  set(LLVM_MAIN_INCLUDE_DIR ${INCLUDE_DIR} CACHE PATH "Path to llvm/include")
  set(LLVM_BINARY_DIR ${LLVM_OBJ_ROOT} CACHE PATH "Path to LLVM build tree")
  set(LLVM_MAIN_SRC_DIR ${MAIN_SRC_DIR} CACHE PATH "Path to LLVM source tree")

  # Normalize LLVM_CMAKE_PATH. --cmakedir might contain backslashes.
  # CMake assumes slashes as PATH.
  file(TO_CMAKE_PATH ${LLVM_CONFIG_CMAKE_PATH} LLVM_CMAKE_PATH)

  find_program(LLVM_TABLEGEN_EXE "llvm-tblgen" ${LLVM_TOOLS_BINARY_DIR}
    NO_DEFAULT_PATH)

  set(LLVMCONFIG_FILE "${LLVM_CMAKE_PATH}/LLVMConfig.cmake")
  if(EXISTS ${LLVMCONFIG_FILE})
    list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_PATH}")
    include(${LLVMCONFIG_FILE})
  else()
    message(FATAL_ERROR "Not found: ${LLVMCONFIG_FILE}")
  endif()

  # They are used as destination of target generators.
  set(LLVM_RUNTIME_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/bin)
  set(LLVM_LIBRARY_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/lib)

  option(LLVM_INSTALL_TOOLCHAIN_ONLY
    "Only include toolchain files in the 'install' target." OFF)

  option(LLVM_FORCE_USE_OLD_HOST_TOOLCHAIN
    "Set to ON to force using an old, unsupported host toolchain." OFF)

  include(AddLLVM)
  include(TableGen)
  include(HandleLLVMOptions)

  set(PACKAGE_VERSION "${LLVM_PACKAGE_VERSION}")

  if (NOT DEFINED LLVM_INCLUDE_TESTS)
    set(LLVM_INCLUDE_TESTS ON)
  endif()

  include_directories("${LLVM_BINARY_DIR}/include" "${LLVM_MAIN_INCLUDE_DIR}")
  link_directories("${LLVM_LIBRARY_DIR}")

  set( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin )
  set( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib )
  set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib )

  if(LLVM_INCLUDE_TESTS)
    # Check prebuilt llvm/utils.
    if(EXISTS ${LLVM_TOOLS_BINARY_DIR}/FileCheck${CMAKE_EXECUTABLE_SUFFIX}
        AND EXISTS ${LLVM_TOOLS_BINARY_DIR}/count${CMAKE_EXECUTABLE_SUFFIX}
        AND EXISTS ${LLVM_TOOLS_BINARY_DIR}/not${CMAKE_EXECUTABLE_SUFFIX})
      set(LLVM_UTILS_PROVIDED ON)
    endif()

    if(EXISTS ${LLVM_MAIN_SRC_DIR}/utils/lit/lit.py)
      # Note: path not really used, except for checking if lit was found
      set(LLVM_LIT ${LLVM_MAIN_SRC_DIR}/utils/lit/lit.py)
      if(EXISTS ${LLVM_MAIN_SRC_DIR}/utils/llvm-lit)
        add_subdirectory(${LLVM_MAIN_SRC_DIR}/utils/llvm-lit utils/llvm-lit)
      endif()
      if(NOT LLVM_UTILS_PROVIDED)
        add_subdirectory(${LLVM_MAIN_SRC_DIR}/utils/FileCheck utils/FileCheck)
        add_subdirectory(${LLVM_MAIN_SRC_DIR}/utils/count utils/count)
        add_subdirectory(${LLVM_MAIN_SRC_DIR}/utils/not utils/not)
        set(LLVM_UTILS_PROVIDED ON)
        set(FORT_TEST_DEPS FileCheck count not)
      endif()
      set(UNITTEST_DIR ${LLVM_MAIN_SRC_DIR}/utils/unittest)
      if(EXISTS ${UNITTEST_DIR}/googletest/include/gtest/gtest.h
          AND NOT EXISTS ${LLVM_LIBRARY_DIR}/${CMAKE_STATIC_LIBRARY_PREFIX}gtest${CMAKE_STATIC_LIBRARY_SUFFIX}
          AND EXISTS ${UNITTEST_DIR}/CMakeLists.txt)
        add_subdirectory(${UNITTEST_DIR} utils/unittest)
      endif()
    else()
      # Seek installed Lit.
      find_program(LLVM_LIT "lit.py" ${LLVM_MAIN_SRC_DIR}/utils/lit
        DOC "Path to lit.py")
    endif()

    if(LLVM_LIT)
      # Define the default arguments to use with 'lit', and an option for the user
      # to override.
      set(LIT_ARGS_DEFAULT "-sv")
      if (MSVC OR XCODE)
        set(LIT_ARGS_DEFAULT "${LIT_ARGS_DEFAULT} --no-progress-bar")
      endif()
      set(LLVM_LIT_ARGS "${LIT_ARGS_DEFAULT}" CACHE STRING "Default options for lit")

      # On Win32 hosts, provide an option to specify the path to the GnuWin32 tools.
      if( WIN32 AND NOT CYGWIN )
        set(LLVM_LIT_TOOLS_DIR "" CACHE PATH "Path to GnuWin32 tools")
      endif()
    else()
      set(LLVM_INCLUDE_TESTS OFF)
    endif()
  endif()

  set( FORT_BUILT_STANDALONE 1 )
  set(BACKEND_PACKAGE_STRING "LLVM ${LLVM_PACKAGE_VERSION}")
else()
  set(BACKEND_PACKAGE_STRING "${PACKAGE_STRING}")
endif()

set(FORT_RESOURCE_DIR "" CACHE STRING
  "Relative directory from the Fort binary to its resource files.")

set(C_INCLUDE_DIRS "" CACHE STRING
  "Colon separated list of directories fort will search for headers.")

set(GCC_INSTALL_PREFIX "" CACHE PATH "Directory where gcc is installed." )
set(DEFAULT_SYSROOT "" CACHE PATH
  "Default <path> to all compiler invocations for --sysroot=<path>." )

set(FORT_VENDOR "" CACHE STRING
  "Vendor-specific text for showing with version information.")

if( FORT_VENDOR )
  add_definitions( -DFORT_VENDOR="${FORT_VENDOR} " )
endif()

set(FORT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(FORT_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})

if( CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR AND NOT MSVC_IDE )
  message(FATAL_ERROR "In-source builds are not allowed. CMake would overwrite "
"the makefiles distributed with LLVM. Please create a directory and run cmake "
"from there, passing the path to this source directory as the last argument. "
"This process created the file `CMakeCache.txt' and the directory "
"`CMakeFiles'. Please delete them.")
endif()

if( NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR )
  file(GLOB_RECURSE
    tablegenned_files_on_include_dir
    "${FORT_SOURCE_DIR}/include/fort/*.inc")
  if( tablegenned_files_on_include_dir )
    message(FATAL_ERROR "Apparently there is a previous in-source build, "
"probably as the result of running `configure' and `make' on "
"${FORT_SOURCE_DIR}. This may cause problems. The suspicious files are:\n"
"${tablegenned_files_on_include_dir}\nPlease clean the source directory.")
  endif()
endif()

# Compute the Fort version from the LLVM version.
string(REGEX MATCH "[0-9]+\\.[0-9]+(\\.[0-9]+)?" FORT_VERSION
  ${PACKAGE_VERSION})
message(STATUS "Fort version: ${FORT_VERSION}")

string(REGEX REPLACE "([0-9]+)\\.[0-9]+(\\.[0-9]+)?" "\\1" FORT_VERSION_MAJOR
  ${FORT_VERSION})
string(REGEX REPLACE "[0-9]+\\.([0-9]+)(\\.[0-9]+)?" "\\1" FORT_VERSION_MINOR
  ${FORT_VERSION})
if (${FORT_VERSION} MATCHES "[0-9]+\\.[0-9]+\\.[0-9]+")
  set(FORT_HAS_VERSION_PATCHLEVEL 1)
  string(REGEX REPLACE "[0-9]+\\.[0-9]+\\.([0-9]+)" "\\1" FORT_VERSION_PATCHLEVEL
    ${FORT_VERSION})
else()
  set(FORT_HAS_VERSION_PATCHLEVEL 0)
endif()

# Configure the Version.inc file.
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/include/fort/Basic/Version.inc.in
  ${CMAKE_CURRENT_BINARY_DIR}/include/fort/Basic/Version.inc)

# Add appropriate flags for GCC
if (LLVM_COMPILER_IS_GCC_COMPATIBLE)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-common -Woverloaded-virtual -Wcast-qual -fno-strict-aliasing -pedantic -Wno-long-long -Wall -W -Wno-unused-parameter -Wwrite-strings")
endif ()

if (APPLE)
  set(CMAKE_MODULE_LINKER_FLAGS "-Wl,-flat_namespace -Wl,-undefined -Wl,suppress")
endif ()

configure_file(
  ${FORT_SOURCE_DIR}/include/fort/Config/config.h.cmake
  ${FORT_BINARY_DIR}/include/fort/Config/config.h)

#include(LLVMParseArguments)

function(fort_tablegen)
  # Syntax:
  # fort_tablegen output-file [tablegen-arg ...] SOURCE source-file
  # [[TARGET cmake-target-name] [DEPENDS extra-dependency ...]]
  #
  # Generates a custom command for invoking tblgen as
  #
  # tblgen source-file -o=output-file tablegen-arg ...
  #
  # and, if cmake-target-name is provided, creates a custom target for
  # executing the custom command depending on output-file. It is
  # possible to list more files to depend after DEPENDS.

  cmake_parse_arguments(CTG "" "SOURCE;TARGET" "" ${ARGN})

  if( NOT CTG_SOURCE )
    message(FATAL_ERROR "SOURCE source-file required by fort_tablegen")
  endif()

  set( LLVM_TARGET_DEFINITIONS ${CTG_SOURCE} )
  tablegen(FORT ${CTG_UNPARSED_ARGUMENTS})

  if(CTG_TARGET)
    add_public_tablegen_target(${CTG_TARGET})
    set_target_properties( ${CTG_TARGET} PROPERTIES FOLDER "Fort tablegenning")
    set_property(GLOBAL APPEND PROPERTY FORT_TABLEGEN_TARGETS ${CTG_TARGET})
  endif()
endfunction(fort_tablegen)

macro(add_fort_library name)
  llvm_process_sources(srcs ${ARGN})
  if(MSVC_IDE OR XCODE)
    # Add public headers
    file(RELATIVE_PATH lib_path
      ${FORT_SOURCE_DIR}/lib/
      ${CMAKE_CURRENT_SOURCE_DIR}
    )
    if(NOT lib_path MATCHES "^[.][.]")
      file( GLOB_RECURSE headers
        ${FORT_SOURCE_DIR}/include/fort/${lib_path}/*.h
        ${FORT_SOURCE_DIR}/include/fort/${lib_path}/*.def
      )
      set_source_files_properties(${headers} PROPERTIES HEADER_FILE_ONLY ON)

      file( GLOB_RECURSE tds
        ${FORT_SOURCE_DIR}/include/fort/${lib_path}/*.td
      )
      source_group("TableGen descriptions" FILES ${tds})
      set_source_files_properties(${tds}} PROPERTIES HEADER_FILE_ONLY ON)

      set(srcs ${srcs} ${headers} ${tds})
    endif()
  endif(MSVC_IDE OR XCODE)
  if (MODULE)
    set(libkind MODULE)
  elseif (SHARED_LIBRARY)
    set(libkind SHARED)
  else()
    set(libkind)
  endif()
  llvm_add_library( ${name} ${libkind} ${srcs} )
endmacro(add_fort_library)

macro(add_fort_executable name)
  add_llvm_executable( ${name} ${ARGN} )
  set_target_properties(${name} PROPERTIES FOLDER "Fort executables")
endmacro(add_fort_executable)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

include_directories(BEFORE
  ${CMAKE_CURRENT_BINARY_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  )

install(DIRECTORY include/
  DESTINATION include
  FILES_MATCHING
  PATTERN "*.def"
  PATTERN "*.h"
  PATTERN "config.h" EXCLUDE
  PATTERN ".svn" EXCLUDE
  )

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include/
  DESTINATION include
  FILES_MATCHING
  PATTERN "CMakeFiles" EXCLUDE
  PATTERN "*.inc"
  )

add_definitions( -D_GNU_SOURCE )

# Fort version information
set(FORT_EXECUTABLE_VERSION
     "${FORT_VERSION_MAJOR}" CACHE STRING
    "Version number that will be appended to the fort executable")
set(LIBFORT_LIBRARY_VERSION
     "${FORT_VERSION_MAJOR}" CACHE STRING
    "Version number that will be appended to the libfort library")
mark_as_advanced(FORT_EXECUTABLE_VERSION LIBFORT_LIBRARY_VERSION)

add_subdirectory(utils/TableGen)

add_subdirectory(include)

# All targets below may depend on all tablegen'd files
get_property(FORT_TABLEGEN_TARGETS GLOBAL PROPERTY FORT_TABLEGEN_TARGETS)
list(APPEND LLVM_COMMON_DEPENDS ${FORT_TABLEGEN_TARGETS})

add_subdirectory(lib)
add_subdirectory(tools)

if( LLVM_INCLUDE_TESTS )
  add_subdirectory(unittests)
  list(APPEND FORT_TEST_DEPS FortUnitTests)
    list(APPEND FORT_TEST_PARAMS
      fort_unit_site_config=${CMAKE_CURRENT_BINARY_DIR}/test/Unit/lit.site.cfg
      )
  add_subdirectory(test)
endif()

option(FORT_INCLUDE_DOCS "Generate build targets for the Fort docs."
  ${LLVM_INCLUDE_DOCS})
if( FORT_INCLUDE_DOCS )
  add_subdirectory(docs)
endif()

# Workaround for MSVS10 to avoid the Dialog Hell
# FIXME: This could be removed with future version of CMake.
if( FORT_BUILT_STANDALONE AND MSVC_VERSION EQUAL 1600 )
  set(FORT_SLN_FILENAME "${CMAKE_CURRENT_BINARY_DIR}/Fort.sln")
  if( EXISTS "${FORT_SLN_FILENAME}" )
    file(APPEND "${FORT_SLN_FILENAME}" "\n# This should be regenerated!\n")
  endif()
endif()

set(BUG_REPORT_URL "https://github.com/llvm-fortran/fort/issues" CACHE STRING
  "Default URL where bug reports are to be submitted.")

