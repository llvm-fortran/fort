! RUN: %flang -fsyntax-only -verify < %s
PROGRAM charexpressions
  IMPLICIT NONE
  CHARACTER * 16 C

  C = 'HELLO' // 'WORLD'
  C = C // '!'

  C = C // 1 ! expected-error {{invalid operands to a character binary expression ('CHARACTER (LEN=16)' and 'INTEGER')}}
  C = .false. // 'TRUE' ! expected-error {{invalid operands to a character binary expression ('LOGICAL' and 'CHARACTER')}}

ENDPROGRAM charexpressions
