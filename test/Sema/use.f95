! RUN: %fort -fsyntax-only -verify %s
module mod1
  integer :: n
  integer :: m
  integer :: s
end module
program p
  use mdl ! expected-error {{unknown module 'mdl'}}
  use :: mod1, only: m
  implicit none

  n = 1 ! expected-error {{use of undeclared identifier 'n'}}
  m = 2
end program
