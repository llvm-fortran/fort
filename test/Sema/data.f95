! RUN: %flang -fsyntax-only -verify < %s
PROGRAM datatest
  INTEGER I, J, K, M, N
  REAL X,Y,Z, A, ZZZ
  INTEGER I_ARR(10)
  INTEGER I_ARR2(2,2)
  REAL R_ARR(10)
  CHARACTER*(10) STR, STR_ARR(11)

  PARAMETER (PI = 3.14, INDEX = 1)

  DATA I / 1 /
  DATA J, K / 2*42 / M / 11 /

  DATA X, Y, Z / 0*11 / ! expected-error {{expected an integer greater than 0}}
  DATA X, Y / 2*ZZZ / ! expected-error {{expected a constant expression}}
  DATA X, Y / 2, J / ! expected-error {{expected a constant expression}}

  DATA X / PI /

  DATA X / 1 /

  DATA A / .false. / ! expected-error {{assigning to 'REAL' from incompatible type 'LOGICAL'}}
  DATA N / 'STR' /   ! expected-error {{assigning to 'INTEGER' from incompatible type 'CHARACTER'}}

  DATA R_ARR(1) / 1.0 / R_ARR(2), R_ARR(3) / 2*0.0 /

  DATA R_ARR(4) / .false. / ! expected-error {{assigning to 'REAL' from incompatible type 'LOGICAL'}}

  DATA STR / 'Hello' / STR_ARR(1)(:), STR_ARR(2) / 2*'World' /
  DATA STR_ARR(3)(2:4) / 'STR' /

  DATA STR_ARR(4)(:4) / 1 / ! expected-error {{assigning to 'CHARACTER' from incompatible type 'INTEGER'}}

  ! FIXME:
  ! DATA (I_ARR(I), I = 1,10) / 10*0 /
  ! DATA (I_ARR(I), I = 0 + 1, 20/2) / 10*0 /

  DATA (I_ARR(WHAT), I = 1,10) / 10*0 / ! expected-error {{use of undeclared identifier 'what'}}

  DATA (ZZZ, I = 1,10) / 1 / ! expected-error {{expected an implied do or an array element expression}}

  DATA (I_ARR(I), I = 1, .true.) / 10*0 / ! expected-error {{expected an integer constant or an implied do variable expression}}
  DATA (I_ARR(.false.), I = 1,5) / 5*11 / ! expected-error {{expected an integer expression}}

  ! DATA ((I_ARR2(I,J), J = 1,2), I = 1,2) / 4*3 /

  ! DATA ((I_ARR2(I,J), J = 1,2), I_ARR(I), I = 1,2) / 4*3, 2*0 /

  DATA ((I_ARR2(I,J), J = 1,2), I_ARR(J), I = 1,2) / 4*3, 2*0 / ! expected-error {{expected an integer constant or an implied do variable expression}}
  DATA (I_ARR(I+1), I=1,10) / 10*9 / ! expected-error {{expected an integer constant expression}}

  ! DATA (I_ARR(I), I = INDEX, 10) / 10 * 0 /
  ! DATA (I_ARR2(INDEX,J), J = 1,2) / 2*3 /

END PROGRAM
