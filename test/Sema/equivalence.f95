! RUN: %flang -verify -fsyntax-only < %s

SUBROUTINE equivtest(JJ) ! expected-note {{'jj' is an argument defined here}}
  REAL A, B
  EQUIVALENCE (A,B) ! expected-note {{an identical association was already created here}}
  INTEGER I, J, JJ, K, M, N, O(3), I_ARR(3,3)
  CHARACTER*10 STR, STR_ARR(10)
  INTEGER(Kind=8) FOO
  PARAMETER(II = 111) ! expected-note {{'ii' is a parameter constant defined here}}
  INTEGER II ! expected-note@+1 {{an identical association was already created here}}
  EQUIVALENCE (A, I), (B, K) ! expected-note {{an identical association was already created here}}

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

  EQUIVALENCE (A, A) ! expected-warning {{this equivalence connection uses the same object}}
  EQUIVALENCE (A, B) ! expected-warning {{redundant equivalence connection}}

  EQUIVALENCE (I, A) ! expected-warning {{redundant equivalence connection}}
  EQUIVALENCE (A, I) ! expected-warning {{redundant equivalence connection}}

END

SUBROUTINE foo()
  REAL A
  INTEGER I, I_ARR(3,3) ! expected-note@+1 {{an identical association was already created here}}
  EQUIVALENCE (A, I) ! expected-note {{an identical association was already created here}}
  EQUIVALENCE (I, I_ARR)
  EQUIVALENCE (I, A) ! expected-warning {{redundant equivalence connection}}
  EQUIVALENCE (A, I) ! expected-warning {{redundant equivalence connection}}
END
