! RUN: %flang -fsyntax-only -verify < %s
! RUN: %flang -fsyntax-only -verify -ast-print %s 2>&1 | %file_check %s
PROGRAM intrinfuntest
  INTEGER I
  REAL R
  DOUBLE PRECISION D
  COMPLEX C
  CHARACTER (LEN=100) STRING
  LOGICAL logicalResult

  INTRINSIC INT, IFIX, IDINT
  INTRINSIC REAL, FLOAT, sngl
  INTRINSIC DBLE, cmplx
  INTRINSIC char, ICHAR

  INTRINSIC AINT, dint, anint, DNINT, nint, IDNINT
  INTRINSIC abs, iabs, dabs, cabs
  INTRINSIC mod, sign, dim, dprod, max, min
  INTRINSIC len, len_trim, index
  INTRINSIC aimag, conjg

  intrinsic sqrt, dsqrt, csqrt, exp, dexp, cexp
  intrinsic log, alog, dlog, clog, log10, alog10, dlog10
  intrinsic sin, dsin, csin, cos, dcos, ccos, tan, dtan
  intrinsic asin, dasin, acos, dacos, atan, datan, atan2, datan2
  intrinsic sinh, dsinh, cosh, dcosh, tanh, dtanh

  intrinsic lge, lgt, lle, llt

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
  C = CMPLX(1.0, .false.) ! expected-error {{passing 'LOGICAL' to parameter of incompatible type 'INTEGER' or 'REAL' or 'COMPLEX'}}

  I = ICHAR('HELLO')
  I = ichar(.false.) ! expected-error {{passing 'LOGICAL' to parameter of incompatible type 'CHARACTER'}}

  STRING = CHAR(65)
  STRING = char('TRUTH') ! expected-error {{passing 'CHARACTER' to parameter of incompatible type 'INTEGER'}}

!! misc and maths functions

  R = AINT(R) ! CHECK: R = AINT(R)
  D = AINT(D) ! CHECK: D = AINT(D)
  D = DINT(D) ! CHECK: D = DINT(D)
  D = DINT(R) ! expected-error {{passing 'REAL' to parameter of incompatible type 'DOUBLE PRECISION'}}

  R = ANINT(R) ! CHECK: R = ANINT(R)
  D = ANINT(D) ! CHECK: D = ANINT(D)
  D = DNINT(D) ! CHECK: D = DNINT(D)
  D = DNINT(R) ! expected-error {{passing 'REAL' to parameter of incompatible type 'DOUBLE PRECISION'}}

  I = NINT(R) ! CHECK: I = NINT(R)
  I = NINT(D) ! CHECK: I = NINT(D)
  I = IDNINT(D) ! CHECK: I = IDNINT(D)
  I = IDNINT(R) ! expected-error {{passing 'REAL' to parameter of incompatible type 'DOUBLE PRECISION'}}

  I = ABS(I) ! CHECK: I = ABS(I)
  R = ABS(R) ! CHECK: R = ABS(R)
  D = ABS(D) ! CHECK: D = ABS(D)
  R = ABS(C) ! CHECK: R = ABS(C)
  I = IABS(I) ! CHECK: I = IABS(I)
  D = DABS(D) ! CHECK: D = DABS(D)
  R = CABS(C) ! CHECK: R = CABS(C)

  I = MOD(3,I)     ! CHECK: I = MOD(3, I)
  R = MOD(R, 3.0)  ! CHECK: R = MOD(R, 3)
  R = SIGN(R, 0.0) ! CHECK: R = SIGN(R, 0)
  D = DPROD(R, R)  ! CHECK: D = DPROD(R, R)
  R = max(1.0, R)  ! CHECK: R = MAX(1, R)
  I = min(I, 11)

  I = LEN(STRING) ! CHECK: I = LEN(STRING)
  I = len_trim(STRING) ! CHECK: I = LEN_TRIM(STRING)
  I = LEN(22) ! expected-error {{passing 'INTEGER' to parameter of incompatible type 'CHARACTER'}}
  I = INDEX(STRING, STRING) ! CHECK: I = INDEX(STRING, STRING)

  R = aimag(C) ! CHECK: R = AIMAG(C)
  C = CONJG(C) ! CHECK: C = CONJG(C)
  C = CONJG(R) ! expected-error {{passing 'REAL' to parameter of incompatible type 'COMPLEX'}}

  R = SQRT(R) ! CHECK: R = SQRT(R)
  D = SQRT(D) ! CHECK: D = SQRT(D)
  C = SQRT(C) ! CHECK: C = SQRT(C)

  R = EXP(R) ! CHECK: R = EXP(R)
  D = EXP(D) ! CHECK: D = EXP(D)
  C = EXP(C) ! CHECK: C = EXP(C)

  R = SQRT(.false.) ! expected-error {{passing 'LOGICAL' to parameter of incompatible type 'REAL' or 'COMPLEX'}}

  R = LOG(R) ! CHECK: R = LOG(R)
  D = LOG(D) ! CHECK: D = LOG(D)
  C = LOG(C) ! CHECK: C = LOG(C)

  R = Log10(R) ! CHECK: R = LOG10(R)
  D = Log10(D) ! CHECK: D = LOG10(D)
  C = Log10(C) ! expected-error {{passing 'COMPLEX' to parameter of incompatible type 'REAL'}}

  R = SIN(R) ! CHECK: R = SIN(R)
  D = SIN(D) ! CHECK: D = SIN(D)
  C = SIN(C) ! CHECK: C = SIN(C)

  R = TAN(R) ! CHECK: R = TAN(R)
  D = TAN(D) ! CHECK: D = TAN(D)

  R = ALOG10(R) ! CHECK: R = ALOG10(R)
  D = DLOG10(D) ! CHECK: D = DLOG10(D)
  C = CLOG(C) ! CHECK: C = CLOG(C)

  R = ATAN2(R, 1.0) ! CHECK: R = ATAN2(R, 1)
  D = ATAN2(D, 1D0) ! CHECK: D = ATAN2(D, 1)

!! lexical comparison functions

  logicalResult = lge(STRING, STRING) ! CHECK: LOGICALRESULT = LGE(STRING, STRING)
  logicalResult = lgt(STRING, STRING) ! CHECK: LOGICALRESULT = LGT(STRING, STRING)
  logicalResult = lle(STRING, STRING) ! CHECK: LOGICALRESULT = LLE(STRING, STRING)
  logicalResult = llt(STRING, STRING) ! CHECK: LOGICALRESULT = LLT(STRING, STRING)

END PROGRAM
