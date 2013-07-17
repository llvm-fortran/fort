! RUN: %flang -fsyntax-only < %s
! RUN: %flang -fsyntax-only -ast-print %s 2>&1 | %file_check %s
PROGRAM imptest
  !Unless specified otherwise,
  !all variables starting with letters I, J, K, L, M and N are default INTEGERs,
  !and all others are default REAL

  I = 22 ! CHECK: I = 22
  J = I ! CHECK: J = I
  k = J ! CHECK: K = J
  M = 0.0 ! CHECK: M = INT(0)
  n = -1 ! CHECK: N = (-1)

  R = 33.25 ! CHECK: R = 33.25
  Z = 1 ! CHECK: Z = REAL(1)
  a = -11.23
END PROGRAM imptest
