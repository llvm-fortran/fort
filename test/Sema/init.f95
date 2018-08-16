! RUN: %fort -fsyntax-only -verify < %s
! RUN: %fort -fsyntax-only -verify -ast-print %s 2>&1 | %file_check %s
PROGRAM init
  INTEGER n = 1 ! expected-error {{expected line break or ';' at end of statement}}
  INTEGER var ! expected-note@+1 {{this expression is not allowed in a constant expression}}
  INTEGER :: a = var ! expected-error {{'a' must be initialized by a constant expression}}
  INTEGER, SAVE :: b = 2
  REAL, PARAMETER :: p = 3.5 ! CHECK: real p = 3.5
  REAL :: q = .false. ! expected-error {{initializing 'real' with an expression of incompatible type 'logical'}}
  INTEGER :: j = 10, k = 3 + 2 ! CHECK: integer j = 10
  ! CHECK: integer k = (3+2)

END PROGRAM init
