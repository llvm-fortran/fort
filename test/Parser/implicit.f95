! RUN: %flang -verify < %s
PROGRAM imptest
  IMPLICIT INTEGER(A)
  IMPLICIT REAL(B, G), COMPLEX(H)
  IMPLICIT INTEGER(C-D)
  IMPLICIT REAL(M - N)

  IMPLICIT REAL (0) ! expected-error {{expected a letter}}
  IMPLICIT REAL (X-'Y') ! expected-error {{expected a letter}}
  IMPLICIT REAL X ! expected-error {{expected '('}}
  IMPLICIT REAL (X ! expected-error@+2 {{expected ')'}}

  A = 33
  B = 44.9
  C = A
  D = C
  M = B
  N = M

END PROGRAM imptest
