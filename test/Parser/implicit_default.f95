! RUN: %flang < %s
PROGRAM imptest
  !Unless specified otherwise,
  !all variables starting with letters I, J, K, L, M and N are default INTEGERs,
  !and all others are default REAL

  I = 22
  J = I
  k = J
  M = 0
  n = -1

  R = 33.1
  Z = 22.98
  a = -11.23
END PROGRAM imptest
