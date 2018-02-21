//===-- DeclSpec.h - Declaration Type Specifiers ----------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Declaration type specifiers.
//
//===----------------------------------------------------------------------===//

#ifndef FORT_SEMA_DECLSPEC_H__
#define FORT_SEMA_DECLSPEC_H__

#include "fort/AST/Type.h"
#include "fort/Basic/SourceLocation.h"
#include "fort/Basic/Specifiers.h"
#include "fort/Sema/Ownership.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"

namespace llvm {
class raw_ostream;
} // namespace llvm

namespace fort {

class Expr;
class ArraySpec;
class RecordDecl;

//===----------------------------------------------------------------------===//
/// DeclSpec - A declaration type specifier is the type -- intrinsic, TYPE, or
/// CLASS -- plus any kind selectors for that type.
class DeclSpec {
public:
  // Import intrinsic type specifiers.
  typedef TypeSpecifierType TST;
  static const TST TST_unspecified = fort::TST_unspecified;
  static const TST TST_integer = fort::TST_integer;
  static const TST TST_real = fort::TST_real;
  static const TST TST_complex = fort::TST_complex;
  static const TST TST_character = fort::TST_character;
  static const TST TST_logical = fort::TST_logical;
  static const TST TST_struct = fort::TST_struct;

  // Import attribute specifiers.
  typedef AttributeSpecifier AS;
  static const AS AS_unspecified = fort::AS_unspecified;
  static const AS AS_allocatable = fort::AS_allocatable;
  static const AS AS_asynchronous = fort::AS_asynchronous;
  static const AS AS_codimension = fort::AS_codimension;
  static const AS AS_contiguous = fort::AS_contiguous;
  static const AS AS_dimension = fort::AS_dimension;
  static const AS AS_external = fort::AS_external;
  static const AS AS_intrinsic = fort::AS_intrinsic;
  static const AS AS_optional = fort::AS_optional;
  static const AS AS_parameter = fort::AS_parameter;
  static const AS AS_pointer = fort::AS_pointer;
  static const AS AS_protected = fort::AS_protected;
  static const AS AS_save = fort::AS_save;
  static const AS AS_target = fort::AS_target;
  static const AS AS_value = fort::AS_value;
  static const AS AS_volatile = fort::AS_volatile;

  /// Import intent specifiers.
  typedef IntentSpecifier IS;
  static const IS IS_unspecified = fort::IS_unspecified;
  static const IS IS_in = fort::IS_in;
  static const IS IS_out = fort::IS_out;
  static const IS IS_inout = fort::IS_inout;

  /// Import access specifiers.
  typedef AccessSpecifier AC;
  static const AC AC_unspecified = fort::AC_unspecified;
  static const AC AC_public = fort::AC_public;
  static const AC AC_private = fort::AC_private;

private:
  /*TST*/ unsigned TypeSpecType : 3;
  /*AS*/ unsigned AttributeSpecs : 15;
  /*IS*/ unsigned IntentSpec : 3;
  /*AC*/ unsigned AccessSpec : 3;
  unsigned IsDoublePrecision : 1; // can apply to reals or complex
  unsigned IsByte : 1;            // Logical is BYTE type
  unsigned IsStarLength : 1;      // LEN = *

  /// \brief The kind and length selectors.
  Expr *Kind;
  Expr *Len;
  SmallVector<ArraySpec *, 4> Dimensions;
  RecordDecl *Record;

public:
  explicit DeclSpec()
      : TypeSpecType(TST_unspecified), AttributeSpecs(AS_unspecified),
        IntentSpec(IS_unspecified), AccessSpec(AC_unspecified),
        IsDoublePrecision(0), IsByte(0), IsStarLength(0), Kind(0), Len(0),
        Record(nullptr) {}
  virtual ~DeclSpec();

  bool isDoublePrecision() const { return IsDoublePrecision == 1; }
  void setDoublePrecision() { IsDoublePrecision = 1; }

  bool isByte() const { return IsByte == 1; }
  void setByte() { IsByte = 1; }

  bool hasKindSelector() const { return Kind != 0; }
  Expr *getKindSelector() const { return Kind; }
  void setKindSelector(Expr *K) { Kind = K; }

  bool hasLengthSelector() const { return Len != 0 || IsStarLength != 0; }
  bool isStarLengthSelector() const { return IsStarLength != 0; }
  Expr *getLengthSelector() const { return Len; }
  void setLengthSelector(Expr *L) { Len = L; }
  void setStartLengthSelector() { IsStarLength = 1; }

  bool hasDimensions() const { return !Dimensions.empty(); }
  void setDimensions(ArrayRef<ArraySpec *> Dims);
  ArrayRef<ArraySpec *> getDimensions() const { return Dimensions; }

  RecordDecl *getRecord() const { return Record; }
  void setRecord(RecordDecl *R) { Record = R; }

  typedef SmallVectorImpl<ArraySpec *>::iterator dim_iterator;
  typedef SmallVectorImpl<ArraySpec *>::const_iterator const_dim_iterator;
  dim_iterator begin() { return Dimensions.begin(); }
  dim_iterator end() { return Dimensions.end(); }
  const_dim_iterator begin() const { return Dimensions.begin(); }
  const_dim_iterator end() const { return Dimensions.end(); }

  /// getSpecifierName - Turn a type-specifier-type into a string like "REAL"
  /// or "ALLOCATABLE".
  static const char *getSpecifierName(DeclSpec::TST I);
  static const char *getSpecifierName(DeclSpec::AS A);
  static const char *getSpecifierName(DeclSpec::IS I);
  static const char *getSpecifierName(DeclSpec::AC I);

  bool hasAttributeSpec(DeclSpec::AS A) const { return AttributeSpecs & A; }
  unsigned getAttributeSpecs() const { return AttributeSpecs; }
  void setAttributeSpec(DeclSpec::AS A) { AttributeSpecs |= A; }

  bool hasIntentSpec(DeclSpec::IS I) const { return IntentSpec & I; }
  IS getIntentSpec() const { return IS(IntentSpec); }
  void setIntentSpec(DeclSpec::IS I) { IntentSpec |= I; }

  bool hasAccessSpec(DeclSpec::AC A) const { return AccessSpec & A; }
  AC getAccessSpec() const { return AC(AccessSpec); }
  void setAccessSpec(DeclSpec::AC A) { AccessSpec |= A; }

  TST getTypeSpecType() const { return TST(TypeSpecType); }
  bool SetTypeSpecType(TST T) {
    if (TST(TypeSpecType) != TST_unspecified)
      return true;
    TypeSpecType = T;
    return false;
  }

  bool hasAttributes() const {
    return AttributeSpecs != 0 || IntentSpec != 0 || AccessSpec != 0;
  }

  virtual void print(llvm::raw_ostream &) {}

  static bool classof(DeclSpec *) { return true; }
};

} // namespace fort

#endif
