//===--- Scope.h - Scope interface ------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the Scope interface.
//
//===----------------------------------------------------------------------===//

#ifndef FLANG_SEMA_SCOPE_H__
#define FLANG_SEMA_SCOPE_H__

#include "flang/Basic/Diagnostic.h"
#include "flang/AST/Stmt.h"
#include "flang/AST/FormatSpec.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include <map>

namespace flang {

class Decl;
class DeclContext;
class UsingDirectiveDecl;

/// BlockStmtBuilder - Constructs bodies for statements and program units.
class BlockStmtBuilder {
public:
  /// \brief A list of executable statements for all the blocks
  std::vector<Stmt*> StmtList;

  /// \brief Represents a statement or a declaration with body(bodies) like DO or IF
  struct Entry {
    Stmt *Statement;
    size_t BeginOffset;
    /// \brief used only when a statement is a do which terminates
    /// with a labeled statement.
    Expr *ExpectedEndDoLabel;

    Entry()
      : Statement(nullptr),BeginOffset(0),
        ExpectedEndDoLabel(nullptr){
    }
    Entry(CFBlockStmt *S)
      : Statement(S), BeginOffset(0),
        ExpectedEndDoLabel(nullptr) {
    }
    Entry(DoStmt *S, Expr *ExpectedEndDo)
      : Statement(S), BeginOffset(0),
        ExpectedEndDoLabel(ExpectedEndDo) {
    }
    Entry(IfStmt *S)
      : Statement(S), BeginOffset(0) {
    }

    bool hasExpectedDoLabel() const {
      return ExpectedEndDoLabel != nullptr;
    }
  };

  /// \brief A stack of current block statements like IF and DO
  SmallVector<Entry, 16> ControlFlowStack;

  BlockStmtBuilder() {}

  void Enter(Entry S);
  void LeaveIfThen(ASTContext &C);
  void Leave(ASTContext &C);
  Stmt *LeaveOuterBody(ASTContext &C, SourceLocation Loc);

  ArrayRef<Stmt*> getDeclStatements() {
    return StmtList;
  }

  const Entry &LastEntered() const {
    return ControlFlowStack.back();
  }
  bool HasEntered() const {
    return ControlFlowStack.size() != 0;
  }

  void Append(Stmt *S);
private:
  Stmt *CreateBody(ASTContext &C, const Entry &Last);
};

/// StatementLabelScope - This is a component of a scope which assist with
/// declaring and resolving statement labels.
///
class StmtLabelScope {
public:
  StmtLabelScope *Parent;

  /// \brief Represents a usage of an undeclared statement label in
  /// some statement.
  struct ForwardDecl {
    Expr *StmtLabel;
    Stmt *Statement;
    FormatSpec *FS;
    size_t ResolveCallbackData;

    ForwardDecl(Expr *SLabel, Stmt *S,
                size_t CallbackData = 0)
      : StmtLabel(SLabel), Statement(S), FS(nullptr),
        ResolveCallbackData(CallbackData) {
    }

    ForwardDecl(Expr *SLabel, FormatSpec *fs,
                size_t CallbackData = 0)
      : StmtLabel(SLabel), Statement(nullptr), FS(fs),
        ResolveCallbackData(CallbackData) {
    }
  };

private:
  typedef std::map<StmtLabelInteger, Stmt*> StmtLabelMapTy;

  /// StmtLabelDeclsInScope - This keeps track of all the declarations of
  /// statement labels in this scope.
  StmtLabelMapTy StmtLabelDeclsInScope;

  /// ForwardStmtLabelDeclsInScope - This keeps track of all the forward
  /// referenced statement labels in this scope.
  llvm::SmallVector<ForwardDecl, 16> ForwardStmtLabelDeclsInScope;
public:

  typedef StmtLabelMapTy::const_iterator decl_iterator;
  decl_iterator decl_begin() const { return StmtLabelDeclsInScope.begin(); }
  decl_iterator decl_end()   const { return StmtLabelDeclsInScope.end(); }
  bool decl_empty()          const { return StmtLabelDeclsInScope.empty(); }

  ArrayRef<ForwardDecl> getForwardDecls() const {
    return ForwardStmtLabelDeclsInScope;
  }

  StmtLabelScope *getParent() const {
    return Parent;
  }
  void setParent(StmtLabelScope *P);

  /// \brief Declares a new statement label.
  /// Returns true if such declaration already exits.
  void Declare(Expr *StmtLabel, Stmt *Statement);

  /// \brief Tries to resolve a statement label reference.
  /// If the reference is resolved, the statement with the label
  /// is notified that the label is used.
  Stmt *Resolve(Expr *StmtLabel) const;

  /// \brief Declares a forward reference of some statement label.
  void DeclareForwardReference(ForwardDecl Reference);

  /// \brief Removes a forward reference of some statement label.
  void RemoveForwardReference(const Stmt *User);

  /// \brief Returns true is the two statement labels are identical.
  bool IsSame(const Expr *StmtLabelA, const Expr *StmtLabelB) const;
};

/// ImplicitTypingScope - This is a component of a scope which assist with
/// declaring and resolving typing rules using the IMPLICIT statement.
///
class ImplicitTypingScope {
  ImplicitTypingScope *Parent;
  llvm::StringMap<QualType> Rules;
  bool None;
public:
  ImplicitTypingScope(ImplicitTypingScope *Prev = nullptr);

  enum RuleType {
    DefaultRule,
    TypeRule,
    NoneRule
  };

  ImplicitTypingScope *getParent() const {
    return Parent;
  }
  void setParent(ImplicitTypingScope *P);

  /// \brief Associates a type rule with an identifier
  /// returns true if associating is sucessfull.
  bool Apply(const ImplicitStmt::LetterSpecTy& Spec, QualType T);

  /// \brief Applies an IMPLICIT NONE rule.
  /// returns true if the applicating is sucessfull.
  bool ApplyNone();

  /// \brief returns true if IMPLICIT NONE was used in this scope.
  bool isNoneInThisScope() const {
    return None;
  }

  /// \brief Returns a rule and possibly a type associated with this identifier.
  std::pair<RuleType, QualType> Resolve(const IdentifierInfo *IdInfo);
};

/// InnerScope - This is a scope which assists with resolving identifiers
/// in the inner scopes of declaration contexts, such as implied do
/// and statement function.
///
class InnerScope {
  llvm::StringMap<Decl*> Declarations;
  InnerScope *Parent;
public:
  InnerScope(InnerScope *Prev = nullptr);

  InnerScope *getParent() const { return Parent; }

  /// Declares a new declaration in this scope.
  void Declare(const IdentifierInfo *IDInfo, Decl *Declaration);

  /// Returns a valid declaration if such declaration exists in this scope.
  Decl *Lookup(const IdentifierInfo *IDInfo) const;

  /// Resolves an identifier by looking at this and parent scopes.
  Decl *Resolve(const IdentifierInfo *IDInfo) const;
};

/// The scope of a translation unit (a single file)
class TranslationUnitScope {
public:
  StmtLabelScope StmtLabels;
  ImplicitTypingScope ImplicitTypingRules;
};

/// The scope of an executable program unit.
class ExecutableProgramUnitScope {
public:
  StmtLabelScope StmtLabels;
  ImplicitTypingScope ImplicitTypingRules;
  BlockStmtBuilder Body;
};

/// The scope of a main program
class MainProgramScope : public ExecutableProgramUnitScope {
};

/// The scope of a function/subroutine
class SubProgramScope : public ExecutableProgramUnitScope {
};

}  // end namespace flang

#endif
