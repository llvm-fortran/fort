! RUN: %flang -verify < %s
PROGRAM datatest
  INTEGER I, J, K
  REAL X,Y,Z, ZZZ

  ! FIXME: PARAMETER (PI = 3.14)

  DATA I / 1 /
  DATA J, K / 2*42 /

  DATA X, Y, Z / 0*11 / ! expected-error {{expected an integer greater than 0}}
  DATA X, Y / 2*ZZZ / ! expected-error {{expected a constant expression}}
  DATA X, Y / 2, J / ! expected-error {{expected a constant expression}}

  ! FIXME: DATA X / PI /

END PROGRAM
