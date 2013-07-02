//===--- Expr.h - Fortran Expressions ---------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the expression objects.
//
//===----------------------------------------------------------------------===//

#ifndef FLANG_AST_EXPR_H__
#define FLANG_AST_EXPR_H__

#include "flang/AST/Type.h"
#include "flang/AST/IntrinsicFunctions.h"
#include "flang/Basic/SourceLocation.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/StringRef.h"
#include "flang/Basic/LLVM.h"

namespace flang {

class ASTContext;
class IdentifierInfo;
class Decl;
class VarDecl;
class FunctionDecl;
class SubroutineDecl;

/// Expr - Top-level class for expressions.
class Expr {
protected:
  enum ExprType {
    // Primary Expressions
    Designator,

    // Unary Expressions
    Constant,
    IntegerConstant,
    RealConstant,
    DoublePrecisionConstant,
    ComplexConstant,
    CharacterConstant,
    BOZConstant,
    LogicalConstant,
    RepeatedConstant,
    ImplicitCast,

    Variable,
    UnresolvedIdentifier,
    ReturnedValue,
    Unary,
    DefinedUnaryOperator,

    // Binary Expressions
    Binary,
    DefinedBinaryOperator,

    // Call Expressions
    Call,
    IntrinsicFunctionCall,

    //Other
    ImpliedDo
  };
private:
  QualType Ty;
  ExprType ExprID;
  SourceLocation Loc;
  friend class ASTContext;
protected:
  Expr(ExprType ET, QualType T, SourceLocation L) : ExprID(ET), Loc(L) {
    setType(T);
  }
public:
  QualType getType() const { return Ty; }
  void setType(QualType T) { Ty = T; }

  ExprType getExpressionID() const { return ExprID; }
  SourceLocation getLocation() const { return Loc; }

  virtual SourceLocation getLocStart() const { return Loc; }
  virtual SourceLocation getLocEnd() const { return Loc; }

  inline SourceRange getSourceRange() const {
    return SourceRange(getLocStart(), getLocEnd());
  }

  virtual void print(raw_ostream&);
  void dump();

  static bool classof(const Expr *) { return true; }
};

/// ConstantExpr - The base class for all constant expressions.
class ConstantExpr : public Expr {
  Expr *Kind;                   // Optional Kind Selector
  SourceLocation MaxLoc;
protected:
  ConstantExpr(ExprType Ty, QualType T, SourceLocation Loc, SourceLocation MLoc)
    : Expr(Ty, T, Loc), MaxLoc(MLoc), Kind(0) {}
public:
  Expr *getKindSelector() const { return Kind; }
  void setKindSelector(Expr *K) { Kind = K; }

  virtual SourceLocation getLocEnd() const;

  virtual void print(llvm::raw_ostream&);

  static bool classof(const Expr *E) {
    ExprType ETy = E->getExpressionID();
    return ETy == Expr::Constant || ETy == Expr::CharacterConstant ||
      ETy == Expr::IntegerConstant || ETy == Expr::RealConstant ||
      ETy == Expr::DoublePrecisionConstant || ETy == Expr::ComplexConstant ||
      ETy == Expr::BOZConstant || ETy == Expr::LogicalConstant;
  }
  static bool classof(const ConstantExpr *) { return true; }
};

/// \brief Used by {Integer,Real,BOZ}ConstantExpr to store the numeric without
/// leaking memory.
///
/// For large floats/integers, APFloat/APInt will allocate memory from the heap
/// to represent these numbers. Unfortunately, when we use a BumpPtrAllocator
/// to allocate IntegerLiteral/FloatingLiteral nodes the memory associated with
/// the APFloat/APInt values will never get freed. APNumericStorage uses
/// ASTContext's allocator for memory allocation.
class APNumericStorage {
  unsigned BitWidth;
  union {
    uint64_t VAL;    ///< Used to store the <= 64 bits integer value.
    uint64_t *pVal;  ///< Used to store the >64 bits integer value.
  };

  bool hasAllocation() const { return llvm::APInt::getNumWords(BitWidth) > 1; }

  APNumericStorage(const APNumericStorage&); // do not implement
  APNumericStorage& operator=(const APNumericStorage&); // do not implement

protected:
  APNumericStorage() : BitWidth(0), VAL(0) { }

