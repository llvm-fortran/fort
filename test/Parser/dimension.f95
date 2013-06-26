! RUN: %flang -verify < %s
PROGRAM dimtest

  DIMENSION X(1,2,3,4,5)
  INTEGER X

  INTEGER Y, Z, W
  DIMENSION Y(20), Z(10)

  DIMENSION W(20 ! expected-error {{expected ')'}}

ENDPROGRAM
