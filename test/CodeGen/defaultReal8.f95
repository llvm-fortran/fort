! RUN: %fort -fdefault-real-8 -emit-llvm -o - %s | FileCheck %s

program test
  real x             ! CHECK:      alloca double
  double precision y ! CHECK-NEXT: alloca fp128
  integer i          ! CHECK-NEXT: alloca i32
end
