! RUN: %flang -verify < %s
PROGRAM relexpressions
  IMPLICIT NONE
  INTEGER I
  REAL R
  COMPLEX C
  LOGICAL L

  I = 0
  R = 2.0
  C = (1.0,1.0)

  L = I .LT. I
  L = I .EQ. 2
  L = 3 .NE. I
  L = I .GT. R
  L = I .LE. R
  L = I .GE. I

  L = R .LT. R
  L = R .GT. 2.0

  L = C .EQ. C
  L = C .NE. C
  L = C .NE. R
  L = C .LE. C ! expected-error {{invalid operands to a relational binary expression ('COMPLEX' and 'COMPLEX')}}

  L = 'HELLO' .EQ. 'WORLD'
  L = 'HELLO' .NE. 'WORLD'

  I = 1 .NE. 2 ! expected-error {{assigning to 'INTEGER' from incompatible type 'LOGICAL'}}
  R = 2.0 .LT. 1 ! expected-error {{assigning to 'REAL' from incompatible type 'LOGICAL'}}

  L = L .EQ. L ! expected-error {{invalid operands to a relational binary expression ('LOGICAL' and 'LOGICAL')}}
  L = .TRUE. .NE. L ! expected-error {{invalid operands to a relational binary expression ('LOGICAL' and 'LOGICAL')}}
  L = L .LT. .FALSE. ! expected-error {{invalid operands to a relational binary expression ('LOGICAL' and 'LOGICAL')}}

END PROGRAM
