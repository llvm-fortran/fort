//===--- Stmt.h - Fortran Statements ----------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the statement objects.
//
//===----------------------------------------------------------------------===//

#ifndef FLANG_AST_STMT_H__
#define FLANG_AST_STMT_H__

#include "flang/AST/ASTContext.h"
#include "flang/Sema/Ownership.h"
#include "flang/Basic/Token.h"
#include "flang/Basic/SourceLocation.h"
#include "llvm/ADT/ArrayRef.h"
#include "flang/Basic/LLVM.h"

namespace flang {

class Expr;
class VarExpr;
class FormatSpec;
class IdentifierInfo;

/// Stmt - The base class for all Fortran statements.
///
class Stmt {
public:
  enum StmtTy {
    DeclStmtKind,
    BundledCompound,
    Program,

    // Specification Part
    Use,
    Import,
    Dimension,

    // Implicit Part
    Implicit,
    Format,
    Entry,

    Asynchronous,
    External,
    Intrinsic,
    Data,
    EndProgram,

    // Action Statements
    Block,
    Assign,
    AssignedGoto,
    Goto,
    If,
    Else,
    EndIf,
    Do,
    EndDo,
    Continue,
    Stop,
    Assignment,
    Print
  };
private:
  StmtTy StmtID;
  SourceLocation Loc;
  Expr *StmtLabel;

  Stmt(const Stmt &);           // Do not implement!
  friend class ASTContext;
protected:
  // Make vanilla 'new' and 'delete' illegal for Stmts.
  void* operator new(size_t bytes) throw() {
    assert(0 && "Stmts cannot be allocated with regular 'new'.");
    return 0;
  }
  void operator delete(void* data) throw() {
    assert(0 && "Stmts cannot be released with regular 'delete'.");
  }

  Stmt(StmtTy ID, SourceLocation L, Expr *SLT)
    : StmtID(ID), Loc(L), StmtLabel(SLT) {}
public:
  virtual ~Stmt();

  /// Creates a statement that does nothing
  static Stmt *Create(ASTContext &C, StmtTy StmtType,
                      SourceLocation Loc, Expr *StmtLabel);

  /// getStatementID - Get the ID of the statement.
  StmtTy getStatementID() const { return StmtID; }

  /// getLocation - Get the location of the statement.
  SourceLocation getLocation() const { return Loc; }

  virtual SourceLocation getLocStart() const { return Loc; }
  virtual SourceLocation getLocEnd() const { return Loc; }

  inline SourceRange getSourceRange() const {
    return SourceRange(getLocStart(), getLocEnd());
  }

  /// getStmtLabel - Get the statement label for this statement.
  Expr *getStmtLabel() const { return StmtLabel; }

  void setStmtLabel(Expr *E) {
    assert(!StmtLabel);
    StmtLabel = E;
  }

  static bool classof(const Stmt*) { return true; }

public:
  // Only allow allocation of Stmts using the allocator in ASTContext or by
  // doing a placement new.
  void *operator new(size_t bytes, ASTContext &C,
                     unsigned alignment = 8) throw() {
    return ::operator new(bytes, C, alignment);
  }

  void *operator new(size_t bytes, ASTContext *C,
                     unsigned alignment = 8) throw() {
    return ::operator new(bytes, *C, alignment);
  }

  void *operator new(size_t bytes, void *mem) throw() {
    return mem;
  }

  void operator delete(void*, ASTContext&, unsigned) throw() { }
  void operator delete(void*, ASTContext*, unsigned) throw() { }
  void operator delete(void*, std::size_t) throw() { }
  void operator delete(void*, void*) throw() { }
};

/// DeclStmt - Adaptor class for mixing declarations with statements and
/// expressions.
///
class DeclStmt : public Stmt {
  NamedDecl *Declaration;

