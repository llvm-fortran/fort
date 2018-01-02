! RUN: %fort -fsyntax-only %s
module mod
  ! TODO:  integer, parameter :: n = 10
  integer n
  parameter(n = 10)
end module
