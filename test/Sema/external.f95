! RUN: %flang -fsyntax-only -verify < %s
! RUN: %flang -fsyntax-only -verify -ast-print %s 2>&1 | %file_check %s
PROGRAM exttest
  EXTERNAL SUB

  INTEGER FUNC
  REAL FUNC2
  EXTERNAL FUNC, FUNC2

  INTEGER FUNC ! expected-error {{the return type for a function 'func' was already specified}}

  INTEGER F3(10) ! expected-error {{invalid type for a function 'f3'}}
  EXTERNAL F3

  EXTERNAL F4
  COMPLEX F4
  REAL F4 ! expected-error {{the return type for a function 'f4' was already specified}}

  INTEGER I
  REAL R
  COMPLEX C

  I = FUNC() + FUNC2() ! CHECK: i = int((real(func())+func2()))
  C = F4(I) ! CHECK: c = f4(i)

END PROGRAM