  llvm::APInt getIntValue() const {
    unsigned NumWords = llvm::APInt::getNumWords(BitWidth);
    if (NumWords > 1)
      return llvm::APInt(BitWidth, NumWords, pVal);
    else
      return llvm::APInt(BitWidth, VAL);
  }
  void setIntValue(ASTContext &C, const llvm::APInt &Val);
};

class APIntStorage : public APNumericStorage {
public:  
  llvm::APInt getValue() const { return getIntValue(); } 
  void setValue(ASTContext &C, const llvm::APInt &Val) { setIntValue(C, Val); }
};

static inline const llvm::fltSemantics &
GetIEEEFloatSemantics(const llvm::APInt &api) {
  if (api.getBitWidth() == 16)
    return llvm::APFloat::IEEEhalf;
  else if (api.getBitWidth() == 32)
    return llvm::APFloat::IEEEsingle;
  else if (api.getBitWidth()==64)
    return llvm::APFloat::IEEEdouble;
  else if (api.getBitWidth()==128)
    return llvm::APFloat::IEEEquad;
  llvm_unreachable("Unknown float semantic.");
}

class APFloatStorage : public APNumericStorage {  
public:
  llvm::APFloat getValue() const {
    llvm::APInt Int = getIntValue();
    return llvm::APFloat(GetIEEEFloatSemantics(Int), Int);
  } 
  void setValue(ASTContext &C, const llvm::APFloat &Val) {
    setIntValue(C, Val.bitcastToAPInt());
  }
};

class IntegerConstantExpr : public ConstantExpr {
  APIntStorage Num;
  IntegerConstantExpr(ASTContext &C, SourceLocation Loc,
                      SourceLocation MaxLoc, llvm::StringRef Data);
public:
  static IntegerConstantExpr *Create(ASTContext &C, SourceLocation Loc,
                                     SourceLocation MaxLoc, llvm::StringRef Data);

  APInt getValue() const { return Num.getValue(); }

  virtual void print(llvm::raw_ostream&);

  static bool classof(const Expr *E) {
    return E->getExpressionID() == Expr::IntegerConstant;
  }
  static bool classof(const IntegerConstantExpr *) { return true; }
};

class RealConstantExpr : public ConstantExpr {
public:
  enum Kind {
    Kind4,
    Kind8,
    Kind16
  };
private:
  APFloatStorage Num;
  RealConstantExpr(ASTContext &C, SourceLocation Loc,
                   SourceLocation MaxLoc, llvm::StringRef Data,
                   Kind kind);
public:
  static RealConstantExpr *Create(ASTContext &C, SourceLocation Loc,
                                  SourceLocation MaxLoc, llvm::StringRef Data,
                                  Kind kind = Kind4);

  APFloat getValue() const { return Num.getValue(); }

  virtual void print(llvm::raw_ostream&);

  static bool classof(const Expr *E) {
    return E->getExpressionID() == Expr::RealConstant;
  }
  static bool classof(const RealConstantExpr *) { return true; }
};

class ComplexConstantExpr : public ConstantExpr {
public:
  enum Kind {
    Kind4,
    Kind8,
    Kind16
  };
private:
  APFloatStorage Re, Im;
  ComplexConstantExpr(ASTContext &C, SourceLocation Loc, SourceLocation MaxLoc,
                      const APFloat &Re, const APFloat &Im, Kind kind);
public:
  static ComplexConstantExpr *Create(ASTContext &C, SourceLocation Loc,
                                     SourceLocation MaxLoc,
                                     const APFloat &Re, const APFloat &Im,
                                     Kind kind = Kind4);

  APFloat getRealValue() const { return Re.getValue(); }
  APFloat getImaginaryValue() const { return Im.getValue(); }

  virtual void print(llvm::raw_ostream&);

  static bool classof(const Expr *E) {
    return E->getExpressionID() == Expr::ComplexConstant;
  }
  static bool classof(const ComplexConstantExpr *) { return true; }
};

class CharacterConstantExpr : public ConstantExpr {
  char *Data;
  CharacterConstantExpr(ASTContext &C, SourceLocation Loc,
                        SourceLocation MaxLoc, llvm::StringRef Data);
public:
  static CharacterConstantExpr *Create(ASTContext &C, SourceLocation Loc,
                                       SourceLocation MaxLoc, llvm::StringRef Data);

  const char *getValue() const { return Data; }

  virtual void print(llvm::raw_ostream&);

