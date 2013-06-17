! RUN: %flang -verify < %s
PROGRAM arithexpressions
  IMPLICIT NONE
  INTEGER I
  REAL  R
  DOUBLE PRECISION D
  COMPLEX C
  LOGICAL L
  CHARACTER * 10 CHARS

  I = 1 + 1
  I = I - I
  I = I * I
  I = I / I
  I = I ** 3

  I = I ** 'pow' ! expected-error {{invalid operands to binary expression ('INTEGER' and 'CHARACTER')}}
  I = I + .false. ! expected-error {{invalid operands to binary expression ('INTEGER' and 'LOGICAL')}}
  I = 'true' * .true. ! expected-error {{invalid operands to binary expression ('CHARACTER' and 'LOGICAL')}}
ENDPROGRAM arithexpressions
