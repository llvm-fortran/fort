! RUN: %flang %s 2>&1 | %file_check %s
PROGRAM testcomplexintrinsicmath
  COMPLEX c
  INTRINSIC abs, sqrt, sin, cos, log, exp

  c = (1.0, 0.0)

  c = abs(c)   ! CHECK: call { float, float } @cabsf
  c = sqrt(c)  ! CHECK: call { float, float } @csqrtf
  c = sin(c)   ! CHECK: call { float, float } @csinf
  c = cos(c)   ! CHECK: call { float, float } @ccosf
  c = log(c)   ! CHECK: call { float, float } @clogf
  c = exp(c)   ! CHECK: call { float, float } @cexpf

END
