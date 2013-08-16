! RUN: %flang -fsyntax-only -verify < %s

PROGRAM arrtest
  INTEGER I_ARR(5), I_ARR2(10), I_MAT(2,2)
  REAL R_ARR(5), R_ARR2(10)
  CHARACTER(Len = 10) STR_ARR(5)

  I_ARR = 1
  I_ARR = 2.0
  I_ARR = I_ARR
  R_ARR = I_ARR
  I_ARR = R_ARR
  STR_ARR = 'ABC'

  I_ARR = I_MAT ! expected-error {{expected an expression of array type with 1 dimension (2 dimensions invalid)}}
  I_ARR = I_ARR2 ! expected-error {{expected an expression of array type with size 5 for the dimension 1 (size 10 invalid)}}
  R_ARR2 = I_ARR ! expected-error {{expected an expression of array type with size 10 for the dimension 1 (size 5 invalid)}}
  I_ARR = 'ABC' ! expected-error {{assigning to 'integer' from incompatible type 'character'}}
  R_ARR = STR_ARR ! expected-error {{assigning to 'real' from incompatible type 'character (Len=10)'}}

ENDPROGRAM arrtest

SUBROUTINE FOO(I_ARR)
  INTEGER I_ARR(*), I_ARR2(5)

  I_ARR2 = I_ARR ! expected-error {{use of an array expression with an implied dimension specification}}
  I_ARR = I_ARR2 ! expected-error {{use of an array expression with an implied dimension specification}}
END
