! RUN: %flang -verify < %s
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

  I = I
  I = R
  I = D
  I = C
  I = L ! expected-error{{assigning to 'INTEGER' from incompatible type 'LOGICAL'}}
  I = CHARS ! expected-error{{assigning to 'INTEGER' from incompatible type 'CHARACTER (LEN=10)'}}

  R = I
  R = R
  R = D
  R = C
  R = L ! expected-error{{assigning to 'REAL' from incompatible type 'LOGICAL'}}
  R = CHARS ! expected-error{{assigning to 'REAL' from incompatible type 'CHARACTER (LEN=10)'}}

  D = I
  D = R
  D = D
  D = C
  D = L ! expected-error{{assigning to 'DOUBLE PRECISION' from incompatible type 'LOGICAL'}}
  D = CHARS ! expected-error{{assigning to 'DOUBLE PRECISION' from incompatible type 'CHARACTER (LEN=10)'}}

  C = I
  C = R
  C = D
  C = C
  C = L ! expected-error{{assigning to 'COMPLEX' from incompatible type 'LOGICAL'}}
  C = CHARS ! expected-error{{assigning to 'COMPLEX' from incompatible type 'CHARACTER (LEN=10)'}}

  L = L
  L = I ! expected-error{{assigning to 'LOGICAL' from incompatible type 'INTEGER'}}
  L = R ! expected-error{{assigning to 'LOGICAL' from incompatible type 'REAL'}}
  L = D ! expected-error{{assigning to 'LOGICAL' from incompatible type 'DOUBLE PRECISION'}}
  L = C ! expected-error{{assigning to 'LOGICAL' from incompatible type 'COMPLEX'}}
  L = CHARS ! expected-error{{assigning to 'LOGICAL' from incompatible type 'CHARACTER (LEN=10)'}}

  CHARS = CHARS
  CHARS = I ! expected-error{{assigning to 'CHARACTER (LEN=10)' from incompatible type 'INTEGER'}}
  CHARS = R ! expected-error{{assigning to 'CHARACTER (LEN=10)' from incompatible type 'REAL'}}
  CHARS = D ! expected-error{{assigning to 'CHARACTER (LEN=10)' from incompatible type 'DOUBLE PRECISION'}}
  CHARS = C ! expected-error{{assigning to 'CHARACTER (LEN=10)' from incompatible type 'COMPLEX'}}
  CHARS = L ! expected-error{{assigning to 'CHARACTER (LEN=10)' from incompatible type 'LOGICAL'}}

END PROGRAM assignment
