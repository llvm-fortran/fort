! RUN: %flang -verify -fsyntax-only < %s

SUBROUTINE equivtest(JJ) ! expected-note {{'jj' is an argument defined here}}
  REAL A, B
  EQUIVALENCE (A,B)
  INTEGER I, J, JJ, K, M, N, O(3), I_ARR(3,3)
  CHARACTER*10 STR, STR_ARR(10)
  INTEGER(Kind=8) FOO
  PARAMETER(II = 111) ! expected-note {{'ii' is a parameter constant defined here}}
  EQUIVALENCE (A, I), (B, K)

  EQUIVALENCE (I, I_ARR)
  EQUIVALENCE (STR(2:), STR_ARR)

  EQUIVALENCE (I, O(1))
  EQUIVALENCE (N, O(I)) ! expected-error {{statement requires a constant expression}}

  EQUIVALENCE (II, I) ! expected-error {{specification statement requires a local variable}}
  EQUIVALENCE (I, jj) ! expected-error {{specification statement requires a local variable}}

  EQUIVALENCE (I, 22) ! expected-error {{specification statement requires a variable or an array element expression}}

  EQUIVALENCE (I, STR) ! expected-error {{expected an expression of integer, real, complex or logical type ('character (Len=10)' invalid)}}
  EQUIVALENCE (STR, A) ! expected-error {{expected an expression of character type ('real' invalid)}}
  EQUIVALENCE (I, FOO) ! expected-error {{expected an expression with default type kind ('integer (Kind=8)' invalid)}}


END
