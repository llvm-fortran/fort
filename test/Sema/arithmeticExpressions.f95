! RUN: %flang -fsyntax-only -verify < %s
! RUN: %flang -fsyntax-only -verify %s 2>&1 | %file_check %s
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

  I = 1 + 1 ! CHECK: (1+1)
  I = I - I ! CHECK: (I-I)
  I = I * I ! CHECK: (I*I)
  I = I / I ! CHECK: (I/I)
  I = I ** 3 ! CHECK: (I**3)

  I = I ** 'pow' ! expected-error {{invalid operands to an arithmetic binary expression ('INTEGER' and 'CHARACTER')}}
  I = I + .false. ! expected-error {{invalid operands to an arithmetic binary expression ('INTEGER' and 'LOGICAL')}}
  I = 'true' * .true. ! expected-error {{invalid operands to an arithmetic binary expression ('CHARACTER' and 'LOGICAL')}}

  R = R + 1.0 ! CHECK: (R+1)

  R = R * I ! CHECK: R = (R*REAL(I))
  D = D - I ! CHECK: D = (D-REAL(I,Kind=8))
  C = C / I ! CHECK: C = (C/CMPLX(I))
  R = R ** I ! CHECK: R = (R**I)
  D = D ** I ! CHECK: D = (D**I)
  C = C ** I ! CHECK: C = (C**I)

  R = I * R ! CHECK: R = (REAL(I)*R)
  R = R - R ! CHECK: R = (R-R)
  D = D / R ! CHECK: D = (D/REAL(R,Kind=8))
  C = C ** R ! CHECK: C = (C**CMPLX(R))

  D = I + D ! CHECK: D = (REAL(I,Kind=8)+D)
  D = R * D ! CHECK: D = (REAL(R,Kind=8)*D)
  D = D - 2.0D1 ! CHECK: D = (D-20)  
  ! FIXME: make F77 only
  D = C / D ! expected-error {{invalid operands to an arithmetic binary expression ('COMPLEX' and 'DOUBLE PRECISION')}}

  C = I + C ! CHECK: C = (CMPLX(I)+C)
  C = R - C ! CHECK: C = (CMPLX(R)-C)
  ! FIXME: make F77 only
  C = D * C ! expected-error {{invalid operands to an arithmetic binary expression ('DOUBLE PRECISION' and 'COMPLEX')}}
  C = C / C ! CHECK: (C/C)
  C = C ** R ! CHECK: C = (C**CMPLX(R))

  I = +(I)
  I = -R ! CHECK: I = INT((-R))
  C = -C

  I = +.FALSE. ! expected-error {{invalid argument type 'LOGICAL' to an arithmetic unary expression}}
  R = -'TRUE' ! expected-error {{invalid argument type 'CHARACTER' to an arithmetic unary expression}}


ENDPROGRAM arithexpressions
