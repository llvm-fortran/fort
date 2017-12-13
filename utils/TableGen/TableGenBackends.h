//===- TableGenBackends.h - Declarations for Fort TableGen Backends ------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the declarations for all of the Fort TableGen
// backends. A "TableGen backend" is just a function. See
// "$LLVM_ROOT/utils/TableGen/TableGenBackends.h" for more info.
//
//===----------------------------------------------------------------------===//

#include <string>

namespace llvm {
  class raw_ostream;
  class RecordKeeper;
}

using llvm::raw_ostream;
using llvm::RecordKeeper;

namespace fort {

void EmitFortDeclContext(RecordKeeper &RK, raw_ostream &OS);
void EmitFortASTNodes(RecordKeeper &RK, raw_ostream &OS,
                       const std::string &N, const std::string &S);

void EmitFortDiagsDefs(RecordKeeper &Records, raw_ostream &OS,
                        const std::string &Component);
void EmitFortDiagGroups(RecordKeeper &Records, raw_ostream &OS);
void EmitFortDiagsIndexName(RecordKeeper &Records, raw_ostream &OS);

} // end namespace fort