  DeclStmt(SourceLocation Loc, NamedDecl *Decl, Expr *StmtLabel);
public:
  static DeclStmt *Create(ASTContext &C, SourceLocation Loc,
                          NamedDecl *Declaration, Expr *StmtLabel);

  NamedDecl *getDeclaration() const {
    return Declaration;
  }

  static bool classof(const DeclStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStatementID() == DeclStmtKind;
  }
};

/// ListStmt - A statement which has a list of identifiers associated with it.
///
template <typename T = const IdentifierInfo *>
class ListStmt : public Stmt {
  unsigned NumIDs;
  T *IDList;
protected:
  ListStmt(ASTContext &C, Stmt::StmtTy ID, SourceLocation L, ArrayRef<T> IDs,
           Expr *SLT)
    : Stmt(ID, L, SLT) {
    NumIDs = IDs.size();
    IDList = new (C) T [NumIDs];

    for (unsigned I = 0; I != NumIDs; ++I)
      IDList[I] = IDs[I];
  }
  T *getMutableList() {
    return IDList;
  }
public:
  ArrayRef<T> getIDList() const {
    return ArrayRef<T>(IDList, NumIDs);
  }
};

/// BundledCompoundStmt - This represents a group of statements
/// that are bundled together in the source code under one keyword.
///
/// For example, the code PARAMETER (x=1, y=2) will create an AST like:
///   BundledCompoundStmt {
///     ParameterStmt { x = 1 }
///     ParameterStmt { y = 2 }
///   }
class BundledCompoundStmt : public ListStmt<Stmt*> {
  BundledCompoundStmt(ASTContext &C, SourceLocation Loc,
                      ArrayRef<Stmt*> Body, Expr *StmtLabel);
public:
  static BundledCompoundStmt *Create(ASTContext &C, SourceLocation Loc,
                                     ArrayRef<Stmt*> Body, Expr *StmtLabel);

  ArrayRef<Stmt*> getBody() const {
    return getIDList();
  }
  Stmt *getFirst() const {
    auto Body = getBody();
    if(auto BC = dyn_cast<BundledCompoundStmt>(Body.front()))
      return BC->getFirst();
    return Body.front();
  }

  static bool classof(const BundledCompoundStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStatementID() == BundledCompound;
  }
};

/// ProgramStmt - The (optional) first statement of the 'main' program.
///
class ProgramStmt : public Stmt {
  const IdentifierInfo *ProgName;
  SourceLocation NameLoc;

  ProgramStmt(const IdentifierInfo *progName, SourceLocation Loc,
              SourceLocation NameL, Expr *SLT)
    : Stmt(Program, Loc, SLT), ProgName(progName), NameLoc(NameL) {}
  ProgramStmt(const ProgramStmt &); // Do not implement!
public:
  static ProgramStmt *Create(ASTContext &C, const IdentifierInfo *ProgName,
                             SourceLocation L, SourceLocation NameL,
                             Expr *StmtLabel);

  /// getProgramName - Get the name of the program. This may be null.
  const IdentifierInfo *getProgramName() const { return ProgName; }

  /// getNameLocation - Get the location of the program name.
  SourceLocation getNameLocation() const { return NameLoc; }

  static bool classof(const ProgramStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStatementID() == Program;
  }
};

/// EndProgramStmt - The last statement of the 'main' program.
///
class EndProgramStmt : public Stmt {
  const IdentifierInfo *ProgName;
  SourceLocation NameLoc;

  EndProgramStmt(const IdentifierInfo *progName, SourceLocation Loc,
                 SourceLocation NameL, Expr *SLT)
    : Stmt(EndProgram, Loc, SLT), ProgName(progName), NameLoc(NameL) {}
  EndProgramStmt(const EndProgramStmt &); // Do not implement!
public:
  static EndProgramStmt *Create(ASTContext &C, const IdentifierInfo *ProgName,
                                SourceLocation L, SourceLocation NameL,
                                Expr *StmtLabel);

