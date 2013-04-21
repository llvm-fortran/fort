! RUN: %flang < %s
PROGRAM imptest
  IMPLICIT INTEGER(A)
  IMPLICIT REAL(B)
  IMPLICIT INTEGER(C-D)
  IMPLICIT REAL(M-N)

  A = 33
  B = 44.9
  C = A
  D = C
  M = B
  N = M

END PROGRAM imptest
