//===--- MakePtr.h - pointer type templates --------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the make_ptr and make_const_ptr templates.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_FORT_BASIC_MAKEPTR_H
#define LLVM_FORT_BASIC_MAKEPTR_H

namespace fort {

template <typename T> struct make_ptr { typedef T *type; };
template <typename T> struct make_const_ptr { typedef const T *type; };

} // end namespace fort

#endif
