//===----- CGABI.h - ABI types-----------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "CGABI.h"

namespace flang {
namespace CodeGen {

ABIArgInfo FortranABI::GetArgABI(QualType ArgType) {
  if(ArgType->isCharacterType())
    return ABIArgInfo(ABIArgInfo::Value);

  return ABIArgInfo(ABIArgInfo::Reference);
}

ABIRetInfo FortranABI::GetRetABI(QualType RetType) {
  if(RetType.isNull())
    return ABIRetInfo(ABIRetInfo::Nothing);
  if(RetType->isCharacterType())
    return ABIRetInfo(ABIRetInfo::CharacterValueAsArg);

  return ABIRetInfo(ABIRetInfo::Value);
}

ABIArgInfo RuntimeABI::GetArgABI(QualType ArgType) {
  if(ArgType->isCharacterType() ||
     ArgType->isCharacterType())
    return ABIArgInfo(ABIArgInfo::Expand);

  return ABIArgInfo(ABIArgInfo::Value);
}

ABIRetInfo RuntimeABI::GetRetABI(QualType RetType) {
  return FortranABI::GetRetABI(RetType);
}

}
} // end namespace flang
