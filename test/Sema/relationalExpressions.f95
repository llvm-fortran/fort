! RUN: %flang -verify < %s
! RUN: %flang -verify %s 2>&1 | %file_check %s
PROGRAM relexpressions
  IMPLICIT NONE
  INTEGER I
  REAL R
  COMPLEX C
  LOGICAL L

  I = 0
  R = 2.0
  C = (1.0,1.0)

  L = I .LT. I ! CHECK: (I<I)
  L = I .EQ. 2 ! CHECK: (I==2)
  L = 3 .NE. I ! CHECK: (3/=I)
  L = I .GT. R ! CHECK: (REAL(I)>R)
  L = I .LE. R ! CHECK: (REAL(I)<=R)
  L = I .GE. I ! CHECK: (I>=I)

  L = R .LT. R ! CHECK: (R<R)
  L = R .GT. 2.0 ! CHECK: (R>2)

  L = C .EQ. C ! CHECK: (C==C)
  L = C .NE. C ! CHECK: (C/=C)
  L = C .NE. R ! CHECK: (C/=CMPLX(R))
  L = C .LE. C ! expected-error {{invalid operands to a relational binary expression ('COMPLEX' and 'COMPLEX')}}
  L = C .EQ. 2.0 ! CHECK: (C==CMPLX(2))
  L = C .EQ. 2.0d-1 ! expected-error {{invalid operands to a relational binary expression ('COMPLEX' and 'DOUBLE PRECISION')}}

  L = 'HELLO' .EQ. 'WORLD'
  L = 'HELLO' .NE. 'WORLD'

  I = 1 .NE. 2 ! expected-error {{assigning to 'INTEGER' from incompatible type 'LOGICAL'}}
  R = 2.0 .LT. 1 ! expected-error {{assigning to 'REAL' from incompatible type 'LOGICAL'}}

  L = L .EQ. L ! expected-error {{invalid operands to a relational binary expression ('LOGICAL' and 'LOGICAL')}}
  L = .TRUE. .NE. L ! expected-error {{invalid operands to a relational binary expression ('LOGICAL' and 'LOGICAL')}}
  L = L .LT. .FALSE. ! expected-error {{invalid operands to a relational binary expression ('LOGICAL' and 'LOGICAL')}}

END PROGRAM
