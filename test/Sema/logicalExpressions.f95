! RUN: %flang -verify < %s
PROGRAM logicalexpressions
  IMPLICIT NONE
  LOGICAL L

  L = .true.
  L = L .AND. L
  L = L .OR. .TRUE.
  L = L .EQV. L
  L = L .NEQV. .FALSE.

  L = .FALSE. .OR. (.TRUE. .AND. .FALSE.)

  L = L .AND. 2 ! expected-error {{invalid operands to a logical binary expression ('LOGICAL' and 'INTEGER')}}
  L = L .OR. 'HELLO' ! expected-error {{invalid operands to a logical binary expression ('LOGICAL' and 'CHARACTER')}}
  L = 3.0 .EQV. 2.0d0 ! expected-error {{invalid operands to a logical binary expression ('REAL' and 'DOUBLE PRECISION')}}
  L = L .NEQV. (1.0,2.0) ! expected-error {{invalid operands to a logical binary expression ('LOGICAL' and 'COMPLEX')}}

END PROGRAM
