//===-- Type.h - Fortran Type Interface -------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// The Fortran type interface.
//
//===----------------------------------------------------------------------===//

#ifndef FORTRAN_TYPE_H__
#define FORTRAN_TYPE_H__

#include "flang/Sema/Ownership.h"
#include "llvm/ADT/FoldingSet.h"
#include "llvm/ADT/SmallVector.h"

namespace llvm {
class raw_ostream;
} // end llvm namespace

namespace fortran {

class ASTContext;
class Expr;
class VarDecl;

//===----------------------------------------------------------------------===//
/// Selector - A selector is a modifier on a type that indicates different
/// properties for the type: precision, length, etc.
class Selector {
  ExprResult KindExpr;
public:
  const ExprResult getKindExpr() const { return KindExpr; }
  ExprResult getKindExpr() { return KindExpr; }
  void setKindExpr(ExprResult KE) { KindExpr = KE; }

  void print(llvm::raw_ostream &O) const;
};

//===----------------------------------------------------------------------===//
/// Type - The base class for Fortran types.
class Type {
public:
  /// TypeClass - The intrinsic Fortran type specifications. REAL is the default
  /// if "IMPLICIT NONE" isn't specified.
  enum TypeClass {
    TC_None    = 0,
    TC_Builtin = 1,
    TC_Complex = 2,
    TC_Array   = 3,
    TC_Pointer = 4
  };

  enum AttrSpec {
    AS_None            = 0,
    AS_Allocatable     = 1 << 0,
    AS_Asynchronous    = 1 << 1,
    AS_Dimension       = 1 << 2,
    AS_External        = 1 << 3,
    AS_Intent          = 1 << 4,
    AS_Intrinsic       = 1 << 5,
    AS_Optional        = 1 << 6,
    AS_Parameter       = 1 << 7,
    AS_Pointer         = 1 << 8,
    AS_Protected       = 1 << 9,
    AS_Save            = 1 << 10,
    AS_Target          = 1 << 11,
    AS_Value           = 1 << 12,
    AS_Volatile        = 1 << 13,
    AS_AccessSpec      = 1 << 30,
    AS_LangBindingSpec = 1 << 31
  };

  enum AccessSpec {
    AC_None    = 0,
    AC_Public  = 1 << 0,
    AC_Private = 1 << 1
  };

  enum IntentSpec {
    IS_None   = 0,
    IS_In     = 1 << 0,
    IS_Out    = 1 << 1,
    IS_InOut  = 1 << 2
  };

private:
  TypeClass TyClass;
protected:
  Type(TypeClass tc) : TyClass(tc) {}
  virtual ~Type();
  virtual void Destroy(ASTContext& C);
  friend class ASTContext;
public:
  TypeClass getTypeClass() const { return TyClass; }

  virtual void print(llvm::raw_ostream &O) const = 0;

  static bool classof(const Type *) { return true; }
};

/// BuiltinType - Intrinsic Fortran types.
class BuiltinType : public Type, public llvm::FoldingSetNode {
public:
  /// TypeSpec - The intrinsic Fortran type specifications. REAL is the default
  /// if "IMPLICIT NONE" isn't specified.
  enum TypeSpec {
    TS_Invalid         = -1,
    TS_Integer         = 0,
    TS_Real            = 1,
    TS_DoublePrecision = 2,
    TS_Complex         = 3,
    TS_Character       = 4,
    TS_Logical         = 5
  };

protected:
  TypeSpec TySpec;              //< Type specification.
  Selector Kind;                //< Kind selector.
public:
  BuiltinType() : Type(TC_Builtin), TySpec(TS_Real) {}
  BuiltinType(TypeSpec TS) : Type(TC_Builtin), TySpec(TS) {}
  BuiltinType(TypeSpec TS, Selector K)
    : Type(TC_Builtin), TySpec(TS), Kind(K)
  {}
  virtual ~BuiltinType();

  TypeSpec getTypeSpec() const { return TySpec; }

  bool hasKind() const { return Kind.getKindExpr().isUsable(); }
  Selector getKind() const { return Kind; }

  bool isIntegerType() const { return TySpec == TS_Integer; }
  bool isRealType() const { return TySpec == TS_Real; }
  bool isDoublePrecisionType() const { return TySpec == TS_DoublePrecision; }
  bool isCharacterType() const { return TySpec == TS_Character; }
  bool isLogicalType() const { return TySpec == TS_Logical; }

  virtual void Profile(llvm::FoldingSetNodeID &ID) {
    Profile(ID, TySpec, Kind);
  }
  static void Profile(llvm::FoldingSetNodeID &ID, TypeSpec TS, Selector K) {
    ID.AddInteger(TS);
    ID.AddPointer(K.getKindExpr().get());
  }

