! RUN: %flang -verify < %s
PROGRAM charlitsub
  CHARACTER (LEN=16) :: C

  C = 'HELLO'
  C = 'HELLO'(1:3)
  C = 'HELLO'(1:4 ! expected-error@+1 {{expected ')'}}
  C = 'HELLO'(1 4) ! expected-error {{expected ':'}}
  C = 'HELLO'(:)
  C = 'HELLO'(2:)
  C = 'HELLO'(:3)

ENDPROGRAM charlitsub
