! RUN: %flang -interpret %s | %file_check %s
PROGRAM test
  J(I) = I + 1
  CHARACTER*(7) ABC
  INTEGER FOO
  FOO(A) = J(INT(A)) * 2
  ABC() = 'VOLBEAT'

  PRINT *, J(2) ! CHECK: 3
  PRINT *, J(-1) ! CHECK-NEXT: 0
  PRINT *, FOO(2.5) ! CHECK-NEXT: 6
  PRINT *, ABC() ! CHECK-NEXT: VOLBEAT

END PROGRAM test
