! RUN: %fort -fsyntax-only -verify %s
module mod1
  integer :: n
  integer :: m
  integer :: s
end module
program p
  use mdl ! expected-error {{unknown module 'mdl'}}
  use :: mod1, only: m, grand_total => s
  implicit none

  n = 1 ! expected-error {{use of undeclared identifier 'n'}}
  s = 5 ! expected-error {{use of undeclared identifier 's'}}
  m = 2
  grand_total = 5
end program
