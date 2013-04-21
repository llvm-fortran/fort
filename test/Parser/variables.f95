! RUN: %flang < %s
PROGRAM vartest
  IMPLICIT NONE
  INTEGER :: I
  REAL :: R
  LOGICAL :: L
  INTEGER I2
  REAL R2
  LOGICAL L3,L4,L5

  I = 22
  I = -13
  I = +777
  R = 12.0
  L = .TRUE.
  L = .false.

  I2 = I
  R2 = R
  L2 = L

  L3 = L
  L4 = .true.
  L5 = L3
END PROGRAM vartest
