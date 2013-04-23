! RUN: %flang < %s
PROGRAM arrtest
  INTEGER I_ARR(30)
  REAL MATRIX(4,4)
  LOGICAL SET(10:20)

  !I_ARR(1) = 2
  !I_ARR(2) = 3
  !I_ARR(3) = 4
ENDPROGRAM arrtest
