! RUN: %fort -fdefault-integer-8 -emit-llvm -o - %s | FileCheck %s

program test
  integer i ! CHECK: alloca i64
  logical l ! CHECK-NEXT: alloca i64
end
