! RUN: %flang -verify < %s
PROGRAM datatest
  INTEGER I, J, K
  REAL X,Y,Z

  DATA I / 1 /
  DATA J, K / 2*42 /

  DATA X Y / 1, 2 / ! expected-error {{expected '/'}}

  DATA X, Y / 1 2 / ! expected-error {{expected '/'}}

  DATA ! expected-error@+2 {{expected an expression}}

END PROGRAM
