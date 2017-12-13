//===----- CGABI.h - ABI types-----------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "CGABI.h"

namespace fort {
namespace CodeGen {

ABIArgInfo FortranABI::GetArgABI(QualType ArgType) {
  if(ArgType->isCharacterType())
    return ABIArgInfo(ABIArgInfo::ExpandCharacterPutLengthToAdditionalArgsAsInt);
  else if(ArgType->isFunctionType())
    return ABIArgInfo(ABIArgInfo::Value);

  return ABIArgInfo(ABIArgInfo::Reference);
}

ABIRetInfo FortranABI::GetRetABI(QualType RetType) {
  if(RetType.isNull() || RetType->isVoidType())
    return ABIRetInfo(ABIRetInfo::Nothing);
  if(RetType->isCharacterType())
    return ABIRetInfo(ABIRetInfo::CharacterValueAsArg);

  return ABIRetInfo(ABIRetInfo::Value);
}

ABIArgInfo LibfortABI::GetArgABI(QualType ArgType) {
  if(ArgType->isComplexType() ||
     ArgType->isCharacterType())
    return ABIArgInfo(ABIArgInfo::Expand);
  return ABIArgInfo(ABIArgInfo::Value);
}

ABIRetInfo LibfortABI::GetRetABI(QualType RetType) {
  return FortranABI::GetRetABI(RetType);
}

ABIArgInfo LibfortTransferABI::GetArgABI(QualType ArgType) {
  if(ArgType->isCharacterType())
    return LibfortABI::GetArgABI(ArgType);

  return ABIArgInfo(ABIArgInfo::ReferenceAsVoidExtraSize);
}

}
} // end namespace fort
