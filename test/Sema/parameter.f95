! RUN: %flang -verify < %s
PROGRAM paramtest
  PARAMETER (x=0, y = 2.5, c = 'A') ! expected-note {{previous definition is here}}
  ! FIXME: REAL :: z
  PARAMETER (z = x / 2.0, w = z .EQ. 0)
  PARAMETER (exprs = 1 + 2 * 7) ! expected-note@+2 {{this expression is not allowed in a constant expression}}

  PARAMETER (fail = C(1:1)) ! expected-error {{parameter 'FAIL' must be initialized by a constant expression}}

  PARAMETER (x = 22) ! expected-error {{redefinition of 'X'}}

END PROGRAM paramtest
