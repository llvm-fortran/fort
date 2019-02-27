! RUN: %fort -fsyntax-only -verify %s
program a
  implicit none
  integer, allocatable :: a(10)
  deallocate(b) ! expected-error {{use of undeclared identifier 'b'}}
  deallocate(a)
end program
