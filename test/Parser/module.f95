! RUN: %fort -fsyntax-only -verify %s
! RUN: %fort -fsyntax-only -verify %s -ast-print 2>&1 | FileCheck %s
module e   ! CHECK: module e
end module ! CHECK: end module
module c   ! CHECK: module c
  contains
end module ! CHECK: end module
module mod ! CHECK: module mod
  type point
    real :: x, y
  end type
  ! CHECK: integer, parameter, save n = 10
  integer, parameter :: n = 10
  ! CHECK: integer m = 20
  integer m
  parameter(m = 20)
  contains
  subroutine sub(x) ! CHECK: subroutine sub(x)
    integer, intent(out) :: x ! CHECK: integer, intent(out) x
      x = 1 ! CHECK: x = 1
  end subroutine ! CHECK: end subroutine
  function r(a) ! CHECK: real function r(a)
    implicit none
    type(point), intent(in) :: a ! CHECK: type point, intent(in) a
    real :: r
    r = sqrt(a%x**2 + a%y**2)
  end function
end module ! CHECK: end module
module ! expected-error {{expected identifier after 'module'}}
end
