! RUN: %flang < %s
PROGRAM vartest
  INTEGER :: I
  REAL :: R
  LOGICAL :: L

  I = 22
  R = 12.0
  L = .TRUE.
  L = .false.
END PROGRAM vartest
