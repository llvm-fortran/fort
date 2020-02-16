! RUN: %fort -emit-llvm -o - %s | FileCheck %s
! RUN: %fort -S -o - %s
! FIXME ^ full codegen command above needs to use more explicit target(s)
!   and better redirect its output
PROGRAM test ! CHECK: define i32 @main
  CONTINUE   ! CHECK: br label
END PROGRAM  ! CHECK: ret i32 0
