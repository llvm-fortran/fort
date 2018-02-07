! RUN: %fort %s -S -emit-llvm -o - | %file_check %s
module m
  integer n ! CHECK: @__m_MOD_n_ = internal global i32
  contains
  function f(x) result(r)
    integer :: x
    integer :: r
    r = x * 2
  end function
end module
program p
  use m
  implicit none 
  n = 4 ! CHECK: store i32 4, i32* @__m_MOD_n_
  n = f(n) ! CHECK: call i32 @__m_MOD_f_(i32* @__m_MOD_n_)
  print *, n
end program
