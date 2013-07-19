! RUN: %flang -interpret %s | %file_check %s
PROGRAM jumps
  INTEGER I

  DATA I/0/

1 PRINT *, 'first'  ! CHECK: first
  I = I + 1         ! CHECK-NEXT: first
  IF(I < 2) GOTO 1
  PRINT *, 'end'    ! CHECK-NEXT: end

  GOTO 2
  PRINT *, 'not'
2 PRINT *, 'yes'    ! CHECK-NEXT: yes

END PROGRAM
