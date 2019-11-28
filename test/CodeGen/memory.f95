! RUN: %fort %s -O0 -S -emit-llvm -o - | FileCheck %s
program p
  integer(kind=4), allocatable :: a(10), b(5) ! CHECK: %a = alloca i32*
  continue                                    ! CHECK: %b = alloca i32*
  allocate(a(10), b(5)) ! CHECK: @libfort_malloc({{.*}}40)
  continue              ! CHECK-NEXT: bitcast i8* {{.*}} to i32*
  continue              ! CHECK-NEXT: store i32* {{.*}}, i32** %a
  continue              ! CHECK: @libfort_malloc({{.*}}20)
  continue              ! CHECK-NEXT: bitcast i8* {{.*}} to i32*
  continue              ! CHECK-NEXT: store i32* {{.*}}, i32** %b
  deallocate(a,b) ! CHECK: load {{.*}} %a
  continue        ! CHECK: call void @libfort_free
  continue        ! CHECK: load {{.*}} %b
  continue        ! CHECK: call void @libfort_free
end program
