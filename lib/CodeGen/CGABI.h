//===----- CGABI.h - ABI types-----------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef FORT_CODEGEN_ABI_H
#define FORT_CODEGEN_ABI_H

#include "ABIInfo.h"

namespace fort {
namespace CodeGen {

class FortranABI {
public:
  virtual ~FortranABI() {}
  virtual ABIArgInfo GetArgABI(QualType ArgType);
  virtual ABIRetInfo GetRetABI(QualType RetType);
};

class LibfortABI : public FortranABI {
public:
  virtual ~LibfortABI() {}
  ABIArgInfo GetArgABI(QualType ArgType);
  ABIRetInfo GetRetABI(QualType RetType);
};

class LibfortTransferABI : public LibfortABI {
public:
  virtual ~LibfortTransferABI() {}
  ABIArgInfo GetArgABI(QualType ArgType);
};

} // namespace CodeGen
} // end namespace fort

#endif
