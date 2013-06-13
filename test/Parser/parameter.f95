! RUN: %flang < %s
! FAIL
PROGRAM paramtest
  PARAMETER (x=0, y = 2.5, c = 'ABC')
  ! FIXME: allow REAL NUM
  PARAMETER (NUM = 0.1e4)
END PROGRAM paramtest
