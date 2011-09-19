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
#include "flang/Sema/Ownership.h"
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

//===----------------------------------------------------------------------===//
/// Expr -
class Expr {
protected:
  enum ExprType {
    // Primary Expressions
    Designator,

    // Unary Expressions
    Constant,
    IntegerConstant,
    RealConstant,
    CharacterConstant,
    BOZConstant,
    LogicalConstant,

    Variable,
    Unary,
    DefinedUnaryOperator,

    // Binary Expressions
    Binary,
    DefinedBinaryOperator
  };
private:
  QualType Ty;
  ExprType ExprID;
  SMLoc Loc;
  friend class ASTContext;
protected:
  Expr(ExprType ET, QualType T, SMLoc L) : ExprID(ET), Loc(L) {
    setType(T);
  }
public:
  QualType getType() const { return Ty; }
  void setType(QualType T) { Ty = T; }

  ExprType getExpressionID() const { return ExprID; }
  SMLoc getLocation() const { return Loc; }

  virtual void print(raw_ostream&);
  void dump();

  static bool classof(const Expr *) { return true; }
};

//===----------------------------------------------------------------------===//
/// ConstantExpr -
class ConstantExpr : public Expr {
  char *Kind;                   // Optional Kind Selector
protected:
  ConstantExpr(ExprType Ty, QualType T, SMLoc Loc)
    : Expr(Ty, T, Loc), Kind(0) {}
  
  void setKindSelector(ASTContext &C, StringRef K);
public:
  const char *getKindSelector() const { return Kind; }

  virtual void print(llvm::raw_ostream&);

