! RUN: %fort -fsyntax-only -verify < %s
! RUN: %fort -fsyntax-only -verify -ast-print %s 2>&1 | FileCheck %s
PROGRAM init
  INTEGER n = 1 ! expected-error {{expected line break or ';' at end of statement}}
  INTEGER var ! expected-note@+1 {{this expression is not allowed in a constant expression}}
  INTEGER :: a = var + 1 ! expected-error {{'a' must be initialized by a constant expression}}
  INTEGER, SAVE :: b = 2
  REAL, PARAMETER :: p = 3.5 ! CHECK: real, parameter, save p = 3.5
  REAL :: q = .false. ! expected-error {{initializing 'real' with an expression of incompatible type 'logical'}}
  REAL, PARAMETER :: r ! expected-error {{'r': 'parameter' attribute requires explicit initializer}}
  ! FIXME should this be evaluated to 4.5 at compile time?
  REAL, PARAMETER :: s = p + 1 ! CHECK: real, parameter, save s = (p+real(1))
  INTEGER :: j = 10, k = 3 + 2 ! CHECK: integer, save j = 10
  ! CHECK: integer, save k = (3+2)

END PROGRAM init
