! RUN: %flang -fsyntax-only -verify < %s
PROGRAM dimtest
  IMPLICIT NONE

  ! FIXME: How about 'scalar' was defined here or something
  PARAMETER(SCALAR = 1.0) ! expected-note {{previous definition is here}}

  ! FIXME: note as above?
  REAL TheArray(10, 20)

  DIMENSION X(1,2,3,4,5)
  INTEGER X

  INTEGER Y, Z
  DIMENSION Y(20), Z(10)

  DIMENSION ARR(10) ! expected-error {{'DIMENSION' statement can't be applied because the identifier 'arr' isn't declared in the current context}}
  DIMENSION SCALAR(20) ! expected-error {{'DIMENSION' statement can't be applied because 'scalar' isn't a variable}}

  DIMENSION TheArray(10, 20) ! expected-error {{'DIMENSION' statement can't be applied to the variable 'thearray' because it is already an array}}

  REAL A

  DIMENSION A(10), FOO(5:100) ! expected-error {{'DIMENSION' statement can't be applied because the identifier 'foo' isn't declared in the current context}}

ENDPROGRAM
