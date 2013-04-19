! RUN: %flang < %s
PROGRAM iftest
  CHARACTER (LEN=11) :: C
  IF(1 == 1) C = "YES"
END PROGRAM iftest