  /// getProgramName - Get the name of the program. This may be null.
  const IdentifierInfo *getProgramName() const { return ProgName; }

  /// getNameLocation - Get the location of the program name.
  SourceLocation getNameLocation() const { return NameLoc; }

  static bool classof(const EndProgramStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStatementID() == EndProgram;
  }
};


//===----------------------------------------------------------------------===//
// Specification Part Statements
//===----------------------------------------------------------------------===//

/// UseStmt - A reference to the module it specifies.
///
class UseStmt : public ListStmt<std::pair<const IdentifierInfo *,
                                          const IdentifierInfo *> > {
public:
  enum ModuleNature {
    None,
    Intrinsic,
    NonIntrinsic
  };
  typedef std::pair<const IdentifierInfo *, const IdentifierInfo *> RenamePair;
private:
  ModuleNature ModNature;
  const IdentifierInfo *ModName;
  bool Only;

  UseStmt(ASTContext &C, ModuleNature MN, const IdentifierInfo *modName,
          ArrayRef<RenamePair> RenameList, Expr *StmtLabel);

  void init(ASTContext &C, ArrayRef<RenamePair> RenameList);
public:
  static UseStmt *Create(ASTContext &C, ModuleNature MN,
                         const IdentifierInfo *modName,
                         Expr *StmtLabel);
  static UseStmt *Create(ASTContext &C, ModuleNature MN,
                         const IdentifierInfo *modName, bool Only,
                         ArrayRef<RenamePair> RenameList,
                         Expr *StmtLabel);

  /// Accessors:
  ModuleNature getModuleNature() const { return ModNature; }
  StringRef getModuleName() const;

  static bool classof(const UseStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStatementID() == Use;
  }
};

/// ImportStmt - Specifies that the named entities from the host scoping unit
/// are accessible in the interface body by host association.
///
class ImportStmt : public ListStmt<> {
  ImportStmt(ASTContext &C, SourceLocation Loc, ArrayRef<const IdentifierInfo*> names,
             Expr *StmtLabel);
public:
  static ImportStmt *Create(ASTContext &C, SourceLocation Loc,
                            ArrayRef<const IdentifierInfo*> Names,
                            Expr *StmtLabel);

  static bool classof(const ImportStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStatementID() == Import;
  }
};

/// ImplicitStmt - Specifies a type, and possibly type parameters, for all
/// implicitly typed data entries whose names begin with one of the letters
/// specified in the statement.
///
class ImplicitStmt : public Stmt {
public:
  typedef std::pair<const IdentifierInfo *, const IdentifierInfo *> LetterSpecTy;
private:
  QualType Ty;
  LetterSpecTy LetterSpec;
  bool None;

  ImplicitStmt(SourceLocation Loc, Expr *StmtLabel);
  ImplicitStmt(SourceLocation Loc, QualType T,
               LetterSpecTy Spec, Expr *StmtLabel);
public:
  static ImplicitStmt *Create(ASTContext &C, SourceLocation Loc, Expr *StmtLabel);
  static ImplicitStmt *Create(ASTContext &C, SourceLocation Loc, QualType T,
                              LetterSpecTy LetterSpec,
                              Expr *StmtLabel);

  bool isNone() const { return None; }

  QualType getType() const { return Ty; }
  LetterSpecTy getLetterSpec() const { return LetterSpec; }

  static bool classof(const ImplicitStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStatementID() == Implicit;
  }
};

/// DimensionStmt - Specifies the DIMENSION attribute for a named constant.
///
class DimensionStmt : public ListStmt<ArrayType::Dimension> {
  const IdentifierInfo *VarName;

  DimensionStmt(ASTContext &C, SourceLocation Loc, const IdentifierInfo* IDInfo,
                 ArrayRef<ArrayType::Dimension> Dims,
                 Expr *StmtLabel);
public:
  static DimensionStmt *Create(ASTContext &C, SourceLocation Loc,
                               const IdentifierInfo* IDInfo,
                               ArrayRef<ArrayType::Dimension> Dims,
                               Expr *StmtLabel);