  static bool classof(const Expr *E) {
    return E->getExpressionID() == Expr::CharacterConstant;
  }
  static bool classof(const CharacterConstantExpr *) { return true; }
};

class BOZConstantExpr : public ConstantExpr {
public:
  enum BOZKind { Hexadecimal, Octal, Binary };
private:
  APIntStorage Num;
  BOZKind Kind;
  BOZConstantExpr(ASTContext &C, SourceLocation Loc,
                  SourceLocation MaxLoc, llvm::StringRef Data);
public:
  static BOZConstantExpr *Create(ASTContext &C, SourceLocation Loc,
                                 SourceLocation MaxLoc, llvm::StringRef Data);

  APInt getValue() const { return Num.getValue(); }

  BOZKind getBOZKind() const { return Kind; }

  bool isBinaryKind() const { return Kind == Binary; }
  bool isOctalKind() const { return Kind == Octal; }
  bool isHexKind() const { return Kind == Hexadecimal; }

  static bool classof(const Expr *E) {
    return E->getExpressionID() == Expr::BOZConstant;
  }
  static bool classof(const BOZConstantExpr *) { return true; }
};

class LogicalConstantExpr : public ConstantExpr {
  bool Val;
  LogicalConstantExpr(ASTContext &C, SourceLocation Loc,
                      SourceLocation MaxLoc, llvm::StringRef Data);
public:
  static LogicalConstantExpr *Create(ASTContext &C, SourceLocation Loc,
                                     SourceLocation MaxLoc, llvm::StringRef Data);

  bool isTrue() const { return Val; }
  bool isFalse() const { return !Val; }

  virtual void print(llvm::raw_ostream&);

  static bool classof(const Expr *E) {
    return E->getExpressionID() == Expr::LogicalConstant;
  }
  static bool classof(const LogicalConstantExpr *) { return true; }
};

/// This is a constant repeated several times,
/// for example in DATA statement - 15*0
class RepeatedConstantExpr : public Expr {
  IntegerConstantExpr *RepeatCount;
  Expr *E;
  RepeatedConstantExpr(SourceLocation Loc,
                       IntegerConstantExpr *Repeat,
                       Expr *Expression);
public:
  static RepeatedConstantExpr *Create(ASTContext &C, SourceLocation Loc,
                                      IntegerConstantExpr *RepeatCount,
                                      Expr *Expression);

  APInt getRepeatCount() const { return RepeatCount->getValue(); }
  Expr *getExpression() const { return E; }

  SourceLocation getLocStart() const;
  SourceLocation getLocEnd() const;

  virtual void print(llvm::raw_ostream&);

  static bool classof(const Expr *E) {
    return E->getExpressionID() == Expr::RepeatedConstant;
  }
  static bool classof(const RepeatedConstantExpr *) { return true; }
};


/// An expression with multiple arguments.
class MultiArgumentExpr {
private:
  unsigned NumArguments;
  union {
    Expr **Arguments;
    Expr *Argument;
  };
public:
  MultiArgumentExpr(ASTContext &C, ArrayRef<Expr*> Args);

  ArrayRef<Expr*> getArguments() const {
    return NumArguments == 1? ArrayRef<Expr*>(Argument) :
                              ArrayRef<Expr*>(Arguments,NumArguments);
  }
};

//===----------------------------------------------------------------------===//
/// DesignatorExpr -
class DesignatorExpr : public Expr {
public:
  enum DesignatorTy {
    ObjectName,
    ArrayElement,
    ArraySection,
    CoindexedNamedObject,
    ComplexPartDesignator,
    StructureComponent,
    Substring
  };
private:
  DesignatorTy Ty;
protected:
  DesignatorExpr(SourceLocation loc, QualType T, DesignatorTy ty)
    : Expr(Designator, T, loc), Ty(ty) {}
public:
  DesignatorTy getDesignatorType() const { return Ty; }

  virtual void print(llvm::raw_ostream&);

  static bool classof(const Expr *E) {
    return E->getExpressionID() == Expr::Designator;
  }
  static bool classof(const DesignatorExpr *) { return true; }
};

//===----------------------------------------------------------------------===//
/// SubstringExpr - Returns a substring.
class SubstringExpr : public DesignatorExpr {
private:
  Expr *Target, *StartingPoint, *EndPoint;
  SubstringExpr(ASTContext &C, SourceLocation Loc, Expr *E,
                Expr *Start, Expr *End);
public:
  static SubstringExpr *Create(ASTContext &C, SourceLocation Loc,
                               Expr *Target, Expr *StartingPoint,
                               Expr *EndPoint);

