/* This generated file is for internal use. Do not include it from headers. */

#ifdef FORT_CONFIG_H
#error config.h can only be included once
#else
#define FORT_CONFIG_H

/* Bug report URL. */
#define BUG_REPORT_URL "${BUG_REPORT_URL}"

/* Default linker to use. */
#define FORT_DEFAULT_LINKER "${FORT_DEFAULT_LINKER}"

/* Default runtime library to use. */
#define FORT_DEFAULT_RTLIB "${FORT_DEFAULT_RTLIB}"

/* Default OpenMP runtime used by -fopenmp. */
#define FORT_DEFAULT_OPENMP_RUNTIME "${FORT_DEFAULT_OPENMP_RUNTIME}"

/* Multilib suffix for libdir. */
#define FORT_LIBDIR_SUFFIX "${FORT_LIBDIR_SUFFIX}"

/* Relative directory for resource files */
#define FORT_RESOURCE_DIR "${FORT_RESOURCE_DIR}"

/* Default <path> to all compiler invocations for --sysroot=<path>. */
#define DEFAULT_SYSROOT "${DEFAULT_SYSROOT}"

/* Directory where gcc is installed. */
#define GCC_INSTALL_PREFIX "${GCC_INSTALL_PREFIX}"

/* Define if we have libxml2 */
#cmakedefine FORT_HAVE_LIBXML ${FORT_HAVE_LIBXML}

/* Define if we have z3 and want to build it */
#cmakedefine FORT_ANALYZER_WITH_Z3 ${FORT_ANALYZER_WITH_Z3}

/* Define if we have sys/resource.h (rlimits) */
#cmakedefine FORT_HAVE_RLIMITS ${FORT_HAVE_RLIMITS}

/* The LLVM product name and version */
#define BACKEND_PACKAGE_STRING "${BACKEND_PACKAGE_STRING}"

/* Linker version detected at compile time. */
#cmakedefine HOST_LINK_VERSION "${HOST_LINK_VERSION}"

/* pass --build-id to ld */
#cmakedefine ENABLE_LINKER_BUILD_ID

/* enable x86 relax relocations by default */
#cmakedefine01 ENABLE_X86_RELAX_RELOCATIONS

/* Enable each functionality of modules */
#cmakedefine FORT_ENABLE_ARCMT
#cmakedefine FORT_ENABLE_OBJC_REWRITER
#cmakedefine FORT_ENABLE_STATIC_ANALYZER

#endif
