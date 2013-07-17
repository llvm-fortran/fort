! RUN: %flang -fsyntax-only -verify < %s
! RUN: %flang -fsyntax-only -verify -ast-print %s 2>&1 | %file_check %s
PROGRAM exttest
  EXTERNAL SUB

  INTEGER FUNC
  REAL FUNC2
  EXTERNAL FUNC, FUNC2

  INTEGER FUNC ! expected-error {{the return type for a function 'FUNC' was already specified}}

  INTEGER F3(10) ! expected-error {{invalid type for a function 'F3'}}
  EXTERNAL F3

  EXTERNAL F4
  COMPLEX F4
  REAL F4 ! expected-error {{the return type for a function 'F4' was already specified}}

  INTEGER I
  REAL R
  COMPLEX C

  I = FUNC() + FUNC2() ! CHECK: I = INT((REAL(FUNC())+FUNC2()))
  C = F4(I) ! CHECK: C = F4(I)

END PROGRAM
