! RUN: %flang -fsyntax-only -verify < %s
! RUN: %flang -fsyntax-only -verify -ast-print %s 2>&1 | %file_check %s
! This is more focused on double complex support and some
! obscure intrinsic overloads used by BLAS
PROGRAM doubletest
  IMPLICIT NONE

  DOUBLE PRECISION dbl
  DOUBLE COMPLEX dc
  COMPLEX*16 dcc

  REAL r
  COMPLEX c

  INTRINSIC DCMPLX, CDABS, DCONJG, DIMAG

  r = dbl ! CHECK: r = REAL(dbl)
  c = dc  ! CHECK: c = CMPLX(dc)
  dbl = r ! CHECK: dbl = REAL(r,Kind=8)
  dc = c  ! CHECK: dc = CMPLX(c,Kind=8)

  dc = (1,2) ! CHECK: dc = CMPLX((1,2),Kind=8)
  ! create a double precision complex when a part has double precision
  dc = (1D1,2) ! CHECK: dc = (10,2)
  dc = (0,20D-1) ! CHECK: dc = (0,2)
  dc = (1d1, 25d-1) ! CHECK: dc = (10,2.5)

  dc = c + dc + r ! CHECK: dc = ((CMPLX(c,Kind=8)+dc)+CMPLX(r,Kind=8))
  dbl = r + dbl ! CHECK: dbl = (REAL(r,Kind=8)+dbl)

  dc = DCMPLX(r) ! CHECK: dc = dcmplx(r)
  r = CDABS(dc) ! CHECK: r = REAL(cdabs(dc))
  dc = DCONJG(dc) ! CHECK: dc = dconjg(dc)
  dc = DCONJG(c) ! expected-error{{passing 'COMPLEX' to parameter of incompatible type 'DOUBLE COMPLEX'}}

  dbl = DIMAG(dc) ! CHECK: dbl = dimag(dc)

  dc = dc/dbl ! CHECK: dc = (dc/CMPLX(dbl,Kind=8))

  dc = dcc ! CHECK: dc = dcc

END PROGRAM
