! RUN: not %fort -fsyntax-only -ffree-line-length-garbage %s 2>&1 | FileCheck %s -check-prefix=VALUE
! RUN: not %fort -fsyntax-only -ffree-line-length-1parrot %s 2>&1 | FileCheck %s -check-prefix=VALUE
! RUN: not %fort -fsyntax-only -ffixed-line-length-garbage %s 2>&1 | FileCheck %s -check-prefix=VALUE
! RUN: not %fort -fsyntax-only -ffixed-line-length-1parrot %s 2>&1 | FileCheck %s -check-prefix=VALUE

! RUN: not %fort -fsyntax-only -ffixed-line-length-garbage %s 2>&1 | FileCheck %s -check-prefix=VALUE
! RUN: not %fort -fsyntax-only -ffixed-line-length-1parrot %s 2>&1 | FileCheck %s -check-prefix=VALUE
! RUN: not %fort -fsyntax-only -ffree-line-length-garbage %s 2>&1 | FileCheck %s -check-prefix=VALUE
! RUN: not %fort -fsyntax-only -ffree-line-length-1parrot %s 2>&1 | FileCheck %s -check-prefix=VALUE

! RUN: not %fort -fsyntax-only -ffree-line-length-1000000000000000000000000 %s 2>&1 | FileCheck %s -check-prefix=SIZE
! RUN: not %fort -fsyntax-only -ffixed-line-length-1000000000000000000000000 %s 2>&1 | FileCheck %s -check-prefix=SIZE

! RUN: not %fort -fsyntax-only -ffixed-line-length-1000000000000000000000000 %s 2>&1 | FileCheck %s -check-prefix=SIZE
! RUN: not %fort -fsyntax-only -ffree-line-length-1000000000000000000000000 %s 2>&1 | FileCheck %s -check-prefix=SIZE

! VALUE: invalid value
! SIZE: value '1000000000000000000000000' is too big