  Expr *getTarget() const { return Target; }
  Expr *getStartingPoint() const { return StartingPoint; }
  Expr *getEndPoint() const { return EndPoint; }

  SourceLocation getLocStart() const;
  SourceLocation getLocEnd() const;

  virtual void print(llvm::raw_ostream &);

  static bool classof(const Expr *E) {
    return E->getExpressionID() == Expr::Designator &&
      llvm::cast<DesignatorExpr>(E)->getDesignatorType() ==
      DesignatorExpr::Substring;
  }
  static bool classof(const DesignatorExpr *E) {
    return E->getDesignatorType() == DesignatorExpr::Substring;
  }
  static bool classof(const SubstringExpr *) { return true; }
};

//===----------------------------------------------------------------------===//
/// ArrayElementExpr - Returns an element of an array.
class ArrayElementExpr : public DesignatorExpr, public MultiArgumentExpr {
private:
  Expr *Target;

  ArrayElementExpr(ASTContext &C, SourceLocation Loc, Expr *E,
                   llvm::ArrayRef<Expr*> Subs);
public:
  static ArrayElementExpr *Create(ASTContext &C, SourceLocation Loc,
                                  Expr *Target,
                                  llvm::ArrayRef<Expr*> Subscripts);

  Expr *getTarget() const { return Target; }
  llvm::ArrayRef<Expr*> getSubscriptList() const {
    return getArguments();
  }

  SourceLocation getLocStart() const;
  SourceLocation getLocEnd() const;

  virtual void print(llvm::raw_ostream &);

  static bool classof(const Expr *E) {
    return E->getExpressionID() == Expr::Designator &&
      llvm::cast<DesignatorExpr>(E)->getDesignatorType() ==
      DesignatorExpr::ArrayElement;
  }
  static bool classof(const DesignatorExpr *E) {
    return E->getDesignatorType() == DesignatorExpr::ArrayElement;
  }
  static bool classof(const ArrayElementExpr *) { return true; }
};

//===----------------------------------------------------------------------===//
/// ArraySpec - The base class for all array specifications.
class ArraySpec {
public:
  enum ArraySpecKind {
    k_ExplicitShape,
    k_AssumedShape,
    k_DeferredShape,
    k_AssumedSize,
    k_ImpliedShape
  };
private:
  ArraySpecKind Kind;
  ArraySpec(const ArraySpec&);
  const ArraySpec &operator=(const ArraySpec&);
protected:
  ArraySpec(ArraySpecKind K);
public:
  ArraySpecKind getKind() const { return Kind; }

  virtual void print(llvm::raw_ostream &);

  static bool classof(const ArraySpec *) { return true; }
};

/// ExplicitShapeSpec - Used for an array whose shape is explicitly declared.
///
///   [R516]:
///     explicit-shape-spec :=
///         [ lower-bound : ] upper-bound
class ExplicitShapeSpec : public ArraySpec {
  Expr *LowerBound;
  Expr *UpperBound;

  ExplicitShapeSpec(Expr *LB, Expr *UB);
  ExplicitShapeSpec(Expr *UB);
public:
  static ExplicitShapeSpec *Create(ASTContext &C, Expr *UB);
  static ExplicitShapeSpec *Create(ASTContext &C, Expr *LB,
                                   Expr *UB);

  Expr *getLowerBound() const { return LowerBound; }
  Expr *getUpperBound() const { return UpperBound; }

  virtual void print(llvm::raw_ostream &);

  static bool classof(const ExplicitShapeSpec *) { return true; }
  static bool classof(const ArraySpec *AS) {
    return AS->getKind() == k_ExplicitShape;
  }
};

/// AssumedShapeSpec - An assumed-shape array is a nonallocatable nonpointer
/// dummy argument array that takes its shape from its effective arguments.
///
///   [R519]:
///     assumed-shape-spec :=
///         [ lower-bound ] :
class AssumedShapeSpec : public ArraySpec {
  Expr *LowerBound;

  AssumedShapeSpec();
  AssumedShapeSpec(Expr *LB);
public:
  static AssumedShapeSpec *Create(ASTContext &C);
  static AssumedShapeSpec *Create(ASTContext &C, Expr *LB);

  Expr *getLowerBound() const { return LowerBound; }

