! RUN: %flang %s 2>&1 | %file_check %s
PROGRAM test
  INTEGER X ! CHECK: alloca i32
  REAL Y    ! CHECK: alloca float
  LOGICAL L ! CHECK: alloca i1

  X = X     ! CHECK: load i32*
  X = +X    ! CHECK: load i32*
  X = -X    ! CHECK: sub i32 0

  X = X + X ! CHECK: add i32
  X = X - X ! CHECK: sub i32
  X = X * X ! CHECK: mul i32
  X = X / X ! CHECK: sdiv i32

  Y = Y     ! CHECK: load float*
  Y = -Y    ! CHECK: fsub

  Y = Y + Y ! CHECK: fadd float
  Y = Y - Y ! CHECK: fsub float
  Y = Y * Y ! CHECK: fmul float
  Y = Y / Y ! CHECK: fdiv float

  X = 1 + X   ! CHECK: add i32 1
  Y = 2.5 * Y ! CHECK: fmul float 2.5

  L = 1 .EQ. X ! CHECK: icmp eq i32 1
  L = 0 .NE. X ! CHECK: icmp ne i32 0
  L = 42 .LE. X ! CHECK: icmp sle i32 42
  L = X .LT. X ! CHECK: icmp slt i32
  L = X .GE. X ! CHECK: icmp sge i32
  L = X .GT. X ! CHECK: icmp sgt i32

  L = 1.0 .EQ. Y ! CHECK: fcmp ueq float 1
  L = 0.0 .NE. Y ! CHECK: fcmp une float 0
  L = Y .LE. Y   ! CHECK: fcmp ule float
  L = Y .LT. Y   ! CHECK: fcmp ult float
  L = Y .GE. Y   ! CHECK: fcmp uge float
  L = Y .GT. Y   ! CHECK: fcmp ugt float

END PROGRAM
