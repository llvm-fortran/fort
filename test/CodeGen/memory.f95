! RUN: %fort %s -S -emit-llvm -o - | %file_check %s
program p
  integer, allocatable :: a(10), b(5) ! CHECK: %a = alloca i32*
  continue                            ! CHECK: %b = alloca i32*
  deallocate(a,b) ! CHECK: load {{.*}} %a
  continue        ! CHECK: call void @libfort_free
  continue        ! CHECK: load {{.*}} %b
  continue        ! CHECK: call void @libfort_free
end program
