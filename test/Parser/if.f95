! RUN: %flang < %s
PROGRAM iftest
  CHARACTER (LEN=11) :: C
  IF(1 == 1) C = "YES"

  IF(1 == 2) THEN
    C = "NO"
  END IF

  IF(3 == 3) THEN
    C = "MAYBE"
  ELSE
    C = "ENDOFDAYS"
  ENDIF

  IF(42 == 69) THEN
    C = "NOPE"
  ELSE IF(12 == 13) THEN
    C = "POSSIBLY"
  ELSEIF(123 == 123) THEN
    C = "CORRECT"
    PRINT *, C
  ELSE
    C = "NEVER"
  END IF

  !Here comes nesting
  IF(33 == 22) THEN
    IF(22 == 33) THEN
      IF(11 == 11) THEN
        STOP
      END IF
    ELSE
      PRINT *, C
    ENDIF
  END IF

END PROGRAM iftest
