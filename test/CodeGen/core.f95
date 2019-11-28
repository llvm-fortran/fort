! RUN: %fort -emit-llvm -o - %s | FileCheck %s
PROGRAM test
  STOP       ! CHECK: call void @libfort_stop()
END PROGRAM
