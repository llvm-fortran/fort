! RUN: %flang -verify < %s
PROGRAM datatest
  INTEGER I, J, K
  REAL X,Y,Z, ZZZ
  INTEGER I_ARR(10)
  INTEGER I_ARR2(2,2)

  ! FIXME: PARAMETER (PI = 3.14)

  DATA I / 1 /
  DATA J, K / 2*42 /

  DATA X, Y, Z / 0*11 / ! expected-error {{expected an integer greater than 0}}
  DATA X, Y / 2*ZZZ / ! expected-error {{expected a constant expression}}
  DATA X, Y / 2, J / ! expected-error {{expected a constant expression}}

  ! FIXME: DATA X / PI /

  DATA (I_ARR(I), I = 1,10) / 10*0 /
  DATA (I_ARR(I), I = 0 + 1, 20/2) / 10*0 /

  DATA (I_ARR(WHAT), I = 1,10) / 10*0 / ! expected-error {{use of undeclared identifier 'WHAT'}}

  DATA (ZZZ, I = 1,10) / 1 / ! expected-error {{expected an implied do or an array element expression}}

  DATA (I_ARR(I), I = 1, .true.) / 10*0 / ! expected-error {{expected an integer constant or an implied do variable expression}}
  DATA (I_ARR(.false.), I = 1,5) / 5*11 / ! expected-error {{expected an integer expression}}

  DATA ((I_ARR2(I,J), J = 1,2), I = 1,2) / 4*3 /

  DATA ((I_ARR2(I,J), J = 1,2), I_ARR(I), I = 1,2) / 4*3, 2*0 /

  !FIXME: change error type.
  DATA ((I_ARR2(I,J), J = 1,2), I_ARR(J), I = 1,2) / 4*3, 2*0 / ! expected-error {{use of undeclared identifier 'J'}}

END PROGRAM
