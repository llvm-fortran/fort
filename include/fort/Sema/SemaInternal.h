//===--- SemaInternal.h - Internal Sema Interfaces --------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_FORT_SEMA_SEMA_INTERNAL_H
#define LLVM_FORT_SEMA_SEMA_INTERNAL_H

#include "fort/AST/ASTContext.h"
#include "fort/AST/FormatSpec.h"
#include "fort/AST/StmtVisitor.h"
#include "fort/Sema/Sema.h"

namespace fort {

/// StmtLabelResolver - 'Sends' a notification
/// about the statement labels declared after they are used
/// to the statements that use them.
class StmtLabelResolver : public StmtVisitor<StmtLabelResolver> {
  DiagnosticsEngine &Diags;
  fort::Sema &Sema;
  StmtLabelScope::ForwardDecl Info;
  Stmt *StmtLabelDecl;
  bool Error;

public:
  void VisitStmt(Stmt *S) { Error = true; }
  void VisitAssignStmt(AssignStmt *S);
  void VisitAssignedGotoStmt(AssignedGotoStmt *S);
  void VisitGotoStmt(GotoStmt *S);
  void VisitComputedGotoStmt(ComputedGotoStmt *S);
  void VisitDoStmt(DoStmt *S);

  void VisitLabelFormatSpec(LabelFormatSpec *FS);
  void Visit(FormatSpec *FS) {
    if (auto LFS = dyn_cast<LabelFormatSpec>(FS))
      VisitLabelFormatSpec(LFS);
    else
      Error = true;
  }

  StmtLabelResolver(fort::Sema &TheSema, DiagnosticsEngine &Diag)
      : Sema(TheSema), Diags(Diag), Info(nullptr, (Stmt *)nullptr) {}

  /// \brief Returns false if the forward declaration has no notification
  /// handler. Notifies the statement with a statement label that the label
  /// is used.
  void ResolveForwardUsage(const StmtLabelScope::ForwardDecl &S, Stmt *Decl) {
    Decl->setStmtLabelUsed();
    Error = false;
    Info = S;
    StmtLabelDecl = Decl;
    if (S.Statement)
      StmtVisitor::Visit(S.Statement);
    else
      Visit(S.FS);
    assert(!Error);
  }
};

} // namespace fort

#endif