  const IdentifierInfo *getVariableName() const {
    return VarName;
  }

  SourceLocation getLocEnd() const;

  static bool classof(const DimensionStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStatementID() == Dimension;
  }
};

class FormatItemList;

/// FormatStmt -
///
class FormatStmt : public Stmt {
  FormatItemList *Items;
  FormatItemList *UnlimitedItems;

  FormatStmt(SourceLocation Loc, FormatItemList *ItemList,
             FormatItemList *UnlimitedItemList, Expr *StmtLabel);
public:
  static FormatStmt *Create(ASTContext &C, SourceLocation Loc,
                            FormatItemList *ItemList,
                            FormatItemList *UnlimitedItemList,
                            Expr *StmtLabel);

  FormatItemList *getItemList() const {
    return Items;
  }
  FormatItemList *getUnlimitedItemList() const {
    return UnlimitedItems;
  }

  static bool classof(const FormatStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStatementID() == Format;
  }
};

/// EntryStmt -
///
class EntryStmt : public Stmt {
  EntryStmt(SourceLocation Loc, Expr *StmtLabel);
public:
  static EntryStmt *Create(ASTContext &C, SourceLocation Loc, Expr *StmtLabel);

  static bool classof(const EntryStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStatementID() == Entry;
  }
};

/// AsynchronousStmt - Specifies the asynchronous attribute for a list of
/// objects.
///
class AsynchronousStmt : public ListStmt<> {
  AsynchronousStmt(ASTContext &C, SourceLocation Loc,
                   ArrayRef<const IdentifierInfo*> objNames,
                   Expr *StmtLabel);
public:
  static AsynchronousStmt *Create(ASTContext &C, SourceLocation Loc,
                                  ArrayRef<const IdentifierInfo*> objNames,
                                  Expr *StmtLabel);

  static bool classof(const AsynchronousStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStatementID() == Asynchronous;
  }
};

/// ExternalStmt - Specifies the external attribute for a list of objects.
///
class ExternalStmt : public ListStmt<> {
  ExternalStmt(ASTContext &C, SourceLocation Loc,
               ArrayRef<const IdentifierInfo *> ExternalNames,
               Expr *StmtLabel);
public:
  static ExternalStmt *Create(ASTContext &C, SourceLocation Loc,
                              ArrayRef<const IdentifierInfo*> ExternalNames,
                              Expr *StmtLabel);

  static bool classof(const ExternalStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStatementID() == External;
  }
};


/// IntrinsicStmt - Lists the intrinsic functions declared in this program unit.
///
class IntrinsicStmt : public ListStmt<> {
  IntrinsicStmt(ASTContext &C, SourceLocation Loc,
                ArrayRef<const IdentifierInfo *> IntrinsicNames,
                Expr *StmtLabel);
public:
  static IntrinsicStmt *Create(ASTContext &C, SourceLocation Loc,
                               ArrayRef<const IdentifierInfo*> IntrinsicNames,
                               Expr *StmtLabel);

  static bool classof(const IntrinsicStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStatementID() == Intrinsic;
  }
};

/// DataStmt - this is a part of the DATA statement
class DataStmt : public Stmt {
  unsigned NumNames;
  unsigned NumValues;
  Expr **NameList, **ValueList;

  DataStmt(ASTContext &C, SourceLocation Loc,
           ArrayRef<Expr*> Names,
           ArrayRef<Expr*> Values,
           Expr *StmtLabel);
public:
  static DataStmt *Create(ASTContext &C, SourceLocation Loc,
                          ArrayRef<Expr*> Names,
                          ArrayRef<Expr*> Values,
                          Expr *StmtLabel);

