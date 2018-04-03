! RUN: %fort -fsyntax-only -verify %s
! RUN: %fort -fsyntax-only -verify %s -ast-print 2>&1 | %file_check %s
module e   ! CHECK: module e
end module ! CHECK: end module
module c   ! CHECK: module c
  contains
end module ! CHECK: end module
module mod ! CHECK: module mod
  type point
    real :: x, y
  end type
  ! CHECK: integer n = 10
  ! TODO:  integer, parameter :: n = 10
  integer n
  parameter(n = 10)
  contains
  subroutine sub(x) ! CHECK: subroutine sub(x)
    integer, intent(out) :: x ! CHECK: integer x
      x = 1 ! CHECK: x = 1
  end subroutine ! CHECK: end subroutine
  function r(a) ! CHECK: real function r(a)
    implicit none
    type(point), intent(in) :: a ! CHECK: type point a
    real :: r
    r = sqrt(a%x**2 + a%y**2)
  end function
end module ! CHECK: end module
module ! expected-error {{expected identifier after 'module'}}
end
