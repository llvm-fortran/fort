! RUN: %flang -fsyntax-only -verify < %s
PROGRAM dimtest
  IMPLICIT NONE

  PARAMETER(SCALAR = 1.0) ! expected-note {{'scalar' is a parameter constant defined here}}

  ! FIXME: note as above?
  REAL TheArray(10, 20)

  DIMENSION X(1,2,3,4,5)
  INTEGER X

  INTEGER Y, Z
  DIMENSION Y(20), Z(10)

  DIMENSION ARR(10) ! expected-error {{use of undeclared identifier 'arr'}}
  DIMENSION SCALAR(20) ! expected-error {{specification statement requires a local variable or an argument}}

  DIMENSION TheArray(10, 20) ! expected-error {{'DIMENSION' statement can't be applied to the variable 'thearray' because it is already an array}}

  REAL A

  DIMENSION A(10), FOO(5:100) ! expected-error {{use of undeclared identifier 'foo'}}

ENDPROGRAM
