//===--- DriverOptions.cpp - Driver Options Table -------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "fort/Driver/Options.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Option/OptTable.h"
#include "llvm/Option/Option.h"
#include <cassert>

using namespace fort::driver;
using namespace fort::driver::options;
using namespace llvm::opt;

#define PREFIX(NAME, VALUE) static const char *const NAME[] = VALUE;
#include "fort/Driver/Options.inc"
#undef PREFIX

static const OptTable::Info InfoTable[] = {
#define OPTION(PREFIX, NAME, ID, KIND, GROUP, ALIAS, ALIASARGS, FLAGS, PARAM,  \
               HELPTEXT, METAVAR, VALUES)                                      \
  {PREFIX, NAME,  HELPTEXT,    METAVAR,     OPT_##ID,  Option::KIND##Class,    \
   PARAM,  FLAGS, OPT_##GROUP, OPT_##ALIAS, ALIASARGS, VALUES},
#include "fort/Driver/Options.inc"
#undef OPTION
};

namespace {

class DriverOptTable : public OptTable {
public:
  DriverOptTable()
    : OptTable(InfoTable) {}
};

}

std::unique_ptr<OptTable> fort::driver::createDriverOptTable() {
  auto Result = std::make_unique<DriverOptTable>();
  // Options.inc is included in DriverOptions.cpp, and calls OptTable's
  // addValues function.
  // Opt is a variable used in the code fragment in Options.inc.
  OptTable &Opt = *Result;
#define OPTTABLE_ARG_INIT
#include "fort/Driver/Options.inc"
#undef OPTTABLE_ARG_INIT
  return std::move(Result);
}
