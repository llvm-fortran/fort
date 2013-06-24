! RUN: %flang -verify < %s
! RUN: %flang -verify %s 2>&1 | %file_check %s
PROGRAM intrinfuntest
  INTEGER I
  REAL R
  DOUBLE PRECISION D
  COMPLEX C
  CHARACTER (LEN=100) STRING

  INTRINSIC INT, IFIX, IDINT
  INTRINSIC REAL, FLOAT, sngl
  INTRINSIC DBLE, cmplx
  INTRINSIC char, ICHAR

!! conversion functions

  I = INT(2.0) ! CHECK: I = INT(2)
  I = int(2.0D1) ! CHECK: I = INT(20)
  I = INT((1,2)) ! CHECK: I = INT((1,2))
  I = ifix(1.0) ! CHECK: I = IFIX(1)
  I = IDINT(4.25D1) ! CHECK: I = IDINT(42.5)
  R = INT(22) ! CHECK: R = REAL(INT(22))

  I = INT() ! expected-error {{too few arguments to intrinsic function call, expected 1, have 0}}
  I = INT(1,2) ! expected-error {{too many arguments to intrinsic function call, expected 1, have 2}}

  I = IFIX(22) ! expected-error {{passing 'INTEGER' to parameter of incompatible type 'REAL'}}
  I = idint(22) ! expected-error {{passing 'INTEGER' to parameter of incompatible type 'DOUBLE PRECISION'}}
  I = int(.true.) ! expected-error {{passing 'LOGICAL' to parameter of incompatible type 'INTEGER' or 'REAL' or 'COMPLEX'}}

  R = REAL(42) ! CHECK: R = REAL(42)
  R = real(1D1) ! CHECK: R = REAL(10)
  R = float(13) ! CHECK: R = FLOAT(13)
  R = SNGL(0d0) ! CHECK: R = SNGL(0)

  R = FLOAT(12.1) ! expected-error {{passing 'REAL' to parameter of incompatible type 'INTEGER'}}
  R = sngl(12) ! expected-error {{passing 'INTEGER' to parameter of incompatible type 'DOUBLE PRECISION'}}

  D = DBLE(I) ! CHECK: D = DBLE(I)
  D = DBLE(R) ! CHECK: D = DBLE(R)
  R = DBLE(I) ! CHECK: R = REAL(DBLE(I))

  C = cmplx(2.0)
  C = CMPLX(33)
  C = CMPLX(1,2)
  C = CMPLX() ! expected-error {{too few arguments to intrinsic function call, expected 1 or 2, have 0}}
  C = CMPLX(1,2,3,4) ! expected-error {{too many arguments to intrinsic function call, expected 1 or 2, have 4}}

  I = ICHAR('HELLO')
  I = ichar(.false.) ! expected-error {{passing 'LOGICAL' to parameter of incompatible type 'CHARACTER'}}

  STRING = CHAR(65)
  STRING = char('TRUTH') ! expected-error {{passing 'CHARACTER' to parameter of incompatible type 'INTEGER'}}

!!

!! math functions

END PROGRAM
