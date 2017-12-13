! RUN: %fort -emit-llvm -o - %s | %file_check %s

PROGRAM sys ! CHECK: call void @libfort_sys_init()

  REAL ETIME
  INTRINSIC ETIME
  REAL R_ARR(2)
  REAL T

  T = ETIME(R_ARR) ! CHECK: call float @libfort_etimef(float* {{.*}}, float*

END