  ArrayRef<Expr*> getNames() const {
    return ArrayRef<Expr*>(NameList, NumNames);
  }
  ArrayRef<Expr*> getValues() const {
    return ArrayRef<Expr*>(ValueList, NumValues);
  }

  static bool classof(const DataStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStatementID() == Data;
  }
};

//===----------------------------------------------------------------------===//
// Executable Statements
//===----------------------------------------------------------------------===//

/// BlockStmt
class BlockStmt : public ListStmt<StmtResult> {
  BlockStmt(ASTContext &C, SourceLocation Loc,
            ArrayRef<StmtResult> Body);
public:
  static BlockStmt *Create(ASTContext &C, SourceLocation Loc,
                           ArrayRef<StmtResult> Body);

  static bool classof(const BlockStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStatementID() == Block;
  }
};

/// StmtLabelInteger - an integer big enough to hold the value
/// of a statement label.
typedef uint32_t StmtLabelInteger;

/// StmtLabelReference - a reference to a statement label
struct StmtLabelReference {
  Stmt *Statement;

  StmtLabelReference()
    : Statement(nullptr) {
  }
  inline StmtLabelReference(Stmt *S)
    : Statement(S) {
    assert(S);
  }
};

/// AssignStmt - assigns a statement label to an integer variable.
class AssignStmt : public Stmt {
  StmtLabelReference Address;
  Expr *Destination;
  AssignStmt(SourceLocation Loc, StmtLabelReference Addr, Expr *Dest,
             Expr *StmtLabel);
public:
  static AssignStmt *Create(ASTContext &C, SourceLocation Loc,
                            StmtLabelReference Address,
                            Expr *Destination,
                            Expr *StmtLabel);

  inline StmtLabelReference getAddress() const {
    return Address;
  }
  void setAddress(StmtLabelReference Address);
  inline Expr *getDestination() const {
    return Destination;
  }

  static bool classof(const AssignStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStatementID() == Assign;
  }
};

/// AssignedGotoStmt - jump to a position determined by an integer
/// variable.
class AssignedGotoStmt : public ListStmt<StmtLabelReference> {
  Expr *Destination;
  AssignedGotoStmt(ASTContext &C, SourceLocation Loc, Expr *Dest,
                   ArrayRef<StmtLabelReference> Vals,
                   Expr *StmtLabel);
public:
  static AssignedGotoStmt *Create(ASTContext &C, SourceLocation Loc,
                                  Expr *Destination,
                                  ArrayRef<StmtLabelReference> AllowedValues,
                                  Expr *StmtLabel);

  inline Expr *getDestination() const {
    return Destination;
  }
  inline ArrayRef<StmtLabelReference> getAllowedValues() const {
    return getIDList();
  }
  void setAllowedValue(size_t I, StmtLabelReference Address);

  static bool classof(const AssignedGotoStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStatementID() == AssignedGoto;
  }
};

/// GotoStmt - an unconditional jump
class GotoStmt : public Stmt {
  StmtLabelReference Destination;
  GotoStmt(SourceLocation Loc, StmtLabelReference Dest, Expr *StmtLabel);
public:
  static GotoStmt *Create(ASTContext &C, SourceLocation Loc,
                          StmtLabelReference Destination,
                          Expr *StmtLabel);

  inline StmtLabelReference getDestination() const {
    return Destination;
  }
  void setDestination(StmtLabelReference Destination);

  static bool classof(const GotoStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStatementID() == Goto;
  }
};

/// IfStmt
/// An if statement is also a control flow statement
class IfStmt : public Stmt {
  Expr *Condition;
  Stmt *ThenArm, *ElseArm;

  IfStmt(SourceLocation Loc, Expr *Cond, Expr *StmtLabel);
public:
  static IfStmt *Create(ASTContext &C, SourceLocation Loc,
                        Expr *Condition, Expr *StmtLabel);