  static bool classof(const AssumedShapeSpec *) { return true; }
  static bool classof(const ArraySpec *AS) {
    return AS->getKind() == k_AssumedShape;
  }
};

/// DeferredShapeSpec - A deferred-shape array is an allocatable array or an
/// array pointer.
///
///   [R520]:
///     deferred-shape-spec :=
///         :
class DeferredShapeSpec : public ArraySpec {
  DeferredShapeSpec();
public:
  static DeferredShapeSpec *Create(ASTContext &C);

  static bool classof(const DeferredShapeSpec *) { return true; }
  static bool classof(const ArraySpec *AS) {
    return AS->getKind() == k_DeferredShape;
  }
};

/// AssumedSizeSpec - An assumed-size array is a dummy argument array whose size
/// is assumed from that of its effective argument.
///
///   [R521]:
///     assumed-size-spec :=
///         [ explicit-shape-spec , ]... [ lower-bound : ] *
class AssumedSizeSpec : public ArraySpec {
  // FIXME: Finish
public:
  static bool classof(const AssumedSizeSpec *) { return true; }
  static bool classof(const ArraySpec *AS) {
    return AS->getKind() == k_AssumedSize;
  }
};

/// ImpliedShapeSpec - An implied-shape array is a named constant taht takes its
/// shape from the constant-expr in its declaration.
///
///   [R522]:
///     implied-shape-spec :=
///         [ lower-bound : ] *
class ImpliedShapeSpec : public ArraySpec {
  SourceLocation Loc; // of *
  Expr *LowerBound;

  ImpliedShapeSpec(SourceLocation L);
  ImpliedShapeSpec(SourceLocation L, Expr *LB);
public:
  static ImpliedShapeSpec *Create(ASTContext &C, SourceLocation Loc);
  static ImpliedShapeSpec *Create(ASTContext &C, SourceLocation Loc, Expr *LB);

  SourceLocation getLocation() const { return Loc; }
  Expr *getLowerBound() const { return LowerBound; }

  virtual void print(llvm::raw_ostream &);

  static bool classof(const ImpliedShapeSpec *) { return true; }
  static bool classof(const ArraySpec *AS) {
    return AS->getKind() == k_ImpliedShape;
  }
};

/// VarExpr -
class VarExpr : public DesignatorExpr {
  const VarDecl *Variable;
  VarExpr(SourceLocation Loc, const VarDecl *Var);
public:
  static VarExpr *Create(ASTContext &C, SourceLocation L, const VarDecl *V);

  const VarDecl *getVarDecl() const { return Variable; }

  SourceLocation getLocEnd() const;

  virtual void print(llvm::raw_ostream&);

  static bool classof(const Expr *E) {
    return E->getExpressionID() == Expr::Designator &&
      llvm::cast<DesignatorExpr>(E)->getDesignatorType() ==
      DesignatorExpr::ObjectName;
  }
  static bool classof(const DesignatorExpr *E) {
    return E->getDesignatorType() == DesignatorExpr::ObjectName;
  }
  static bool classof(const VarExpr *) { return true; }
};

/// ReturnedValueExpr - used to assign to a return value
/// in functions.
class ReturnedValueExpr : public Expr {
  FunctionDecl *Func;
  ReturnedValueExpr(SourceLocation Loc, FunctionDecl *F);
public:
  static ReturnedValueExpr *Create(ASTContext &C, SourceLocation Loc,
                                   FunctionDecl *Func);

  SourceLocation getLocEnd() const;

  virtual void print(llvm::raw_ostream&);

  static bool classof(const Expr *E) {
    return E->getExpressionID() == ReturnedValue;
  }
  static bool classof(const ReturnedValueExpr *) { return true; }
};

/// UnresolvedIdentifierExpr - this is an probably a variable reference
/// to a variable that is declared later on in the source code.
///
/// Used in implied DO in the DATA statement.
class UnresolvedIdentifierExpr : public Expr {
  const IdentifierInfo *IDInfo;
  UnresolvedIdentifierExpr(ASTContext &C, SourceLocation Loc,
                           const IdentifierInfo *ID);
public:
  static UnresolvedIdentifierExpr *Create(ASTContext &C, SourceLocation Loc,
                                          const IdentifierInfo *IDInfo);
  const IdentifierInfo *getIdentifier() const { return IDInfo; }

  SourceLocation getLocEnd() const;

  virtual void print(llvm::raw_ostream&);

