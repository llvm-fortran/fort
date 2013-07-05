! RUN: %flang %s 2>&1 | %file_check %s
PROGRAM dowhiletest
  INTEGER I
  INTEGER J
  REAL R, Z

  J = 1
  DO I = 1, 10 ! CHECK: store i32 1, i32*
    J = J * I  ! CHECK: br label
  END DO       ! CHECK: load i32*
  CONTINUE     ! CHECK: icmp sle
  CONTINUE     ! CHECK: br i1
  CONTINUE     ! CHECK: mul i32
  CONTINUE     ! CHECK: br label
  CONTINUE     ! CHECK: load i32*
  CONTINUE     ! CHECK: add
  CONTINUE     ! CHECK: store i32
  CONTINUE     ! CHECK: br label

END PROGRAM
