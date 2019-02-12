! RUN: %fort -fsyntax-only -verify %s
program a
  ! TODO defferred shape
  integer, allocatable :: a(10), b(10, 10), c(10,10,10)
  allocate(8) ! expected-error {{expected identifier}}
  allocate(a(10), ! expected-error {{expected identifier}}
  allocate(b(10,10) ! expected-error {{expected ')'}}
  allocate(a) ! expected-error {{expected '('}}
  continue ! expected-error@-1 {{expected array specification}}
  deallocate(.5) ! expected-error {{expected identifier}}
  deallocate(c ! expected-error {{expected ')'}}
  deallocate(a,) ! expected-error {{expected identifier}}
end program
