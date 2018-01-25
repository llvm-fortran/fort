! RUN: %fort %s -S -emit-llvm -o - | %file_check %s
module mod
  contains
  subroutine sub(x) ! CHECK: define void @__mod_MOD_sub_(i32* noalias %x)
    integer, intent(out) :: x
      x = 1 ! CHECK: store i32 1, i32* %x
  end subroutine
  integer function f(x) ! CHECK: define i32 @__mod_MOD_f_(i32* noalias %x)
    integer, intent(in) :: x
      f = x + 5
  end function
end module