  static bool classof(const Expr *E) {
    return E->getExpressionID() == Expr::UnresolvedIdentifier;
  }
  static bool classof(const UnresolvedIdentifierExpr *) { return true; }
};

/// UnaryExpr -
class UnaryExpr : public Expr {
public:
  enum Operator {
    None,
    // Level-5 operand.
    Not,

    // Level-2 operands.
    Plus,
    Minus,

    // Level-1 operand.
    Defined
  };
protected:
  Operator Op;
  Expr *E;
  UnaryExpr(ExprType ET, QualType T, SourceLocation Loc, Operator op, Expr *e)
    : Expr(ET, T, Loc), Op(op), E(e) {}
public:
  static UnaryExpr *Create(ASTContext &C, SourceLocation Loc, Operator Op, Expr *E);

  Operator getOperator() const { return Op; }

  const Expr *getExpression() const { return E; }
  Expr *getExpression() { return E; }

  SourceLocation getLocEnd() const;

  virtual void print(llvm::raw_ostream&);

  static bool classof(const Expr *E) {
    return E->getExpressionID() == Expr::Unary;
  }
  static bool classof(const UnaryExpr *) { return true; }
};

/// DefinedOperatorUnaryExpr -
class DefinedOperatorUnaryExpr : public UnaryExpr {
  IdentifierInfo *II;
  DefinedOperatorUnaryExpr(SourceLocation Loc, Expr *E, IdentifierInfo *IDInfo);
public:
  static DefinedOperatorUnaryExpr *Create(ASTContext &C, SourceLocation Loc,
                                          Expr *E, IdentifierInfo *IDInfo);

  const IdentifierInfo *getIdentifierInfo() const { return II; }
  IdentifierInfo *getIdentifierInfo() { return II; }

  virtual void print(llvm::raw_ostream&);

  static bool classof(const Expr *E) {
    return E->getExpressionID() == Expr::DefinedUnaryOperator;
  }
  static bool classof(const DefinedOperatorUnaryExpr *) { return true; }
};

/// BinaryExpr -
class BinaryExpr : public Expr {
public:
  enum Operator {
    None,

    // Level-5 operators
    Eqv,
    Neqv,
    Or,
    And,
    Defined,

    // Level-4 operators
    Equal,
    NotEqual,
    LessThan,
    LessThanEqual,
    GreaterThan,
    GreaterThanEqual,

    // Level-3 operator
    Concat,

    // Level-2 operators
    Plus,
    Minus,
    Multiply,
    Divide,
    Power
  };
protected:
  Operator Op;
  Expr *LHS, *RHS;
  BinaryExpr(ExprType ET, QualType T, SourceLocation Loc, Operator op,
             Expr *lhs, Expr *rhs)
    : Expr(ET, T, Loc), Op(op), LHS(lhs), RHS(rhs) {}
public:
  static BinaryExpr *Create(ASTContext &C, SourceLocation Loc, Operator Op,
                            QualType Type, Expr *LHS, Expr *RHS);

  Operator getOperator() const { return Op; }

  const Expr *getLHS() const { return LHS; }
  Expr *getLHS() { return LHS; }
  const Expr *getRHS() const { return RHS; }
  Expr *getRHS() { return RHS; }

  SourceLocation getLocStart() const;
  SourceLocation getLocEnd() const;

  virtual void print(llvm::raw_ostream&);

  static bool classof(const Expr *E) {
    return E->getExpressionID() == Expr::Binary;
  }
  static bool classof(const BinaryExpr *) { return true; }
};

/// DefinedOperatorBinaryExpr -
class DefinedOperatorBinaryExpr : public BinaryExpr {
  IdentifierInfo *II;
  DefinedOperatorBinaryExpr(SourceLocation Loc, Expr *LHS, Expr *RHS,
                            IdentifierInfo *IDInfo)
    // FIXME: The type here needs to be calculated.
    : BinaryExpr(Expr::DefinedBinaryOperator, QualType(), Loc, Defined,
                 LHS, RHS), II(IDInfo) {}
public:
  static DefinedOperatorBinaryExpr *Create(ASTContext &C, SourceLocation Loc,
                                           Expr *LHS, Expr *RHS,
                                           IdentifierInfo *IDInfo);

  const IdentifierInfo *getIdentifierInfo() const { return II; }
  IdentifierInfo *getIdentifierInfo() { return II; }

  virtual void print(llvm::raw_ostream&);

