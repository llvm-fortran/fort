! RUN: %flang -interpret %s | %file_check %s
PROGRAM loops
  INTEGER I
  INTEGER J

  DO I = 1, 5          ! CHECK: I=1
    PRINT *, 'I=', I   ! CHECK-NEXT: I=2
  END DO               ! CHECK-NEXT: I=3
  CONTINUE             ! CHECK-NEXT: I=4
  CONTINUE             ! CHECK-NEXT: I=5
  PRINT *, 'END'       ! CHECK-NEXT: END

  J = -2
  DO I = 1, J
    PRINT *, 'I=', I
  END DO
  PRINT *, 'END'       ! CHECK-NEXT: END

  DO I = 1, J, -1      ! CHECK-NEXT: I=1
    PRINT *, 'I=', I   ! CHECK-NEXT: I=0
  END DO               ! CHECK-NEXT: I=-1
  CONTINUE             ! CHECK-NEXT: I=-2
  PRINT *, 'END'       ! CHECK-NEXT: END

  DO I = 1,1
    PRINT *, 'I=', I   ! CHECK-NEXT: I=1
  END DO
  PRINT *, 'END'       ! CHECK-NEXT: END

  J = 12
  DO I = 1,J,-1
    PRINT *, 'I=', I
  END DO
  PRINT *, 'END'       ! CHECK-NEXT: END

  I = 0
  DO WHILE(I < 3)      ! CHECK-NEXT: I=0
    PRINT *, 'I=', I   ! CHECK-NEXT: I=1
    I = I + 1
  END DO               ! CHECK-NEXT: I=2
  PRINT *, 'END'       ! CHECK-NEXT: END

END PROGRAM
