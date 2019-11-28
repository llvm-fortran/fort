! RUN: %fort -fsyntax-only -ffree-line-length-136 %s
! RUN: %fort -fsyntax-only -ffree-line-length-0 %s
! RUN: %fort -fsyntax-only -ffree-line-length-none %s

! RUN: not %fort -fsyntax-only -ffree-line-length-100 %s 2>&1 | FileCheck %s -check-prefix=LINE-LENGTH
! RUN: not %fort -fsyntax-only %s 2>&1 | FileCheck %s -check-prefix=LINE-LENGTH

program test
  character(len=126) :: str
!LINE-LENGTH: missing terminating ' character
  str = 'abcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyz'
end program
