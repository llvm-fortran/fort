! RUN: %fort -emit-llvm -o - %s | %file_check %s
SUBROUTINE sub()
  ! CHECK: @sub_r_ = {{.*}} global float
  ! CHECK: store float 2.500000e+00, float* @sub_r_
  real :: r = 2.5
END

PROGRAM test
  integer :: x = 10
  ! CHECK: %x = alloca i32
  ! CHECK: store i32 10, i32* %x
END PROGRAM