  static bool classof(const Expr *E) {
    ExprType ETy = E->getExpressionID();
    return ETy == Expr::Constant || ETy == Expr::CharacterConstant ||
      ETy == Expr::IntegerConstant || ETy == Expr::RealConstant ||
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

class APFloatStorage : public APNumericStorage {
public:  
  llvm::APFloat getValue() const { return llvm::APFloat(getIntValue()); } 
  void setValue(ASTContext &C, const llvm::APFloat &Val) {
    setIntValue(C, Val.bitcastToAPInt());
  }
};

class IntegerConstantExpr : public ConstantExpr {
  APIntStorage Num;
  IntegerConstantExpr(ASTContext &C, SMLoc Loc, StringRef Data);
public:
  static IntegerConstantExpr *Create(ASTContext &C, SMLoc Loc,
                                     StringRef Data);

  APInt getValue() const { return Num.getValue(); }

  static bool classof(const Expr *E) {
    return E->getExpressionID() == Expr::IntegerConstant;
  }
  static bool classof(const IntegerConstantExpr *) { return true; }
};

class RealConstantExpr : public ConstantExpr {
  APFloatStorage Num;
  RealConstantExpr(ASTContext &C, SMLoc Loc, StringRef Data);
public:
  static RealConstantExpr *Create(ASTContext &C, SMLoc Loc, StringRef Data);

  APFloat getValue() const { return Num.getValue(); }

  static bool classof(const Expr *E) {
    return E->getExpressionID() == Expr::RealConstant;
  }
  static bool classof(const RealConstantExpr *) { return true; }
};

class CharacterConstantExpr : public ConstantExpr {
  char *Data;
  CharacterConstantExpr(ASTContext &C, SMLoc Loc, StringRef Data);
public:
  static CharacterConstantExpr *Create(ASTContext &C, SMLoc Loc,
                                       StringRef Data);

  const char *getValue() const { return Data; }

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
  BOZConstantExpr(ASTContext &C, SMLoc Loc, StringRef Data);
public:
  static BOZConstantExpr *Create(ASTContext &C, SMLoc Loc,
                                 StringRef Data);

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
  LogicalConstantExpr(ASTContext &C, SMLoc Loc, StringRef Data);
public:
  static LogicalConstantExpr *Create(ASTContext &C, SMLoc Loc,
                                     StringRef Data);

  bool isTrue() const { return Val; }
  bool isFalse() const { return !Val; }

  static bool classof(const Expr *E) {
    return E->getExpressionID() == Expr::LogicalConstant;
  }
  static bool classof(const LogicalConstantExpr *) { return true; }
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
  DesignatorExpr(SMLoc loc, QualType T, DesignatorTy ty)
    : Expr(Expr::Designator, T, loc), Ty(ty) {}
public:
  DesignatorTy getDesignatorType() const { return Ty; }

  virtual void print(llvm::raw_ostream&);

  static bool classof(const Expr *E) {
    return E->getExpressionID() == Expr::Designator;
  }
  static bool classof(const DesignatorExpr *) { return true; }
};

////////////////////////////////////////////////////////////////////////////////
// FIXME: Should this go somewhere else?

class Subscript {
public:
  enum SubscriptTy { Normal = 0, EmptyRange = ':', Splat = '*' };
private:
  SubscriptTy Ty;
  Expr *Val;
  Subscript(SubscriptTy ty, Expr *e = 0)
    : Ty(ty), Val(e) {}
  Subscript(const Subscript&);  // Don't implement.
public:
  static Subscript *create(Expr *E);
  static Subscript *createEmptyRange();
  static Subscript *createSplat();

  Expr *getValue() const { return Val; }
  void setValue(Expr *V) { Val = V; }

  bool isEmptyRange() const { return Ty == EmptyRange; }
  bool isSplat() const { return Ty == Splat; }
};

class SubscriptTriplet : public Subscript {
  Subscript *Sub1;
  Subscript *Sub2;
  Subscript *Stride;
public:
};

class VectorSubscript : public Subscript {
  // int-expr
public:
};

class CoSubscript : public Subscript {
public:
};

class PartRef {
  Decl *PartName;
  llvm::SmallVector<Subscript*, 4> Subscripts;
  llvm::SmallVector<CoSubscript*, 2> ImageSelector;
public:
};

//
////////////////////////////////////////////////////////////////////////////////

//===----------------------------------------------------------------------===//
/// VarExpr -
class VarExpr : public DesignatorExpr {
  const VarDecl *Variable;
public:
  VarExpr(llvm::SMLoc Loc, const VarDecl *Var);

  const VarDecl *getVarDecl() const { return Variable; }

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

//===----------------------------------------------------------------------===//
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
  ExprResult E;
  UnaryExpr(ExprType ET, QualType T, SMLoc loc, Operator op, ExprResult e)
    : Expr(ET, T, loc), Op(op), E(e) {}
public:
  static UnaryExpr *Create(ASTContext &C, SMLoc loc, Operator op, ExprResult e);

  Operator getOperator() const { return Op; }

  const ExprResult getExpression() const { return E; }
  ExprResult getExpression() { return E; }

  virtual void print(llvm::raw_ostream&);

  static bool classof(const Expr *E) {
    return E->getExpressionID() == Expr::Unary;
  }
  static bool classof(const UnaryExpr *) { return true; }
};

//===----------------------------------------------------------------------===//
/// DefinedOperatorUnaryExpr -
class DefinedOperatorUnaryExpr : public UnaryExpr {
  IdentifierInfo *II;
  DefinedOperatorUnaryExpr(SMLoc loc, ExprResult e, IdentifierInfo *ii);
public:
  static DefinedOperatorUnaryExpr *Create(ASTContext &C, SMLoc loc,
                                          ExprResult e, IdentifierInfo *ii);

  const IdentifierInfo *getIdentifierInfo() const { return II; }
  IdentifierInfo *getIdentifierInfo() { return II; }

  virtual void print(llvm::raw_ostream&);

  static bool classof(const Expr *E) {
    return E->getExpressionID() == Expr::DefinedUnaryOperator;
  }
  static bool classof(const DefinedOperatorUnaryExpr *) { return true; }
};

//===----------------------------------------------------------------------===//
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
  ExprResult LHS;
  ExprResult RHS;
  BinaryExpr(ExprType ET, QualType T, SMLoc loc, Operator op,
             ExprResult lhs, ExprResult rhs)
    : Expr(ET, T, loc), Op(op), LHS(lhs), RHS(rhs) {}
public:
  static BinaryExpr *Create(ASTContext &C, SMLoc loc, Operator op,
                            ExprResult lhs, ExprResult rhs);

  Operator getOperator() const { return Op; }

  const ExprResult getLHS() const { return LHS; }
  ExprResult getLHS() { return LHS; }
  const ExprResult getRHS() const { return RHS; }
  ExprResult getRHS() { return RHS; }

  virtual void print(llvm::raw_ostream&);

  static bool classof(const Expr *E) {
    return E->getExpressionID() == Expr::Binary;
  }
  static bool classof(const BinaryExpr *) { return true; }
};

//===----------------------------------------------------------------------===//
/// DefinedOperatorBinaryExpr -
class DefinedOperatorBinaryExpr : public BinaryExpr {
  IdentifierInfo *II;
  DefinedOperatorBinaryExpr(SMLoc loc, ExprResult lhs, ExprResult rhs,
                            IdentifierInfo *ii)
    // FIXME: The type here needs to be calculated.
    : BinaryExpr(Expr::DefinedBinaryOperator, QualType(), loc, Defined,
                 lhs, rhs), II(ii) {}
public:
  static DefinedOperatorBinaryExpr *Create(ASTContext &C, SMLoc loc,
                                           ExprResult lhs, ExprResult rhs,
                                           IdentifierInfo *ii);

  const IdentifierInfo *getIdentifierInfo() const { return II; }
  IdentifierInfo *getIdentifierInfo() { return II; }

  virtual void print(llvm::raw_ostream&);

  static bool classof(const Expr *E) {
    return E->getExpressionID() == Expr::DefinedBinaryOperator;
  }
  static bool classof(const DefinedOperatorBinaryExpr *) { return true; }
};

} // end flang namespace

#endif
