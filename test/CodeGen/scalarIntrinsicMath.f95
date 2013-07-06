! RUN: %flang %s 2>&1 | %file_check %s
PROGRAM testscalarmath
  INTEGER i
  REAL x

  INTRINSIC abs, mod
  INTRINSIC sqrt, exp, log, log10
  INTRINSIC sin, cos, tan
  INTRINSIC asin, acos, atan2
  INTRINSIC sinh, cosh, tanh

  x = 1.0
  i = 7

  i = abs(i)      ! CHECK: select i1
  i = mod(13, i)  ! CHECK: srem i32 13

  x = abs(x)      ! CHECK: call float @llvm.fabs.f32
  x = mod(4.0, x) ! CHECK: frem float 4

  x = sqrt(x)     ! CHECK: call float @llvm.sqrt.f32
  x = exp(x)      ! CHECK: call float @llvm.exp.f32
  x = log(x)      ! CHECK: call float @llvm.log.f32
  x = log10(x)    ! CHECK: call float @llvm.log10.f32
  x = sin(x)      ! CHECK: call float @llvm.sin.f32
  x = cos(x)      ! CHECK: call float @llvm.cos.f32

END
