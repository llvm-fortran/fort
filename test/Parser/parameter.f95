! RUN: %flang < %s
PROGRAM paramtest
  PARAMETER (x=0, y = 2.5, c = 'A')
  ! FIXME: allow REAL NUM
  PARAMETER (NUM = 0.1e4)
  PARAMETER (CM = (0.5,-6e2))
END PROGRAM paramtest
