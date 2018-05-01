//===--- Options.h - Option info & table ------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_FORT_DRIVER_OPTIONS_H
#define LLVM_FORT_DRIVER_OPTIONS_H

#include <memory>

namespace llvm {
namespace opt {
class OptTable;
}
} // namespace llvm

namespace fort {
namespace driver {

namespace options {
/// Flags specifically for fort options.  Must not overlap with
/// llvm::opt::DriverFlag.
enum FortFlags {
  DriverOption = (1 << 4),
  LinkerInput = (1 << 5),
  NoArgumentUnused = (1 << 6),
  Unsupported = (1 << 7),
  FC1Option = (1 << 10),
  FC1AsOption = (1 << 10),
  NoDriverOption = (1 << 12),
  Ignored = (1 << 13)
};

enum ID {
  OPT_INVALID = 0, // This is not an option ID.
#define OPTION(PREFIX, NAME, ID, KIND, GROUP, ALIAS, ALIASARGS, FLAGS, PARAM,  \
               HELPTEXT, METAVAR, VALUES)                                      \
  OPT_##ID,
#include "fort/Driver/Options.inc"
  LastOption
#undef OPTION
};
} // namespace options

std::unique_ptr<llvm::opt::OptTable> createDriverOptTable();
} // namespace driver
} // namespace fort

#endif
