! RUN: not %fort -fsyntax-only -ffree-line-length-garbage %s 2>&1 | %file_check %s -check-prefix=VALUE
! RUN: not %fort -fsyntax-only -ffree-line-length-1parrot %s 2>&1 | %file_check %s -check-prefix=VALUE
! RUN: not %fort -fsyntax-only -ffixed-line-length-garbage %s 2>&1 | %file_check %s -check-prefix=VALUE
! RUN: not %fort -fsyntax-only -ffixed-line-length-1parrot %s 2>&1 | %file_check %s -check-prefix=VALUE
! VALUE: value invalid

! RUN: not %fort -fsyntax-only -ffree-line-length-1000000000000000000000000 %s 2>&1 | %file_check %s -check-prefix=SIZE
! RUN: not %fort -fsyntax-only -ffixed-line-length-1000000000000000000000000 %s 2>&1 | %file_check %s -check-prefix=SIZE
! SIZE: value too big

! RUN: %fort -fsyntax-only -ffree-line-length-136 %s
! RUN: %fort -fsyntax-only -ffree-line-length-0 %s
! RUN: %fort -fsyntax-only -ffree-line-length-none %s

! RUN: not %fort -fsyntax-only -ffree-line-length-100 %s 2>&1 | %file_check %s -check-prefix=LINE-LENGTH
! RUN: not %fort -fsyntax-only %s 2>&1 | %file_check %s -check-prefix=LINE-LENGTH

program test
  character(len=126) :: str
!LINE-LENGTH: missing terminating ' character
  str = 'abcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyz'
end program
