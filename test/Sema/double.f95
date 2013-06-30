! RUN: %flang -verify < %s
! RUN: %flang -verify %s 2>&1 | %file_check %s
! This is more focused on double complex support and some
! obscure intrinsic overloads used by BLAS
PROGRAM doubletest
  IMPLICIT NONE

  DOUBLE PRECISION DBL
  DOUBLE COMPLEX DC

  REAL R
  COMPLEX C

  INTRINSIC DCMPLX, CDABS, DCONJG

  R = DBL ! CHECK: R = REAL(DBL)
  C = DC  ! CHECK: C = CMPLX(DC)
  DBL = R ! CHECK: DBL = REAL(R,Kind=8)
  DC = C  ! CHECK: DC = CMPLX(C,Kind=8)

  DC = (1,2) ! CHECK: DC = CMPLX((1,2),Kind=8)
  ! create a double precision complex when a part has double precision
  DC = (1D1,2) ! CHECK: DC = (10,2)
  DC = (0,20D-1) ! CHECK: DC = (0,2)
  DC = (1d1, 25d-1) ! CHECK: DC = (10,2.5)

  DC = C + DC + R ! CHECK: DC = ((CMPLX(C,Kind=8)+DC)+CMPLX(R,Kind=8))
  DBL = R + DBL ! CHECK: DBL = (REAL(R,Kind=8)+DBL)

  DC = DCMPLX(R) ! CHECK: DC = DCMPLX(R)
  R = CDABS(DC) ! CHECK: R = REAL(CDABS(DC))
  DC = DCONJG(DC) ! CHECK: DC = DCONJG(DC)
  DC = DCONJG(C) ! expected-error{{passing 'COMPLEX' to parameter of incompatible type 'DOUBLE COMPLEX'}}

END PROGRAM
