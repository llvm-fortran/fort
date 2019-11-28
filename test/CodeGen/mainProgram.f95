! RUN: %fort -emit-llvm -o - %s | FileCheck %s
PROGRAM test ! CHECK: define i32 @main
  CONTINUE   ! CHECK: br label
END PROGRAM  ! CHECK: ret i32 0