  inline Expr *getCondition() const { return Condition; }
  inline Stmt *getThenStmt() const { return ThenArm; }
  inline Stmt *getElseStmt() const { return ElseArm; }
  void setThenStmt(Stmt *Body);
  void setElseStmt(Stmt *Body);

  static bool classof(const IfStmt*) { return true; }
  static bool classof(const Stmt *S){
    return S->getStatementID() == If;
  }
};

/// A base class for statements with own body which
/// the program will execute when entering this body.
class CFBlockStmt : public Stmt {
  Stmt *Body;
protected:
  CFBlockStmt(StmtTy Type, SourceLocation Loc, Expr *StmtLabel);
public:
  Stmt *getBody() const { return Body; }
  void setBody(Stmt *Body);
};

/// DoStmt
class DoStmt : public CFBlockStmt {
  StmtLabelReference TerminatingStmt;
  Expr *DoVar;
  Expr *Init, *Terminate, *Increment;

  DoStmt(SourceLocation Loc, StmtLabelReference TermStmt, Expr *DoVariable,
         Expr *InitialParam, Expr *TerminalParam,
         Expr *IncrementationParam,Expr *StmtLabel);
public:
  static DoStmt *Create(ASTContext &C,SourceLocation Loc, StmtLabelReference TermStmt,
                        Expr *DoVariable, Expr *InitialParam,
                        Expr *TerminalParam,Expr *IncrementationParam,
                        Expr *StmtLabel);

  StmtLabelReference getTerminatingStmt() const { return TerminatingStmt; }
  void setTerminatingStmt(StmtLabelReference Stmt);
  Expr *getDoVar() const { return DoVar; }
  Expr *getInitialParameter() const { return Init; }
  Expr *getTerminalParameter() const { return Terminate; }
  Expr *getIncrementationParameter() const { return Increment; }

  static bool classof(const DoStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStatementID() == Do;
  }
};

/// ContinueStmt
class ContinueStmt : public Stmt {
  ContinueStmt(SourceLocation Loc, Expr *StmtLabel);
public:
  static ContinueStmt *Create(ASTContext &C, SourceLocation Loc, Expr *StmtLabel);

  static bool classof(const ContinueStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStatementID() == Continue;
  }
};

/// StopStmt
class StopStmt : public Stmt {
  Expr *StopCode;

  StopStmt(SourceLocation Loc, Expr *stopCode, Expr *StmtLabel);
public:
  static StopStmt *Create(ASTContext &C, SourceLocation Loc, Expr *stopCode, Expr *StmtLabel);

  Expr *getStopCode() const { return StopCode; }

  static bool classof(const StopStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStatementID() == Stop;
  }
};

/// AssignmentStmt
class AssignmentStmt : public Stmt {
  Expr *LHS;
  Expr *RHS;

  AssignmentStmt(SourceLocation Loc, Expr *lhs, Expr *rhs, Expr *StmtLabel);
public:
  static AssignmentStmt *Create(ASTContext &C, SourceLocation Loc, Expr *LHS,
                                Expr *RHS, Expr *StmtLabel);

  Expr *getLHS() const { return LHS; }
  Expr *getRHS() const { return RHS; }

  static bool classof(const AssignmentStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStatementID() == Assignment;
  }
};

/// PrintStmt
class PrintStmt : public ListStmt<ExprResult> {
  FormatSpec *FS;
  PrintStmt(ASTContext &C, SourceLocation L, FormatSpec *fs,
            ArrayRef<ExprResult> OutList, Expr *StmtLabel);
public:
  static PrintStmt *Create(ASTContext &C, SourceLocation L, FormatSpec *fs,
                           ArrayRef<ExprResult> OutList, Expr *StmtLabel);

  FormatSpec *getFormatSpec() const { return FS; }

  static bool classof(const PrintStmt*) { return true; }
  static bool classof(const Stmt *S) {
    return S->getStatementID() == Print;
  }
};

} // end flang namespace

#endif
