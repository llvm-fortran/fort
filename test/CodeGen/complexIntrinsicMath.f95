! RUN: %fort -emit-llvm -o - -O1 %s | %file_check %s
PROGRAM testcomplexintrinsicmath
  COMPLEX c
  INTRINSIC abs, sqrt, sin, cos, log, exp

  c = (1.0, 0.0)

  c = abs(c)   ! CHECK: call float @libfort_cabsf
  c = sqrt(c)  ! CHECK: call void @libfort_csqrtf(float {{.*}}, float {{.*}}, { float, float }*
  c = sin(c)   ! CHECK: call void @libfort_csinf(float {{.*}}, float {{.*}}, { float, float }*
  c = cos(c)   ! CHECK: call void @libfort_ccosf(float {{.*}}, float {{.*}}, { float, float }*
  c = log(c)   ! CHECK: call void @libfort_clogf(float {{.*}}, float {{.*}}, { float, float }*
  c = exp(c)   ! CHECK: call void @libfort_cexpf(float {{.*}}, float {{.*}}, { float, float }*

END
