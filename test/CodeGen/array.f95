! RUN: %flang %s 2>&1 | %file_check %s

PROGRAM helloArrays

  REAL R(10)                ! CHECK: alloca [10 x float]
  INTEGER I(-1:5)           ! CHECK: alloca [7 x i32]
  COMPLEX C(10, 0:9, 10)    ! CHECK: alloca [1000 x { float, float }]
  CHARACTER*20 STR(-5:4, 9) ! CHECK: alloca [90 x [20 x i8]]


END