  virtual void print(llvm::raw_ostream &O) const;

  static bool classof(const Type *T) { return T->getTypeClass() == TC_Builtin; }
  static bool classof(const BuiltinType *) { return true; }
};

class CharacterBuiltinType : public BuiltinType {
  Selector Len;
public:
  CharacterBuiltinType()
    : BuiltinType(TS_Character) {}
  CharacterBuiltinType(Selector L)
    : BuiltinType(TS_Character), Len(L) {}
  CharacterBuiltinType(Selector L, Selector K)
    : BuiltinType(TS_Character, K), Len(L) {}
  virtual ~CharacterBuiltinType();

  bool hasLen() const { return Len.getKindExpr().isUsable(); }
  Selector getLen() const { return Len; }
  void setLen(Selector L) { Len = L; }

  virtual void Profile(llvm::FoldingSetNodeID &ID) {
    BuiltinType::Profile(ID);
    Profile(ID, Len, Kind);
  }
  static void Profile(llvm::FoldingSetNodeID &ID, Selector L, Selector K) {
    ID.AddInteger(TS_Character);
    ID.AddPointer(L.getKindExpr().get());
    ID.AddPointer(K.getKindExpr().get());
  }

  virtual void print(llvm::raw_ostream &O) const;

  static bool classof(const Type *T) {
    return T->getTypeClass() == TC_Builtin &&
      ((const BuiltinType*)T)->isCharacterType();
  }
  static bool classof(const BuiltinType *BT) { return BT->isCharacterType(); }
  static bool classof(const CharacterBuiltinType *) { return true; }
};

/// PointerType - Allocatable types.
class PointerType : public Type, public llvm::FoldingSetNode {
  const Type *BaseType;
  unsigned NumDims;
  friend class ASTContext;  // ASTContext creates these.
public:
  PointerType(const Type *BaseTy, unsigned Dims)
    : Type(TC_Pointer), BaseType(BaseTy), NumDims(Dims) {}

  const Type *getPointeeType() const { return BaseType; }
  unsigned getNumDimensions() const { return NumDims; }

  void Profile(llvm::FoldingSetNodeID &ID) {
    Profile(ID, getPointeeType(), getNumDimensions());
  }
  static void Profile(llvm::FoldingSetNodeID &ID, const Type *ElemTy,
                      unsigned NumDims) {
    ID.AddPointer(ElemTy);
    ID.AddInteger(NumDims);
  }

  virtual void print(llvm::raw_ostream &O) const {} // FIXME

  static bool classof(const Type *T) { return T->getTypeClass() == TC_Pointer; }
  static bool classof(const PointerType *) { return true; }
};

/// ArrayType - Array types.
class ArrayType : public Type, public llvm::FoldingSetNode {
  const Type *ElemType;
  llvm::SmallVector<unsigned, 4> Dimensions;
  friend class ASTContext;  // ASTContext creates these.
public:
  ArrayType(const Type *ElemTy, const llvm::SmallVectorImpl<unsigned> &Dims)
    : Type(TC_Array), ElemType(ElemTy) {
    Dimensions.append(Dims.begin(), Dims.end());
  }

  const Type *getElementType() const { return ElemType; }
  const llvm::SmallVectorImpl<unsigned> &getDimensions() const {
    return Dimensions;
  }

  typedef llvm::SmallVectorImpl<unsigned>::iterator dim_iterator;
  typedef llvm::SmallVectorImpl<unsigned>::const_iterator const_dim_iterator;

  size_t size() const              { return Dimensions.size(); }
  dim_iterator begin()             { return Dimensions.begin(); }
  dim_iterator end()               { return Dimensions.end(); }
  const_dim_iterator begin() const { return Dimensions.begin(); }
  const_dim_iterator end() const   { return Dimensions.end(); }

  void Profile(llvm::FoldingSetNodeID &ID) {
    Profile(ID, ElemType, Dimensions);
  }
  static void Profile(llvm::FoldingSetNodeID &ID, const Type *ElemTy,
                      const llvm::SmallVectorImpl<unsigned> &Dims) {
    ID.AddPointer(ElemTy);

    for (llvm::SmallVectorImpl<unsigned>::const_iterator
           I = Dims.begin(), E = Dims.end(); I != E; ++I)
      ID.AddInteger(*I);
  }

  virtual void print(llvm::raw_ostream &O) const {} // FIXME

  static bool classof(const Type *T) { return T->getTypeClass() == TC_Array; }
  static bool classof(const ArrayType *) { return true; }
};

} // end fortran namespace

#endif
