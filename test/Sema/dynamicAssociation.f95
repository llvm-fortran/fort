! RUN: %fort -fsyntax-only -verify %s
! RUN: %fort -fsyntax-only -verify -ast-print %s 2>&1 | FileCheck %s
program a
  implicit none
  integer, allocatable :: a(10), c(5) ! CHECK: integer, allocatable array a
  continue ! CHECK: integer, allocatable array c
  allocate(b(3)) ! expected-error {{use of undeclared identifier 'b'}}
  allocate(a(10), c(5)) ! CHECK: allocate(a(10), c(5))
  deallocate(b) ! expected-error {{use of undeclared identifier 'b'}}
  deallocate(a,c) ! CHECK: deallocate(a, c)
end program
