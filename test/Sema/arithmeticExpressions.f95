! RUN: %flang -verify < %s
PROGRAM arithexpressions
  IMPLICIT NONE
  INTEGER I
  REAL  R
  DOUBLE PRECISION D
  COMPLEX C

  I = 0
  R = 1.0
  D = 1.0
  C = (1.0,1.0)

  I = 1 + 1
  I = I - I
  I = I * I
  I = I / I
  I = I ** 3

  I = I ** 'pow' ! expected-error {{invalid operands to an arithmetic binary expression ('INTEGER' and 'CHARACTER')}}
  I = I + .false. ! expected-error {{invalid operands to an arithmetic binary expression ('INTEGER' and 'LOGICAL')}}
  I = 'true' * .true. ! expected-error {{invalid operands to an arithmetic binary expression ('CHARACTER' and 'LOGICAL')}}

  R = R + 1.0

  R = R * I
  D = D - I
  C = C / I
  R = R ** I
  D = D ** I
  C = C ** I

  R = I * R
  R = R - R
  D = D / R
  C = C ** R

  D = I + D
  D = I * D
  D = D - 2.0D1
  D = C / D ! expected-error {{invalid operands to an arithmetic binary expression ('COMPLEX' and 'DOUBLE PRECISION')}}

  C = I + C
  C = R - C
  C = D * C ! expected-error {{invalid operands to an arithmetic binary expression ('DOUBLE PRECISION' and 'COMPLEX')}}
  C = C / C
  C = C ** R

  I = +(I)
  I = -R
  C = -C

  I = +.FALSE. ! expected-error {{invalid argument type 'LOGICAL' to an arithmetic unary expression}}
  R = -'TRUE' ! expected-error {{invalid argument type 'CHARACTER' to an arithmetic unary expression}}


ENDPROGRAM arithexpressions
