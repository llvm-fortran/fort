! RUN: %flang %s 2>&1 | %file_check %s
PROGRAM test
  INTEGER X ! CHECK: %0 = alloca i32
  REAL Y    ! CHECK: %1 = alloca float

  X = X     ! CHECK: load i32* %0
  X = +X    ! CHECK: load i32* %0
  X = -X    ! CHECK: sub i32 0

  X = X + X ! CHECK: add i32
  X = X - X ! CHECK: sub i32
  X = X * X ! CHECK: mul i32
  X = X / X ! CHECK: sdiv i32

  Y = Y     ! CHECK: load float* %1
  Y = -Y    ! CHECK: fsub

  Y = Y + Y ! CHECK: fadd float
  Y = Y - Y ! CHECK: fsub float
  Y = Y * Y ! CHECK: fmul float
  Y = Y / Y ! CHECK: fdiv float
END PROGRAM
