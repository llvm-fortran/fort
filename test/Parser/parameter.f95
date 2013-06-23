! RUN: %flang -verify < %s
PROGRAM paramtest
  PARAMETER (x=0, y = 2.5, c = 'A')
  ! FIXME: allow REAL NUM
  PARAMETER (NUM = 0.1e4)
  PARAMETER (CM = (0.5,-6e2))

  PARAMETER ! expected-error@+1 {{expected '('}}
  PARAMETER (A = 1 B = 2) ! expected-error {{expected ')'}}

END PROGRAM paramtest
