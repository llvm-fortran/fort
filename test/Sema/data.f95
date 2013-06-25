! RUN: %flang -verify < %s
PROGRAM datatest
  INTEGER I, J, K
  REAL X,Y,Z, ZZZ
  INTEGER I_ARR(10)

  ! FIXME: PARAMETER (PI = 3.14)

  DATA I / 1 /
  DATA J, K / 2*42 /

  DATA X, Y, Z / 0*11 / ! expected-error {{expected an integer greater than 0}}
  DATA X, Y / 2*ZZZ / ! expected-error {{expected a constant expression}}
  DATA X, Y / 2, J / ! expected-error {{expected a constant expression}}

  ! FIXME: DATA X / PI /

  DATA (I_ARR(I), I = 1,10) / 10*0 /
  DATA (I_ARR(WHAT), I = 1,10) / 10*0 / ! expected-error {{use of undeclared identifier 'WHAT'}}

END PROGRAM
