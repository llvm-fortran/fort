! RUN: %flang %s 2>&1 | %file_check %s
PROGRAM test
  INTEGER X    ! CHECK: alloca i32
  REAL Y       ! CHECK: alloca float
END PROGRAM
