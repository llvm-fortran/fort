! RUN: %flang -fsyntax-only -verify < %s
! RUN: %flang -fsyntax-only -verify -ast-print %s 2>&1 | %file_check %s
PROGRAM assignment
  IMPLICIT NONE
  INTEGER i
  REAL  r
  DOUBLE PRECISION d
  COMPLEX c
  LOGICAL l
  CHARACTER * 10 chars

  i = 1
  r = 1.0
  d = 1.0D1
  c = (1.0,2.0)
  l = .false.
  chars = 'STRING'

  i = i ! CHECK: i = i
  i = r ! CHECK: i = INT(r)
  i = d ! CHECK: i = INT(d)
  i = c ! CHECK: i = INT(c)
  i = l ! expected-error{{assigning to 'INTEGER' from incompatible type 'LOGICAL'}}
  i = chars ! expected-error{{assigning to 'INTEGER' from incompatible type 'CHARACTER (LEN=10)'}}

  r = i ! CHECK: r = REAL(i)
  r = r ! CHECK: r = r
  r = d ! CHECK: r = REAL(d)
  r = c ! CHECK: r = REAL(c)
  r = l ! expected-error{{assigning to 'REAL' from incompatible type 'LOGICAL'}}
  r = chars ! expected-error{{assigning to 'REAL' from incompatible type 'CHARACTER (LEN=10)'}}

  d = i ! CHECK: d = REAL(i,Kind=8)
  d = r ! CHECK: d = REAL(r,Kind=8)
  d = d ! CHECK: d = d
  d = c ! CHECK: d = REAL(c,Kind=8)
  d = l ! expected-error{{assigning to 'DOUBLE PRECISION' from incompatible type 'LOGICAL'}}
  d = chars ! expected-error{{assigning to 'DOUBLE PRECISION' from incompatible type 'CHARACTER (LEN=10)'}}

  c = i ! CHECK: c = CMPLX(i)
  c = r ! CHECK: c = CMPLX(r)
  c = d ! CHECK: c = CMPLX(d)
  c = c ! CHECK: c = c
  c = l ! expected-error{{assigning to 'COMPLEX' from incompatible type 'LOGICAL'}}
  c = chars ! expected-error{{assigning to 'COMPLEX' from incompatible type 'CHARACTER (LEN=10)'}}

  l = l ! CHECK: l = l
  l = i ! expected-error{{assigning to 'LOGICAL' from incompatible type 'INTEGER'}}
  l = r ! expected-error{{assigning to 'LOGICAL' from incompatible type 'REAL'}}
  l = d ! expected-error{{assigning to 'LOGICAL' from incompatible type 'DOUBLE PRECISION'}}
  l = c ! expected-error{{assigning to 'LOGICAL' from incompatible type 'COMPLEX'}}
  l = chars ! expected-error{{assigning to 'LOGICAL' from incompatible type 'CHARACTER (LEN=10)'}}

  chars = chars ! CHECK: chars = chars
  chars = i ! expected-error{{assigning to 'CHARACTER (LEN=10)' from incompatible type 'INTEGER'}}
  chars = r ! expected-error{{assigning to 'CHARACTER (LEN=10)' from incompatible type 'REAL'}}
  chars = d ! expected-error{{assigning to 'CHARACTER (LEN=10)' from incompatible type 'DOUBLE PRECISION'}}
  chars = c ! expected-error{{assigning to 'CHARACTER (LEN=10)' from incompatible type 'COMPLEX'}}
  chars = l ! expected-error{{assigning to 'CHARACTER (LEN=10)' from incompatible type 'LOGICAL'}}

END PROGRAM assignment