  static bool classof(const Expr *E) {
    return E->getExpressionID() == Expr::DefinedBinaryOperator;
  }
  static bool classof(const DefinedOperatorBinaryExpr *) { return true; }
};

/// ImplicitCastExpr - Allows us to explicitly represent implicit type
/// conversions, which have no direct representation in the original
/// source code.
///
/// = INT(x, Kind) | REAL(x, Kind) | CMPLX(x, Kind)
/// NB: For the sake of Fortran 77 compability, REAL(x, 8) can
///     be used like DBLE.
/// NB: Kind is specified in the expression's type.
class ImplicitCastExpr : public Expr {
  Expr *E;
  ImplicitCastExpr(SourceLocation Loc, QualType Dest, Expr *e);
public:
  static ImplicitCastExpr *Create(ASTContext &C, SourceLocation Loc,
                                  QualType Dest, Expr *E);

  Expr *getExpression() { return E; }

  SourceLocation getLocStart() const;
  SourceLocation getLocEnd() const;

  virtual void print(llvm::raw_ostream&);

  static bool classof(const Expr *E) {
    return E->getExpressionID() == Expr::ImplicitCast;
  }
  static bool classof(const ImplicitCastExpr *) { return true; }
};

/// CallExpr - represents a call to a function.
class CallExpr : public Expr, public MultiArgumentExpr {
  FunctionDecl *Function;
  CallExpr(ASTContext &C, SourceLocation Loc,
           FunctionDecl *Func, ArrayRef<Expr*> Args);
public:
  static CallExpr *Create(ASTContext &C, SourceLocation Loc,
                          FunctionDecl *Func, ArrayRef<Expr*> Args);

  FunctionDecl *getFunction() const { return Function; }

  SourceLocation getLocEnd() const;

  virtual void print(llvm::raw_ostream&);

  static bool classof(const Expr *E) {
    return E->getExpressionID() == Call;
  }
  static bool classof(const CallExpr *) { return true; }
};

/// IntrinsicFunctionCallExpr - represents a call to an intrinsic function
class IntrinsicFunctionCallExpr : public Expr, public MultiArgumentExpr {
  intrinsic::FunctionKind Function;
  IntrinsicFunctionCallExpr(ASTContext &C, SourceLocation Loc,
                            intrinsic::FunctionKind Func,
                            ArrayRef<Expr*> Args,
                            QualType ReturnType);
public:
  static IntrinsicFunctionCallExpr *Create(ASTContext &C, SourceLocation Loc,
                                           intrinsic::FunctionKind Func,
                                           ArrayRef<Expr*> Arguments,
                                           QualType ReturnType);

  intrinsic::FunctionKind getIntrinsicFunction() const { return Function;  }

  SourceLocation getLocEnd() const;

  virtual void print(llvm::raw_ostream&);

  static bool classof(const Expr *E) {
    return E->getExpressionID() == Expr::IntrinsicFunctionCall;
  }
  static bool classof(const IntrinsicFunctionCallExpr *) { return true; }
};

/// ImpliedDoExpr - represents an implied do in a DATA statement
class ImpliedDoExpr : public Expr {
  VarDecl *DoVar;
  MultiArgumentExpr DoList;
  Expr *Init, *Terminate, *Increment;

  ImpliedDoExpr(ASTContext &C, SourceLocation Loc,
                VarDecl *Var, ArrayRef<Expr*> Body,
                Expr *InitialParam, Expr *TerminalParam,
                Expr *IncrementationParam);
public:
  static ImpliedDoExpr *Create(ASTContext &C, SourceLocation Loc,
                               VarDecl *DoVar, ArrayRef<Expr*> Body,
                               Expr *InitialParam, Expr *TerminalParam,
                               Expr *IncrementationParam);

  VarDecl *getVarDecl() const { return DoVar; }
  ArrayRef<Expr*> getBody() const { return DoList.getArguments(); }
  Expr *getInitialParameter() const { return Init; }
  Expr *getTerminalParameter() const { return Terminate; }
  Expr *getIncrementationParameter() const { return Increment; }

  SourceLocation getLocEnd() const;

  virtual void print(llvm::raw_ostream &);

  static bool classof(const Expr *E) {
    return E->getExpressionID() == ImpliedDo;
  }
  static bool classof(const IntrinsicFunctionCallExpr *) { return true; }
};

} // end flang namespace

#endif
