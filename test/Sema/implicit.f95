! RUN: %flang -fsyntax-only -verify < %s
! RUN: %flang -fsyntax-only -verify -ast-print %s 2>&1 | %file_check %s
PROGRAM imptest
  IMPLICIT INTEGER (A, B)
  IMPLICIT REAL (C-E, F)
  IMPLICIT REAL (A) ! expected-error {{redefinition of implicit rule 'A'}}
  IMPLICIT INTEGER (F-G) ! expected-error {{redefinition of implicit rule in the range 'F' - 'G'}}
  IMPLICIT INTEGER (L, P, B) ! expected-error {{redefinition of implicit rule 'B'}}
  IMPLICIT INTEGER (Z-X) ! expected-error {{the range 'Z' - 'X' isn't alphabetically ordered}}

  A = 1 ! CHECK: A = 1
  B = 2.0 ! CHECK: B = INT(2)

  C = A ! CHECK: C = REAL(A)
  D = C ! CHECK: D = C
  E = -1.0 ! CHECK: E = (-1)
  F = C ! CHECK: F = C

  I = 0 ! CHECK: I = 0
END PROGRAM imptest
