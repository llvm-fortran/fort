! RUN: %flang -verify < %s
! RUN: %flang -verify %s 2>&1 | %file_check %s
PROGRAM assignment
  IMPLICIT NONE
  INTEGER I
  REAL  R
  DOUBLE PRECISION D
  COMPLEX C
  LOGICAL L
  CHARACTER * 10 CHARS

  I = 1
  R = 1.0
  D = 1.0D1
  C = (1.0,2.0)
  L = .false.
  CHARS = 'STRING'

  I = I ! CHECK: I = I
  I = R ! CHECK: I = INT(R)
  I = D ! CHECK: I = INT(D)
  I = C ! CHECK: I = INT(C)
  I = L ! expected-error{{assigning to 'INTEGER' from incompatible type 'LOGICAL'}}
  I = CHARS ! expected-error{{assigning to 'INTEGER' from incompatible type 'CHARACTER (LEN=10)'}}

  R = I ! CHECK: R = REAL(I)
  R = R ! CHECK: R = R
  R = D ! CHECK: R = REAL(D)
  R = C ! CHECK: R = REAL(C)
  R = L ! expected-error{{assigning to 'REAL' from incompatible type 'LOGICAL'}}
  R = CHARS ! expected-error{{assigning to 'REAL' from incompatible type 'CHARACTER (LEN=10)'}}

  D = I ! CHECK: D = REAL(I,Kind=8)
  D = R ! CHECK: D = REAL(R,Kind=8)
  D = D ! CHECK: D = D
  D = C ! CHECK: D = REAL(C,Kind=8)
  D = L ! expected-error{{assigning to 'DOUBLE PRECISION' from incompatible type 'LOGICAL'}}
  D = CHARS ! expected-error{{assigning to 'DOUBLE PRECISION' from incompatible type 'CHARACTER (LEN=10)'}}

  C = I ! CHECK: C = CMPLX(I)
  C = R ! CHECK: C = CMPLX(R)
  C = D ! CHECK: C = CMPLX(D)
  C = C ! CHECK: C = C
  C = L ! expected-error{{assigning to 'COMPLEX' from incompatible type 'LOGICAL'}}
  C = CHARS ! expected-error{{assigning to 'COMPLEX' from incompatible type 'CHARACTER (LEN=10)'}}

  L = L ! CHECK: L = L
  L = I ! expected-error{{assigning to 'LOGICAL' from incompatible type 'INTEGER'}}
  L = R ! expected-error{{assigning to 'LOGICAL' from incompatible type 'REAL'}}
  L = D ! expected-error{{assigning to 'LOGICAL' from incompatible type 'DOUBLE PRECISION'}}
  L = C ! expected-error{{assigning to 'LOGICAL' from incompatible type 'COMPLEX'}}
  L = CHARS ! expected-error{{assigning to 'LOGICAL' from incompatible type 'CHARACTER (LEN=10)'}}

  CHARS = CHARS ! CHECK: CHARS = CHARS
  CHARS = I ! expected-error{{assigning to 'CHARACTER (LEN=10)' from incompatible type 'INTEGER'}}
  CHARS = R ! expected-error{{assigning to 'CHARACTER (LEN=10)' from incompatible type 'REAL'}}
  CHARS = D ! expected-error{{assigning to 'CHARACTER (LEN=10)' from incompatible type 'DOUBLE PRECISION'}}
  CHARS = C ! expected-error{{assigning to 'CHARACTER (LEN=10)' from incompatible type 'COMPLEX'}}
  CHARS = L ! expected-error{{assigning to 'CHARACTER (LEN=10)' from incompatible type 'LOGICAL'}}

END PROGRAM assignment
