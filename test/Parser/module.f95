! RUN: %fort -fsyntax-only -verify %s
module e
end module
module c
  contains
end module
module mod
  ! TODO:  integer, parameter :: n = 10
  integer n
  parameter(n = 10)
  contains
  subroutine sub(x)
    integer, intent(out) :: x
      x = 1
  end subroutine
end module
module ! expected-error {{expected identifier after 'module'}}
end
