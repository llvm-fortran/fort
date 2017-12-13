//===- TableGen.cpp - Top-Level TableGen implementation for Fort ---------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the main function for Fort's TableGen.
//
//===----------------------------------------------------------------------===//

#include "TableGenBackends.h" // Declares all backends.
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/TableGen/Error.h"
#include "llvm/TableGen/Main.h"
#include "llvm/TableGen/Record.h"

using namespace llvm;
using namespace fort;

enum ActionType {
  GenFortDiagsDefs,
  GenFortDiagGroups,
  GenFortDiagsIndexName,
  GenFortDeclNodes,
  GenFortStmtNodes,
  GenFortExprNodes
};

namespace {
  cl::opt<ActionType>
  Action(cl::desc("Action to perform:"),
         cl::values(clEnumValN(GenFortDiagsDefs, "gen-fort-diags-defs",
                               "Generate Fort diagnostics definitions"),
                    clEnumValN(GenFortDiagGroups, "gen-fort-diag-groups",
                               "Generate Fort diagnostic groups"),
                    clEnumValN(GenFortDiagsIndexName,
                               "gen-fort-diags-index-name",
                               "Generate Fort diagnostic name index"),
                    clEnumValN(GenFortDeclNodes, "gen-fort-decl-nodes",
                               "Generate Fort AST declaration nodes"),
                    clEnumValN(GenFortStmtNodes, "gen-fort-stmt-nodes",
                               "Generate Fort AST statement nodes"),
                    clEnumValN(GenFortExprNodes, "gen-fort-expr-nodes",
                               "Generate Fort AST expression nodes")));

  cl::opt<std::string>
  FortComponent("fort-component",
                 cl::desc("Only use warnings from specified component"),
                 cl::value_desc("component"), cl::Hidden);

bool FortTableGenMain(raw_ostream &OS, RecordKeeper &Records) {
  switch (Action) {
  case GenFortDiagsDefs:
    EmitFortDiagsDefs(Records, OS, FortComponent);
    break;
  case GenFortDiagGroups:
    EmitFortDiagGroups(Records, OS);
    break;
  case GenFortDiagsIndexName:
    EmitFortDiagsIndexName(Records, OS);
    break;
  case GenFortDeclNodes:
    EmitFortASTNodes(Records, OS, "Decl", "Decl");
    EmitFortDeclContext(Records, OS);
    break;
  case GenFortStmtNodes:
    EmitFortASTNodes(Records, OS, "Stmt", "");
    break;
  case GenFortExprNodes:
    EmitFortASTNodes(Records, OS, "Expr", "");
    break;
  }

  return false;
}
}

int main(int argc, char **argv) {
  sys::PrintStackTraceOnErrorSignal(argv[0]);
  PrettyStackTraceProgram X(argc, argv);
  cl::ParseCommandLineOptions(argc, argv);

  return TableGenMain(argv[0], &FortTableGenMain);
}
