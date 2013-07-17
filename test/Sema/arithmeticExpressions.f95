! RUN: %flang -fsyntax-only -verify < %s
! RUN: %flang -fsyntax-only -verify -ast-print %s 2>&1 | %file_check %s
PROGRAM arithexpressions
  IMPLICIT NONE
  INTEGER i
  REAL  r
  DOUBLE PRECISION d
  COMPLEX c

  i = 0
  r = 1.0
  d = 1.0
  c = (1.0,1.0)

  i = 1 + 1 ! CHECK: (1+1)
  i = i - i ! CHECK: (i-i)
  i = i * i ! CHECK: (i*i)
  i = i / i ! CHECK: (i/i)
  i = i ** 3 ! CHECK: (i**3)

  i = i ** 'pow' ! expected-error {{invalid operands to an arithmetic binary expression ('INTEGER' and 'CHARACTER')}}
  i = i + .false. ! expected-error {{invalid operands to an arithmetic binary expression ('INTEGER' and 'LOGICAL')}}
  i = 'true' * .true. ! expected-error {{invalid operands to an arithmetic binary expression ('CHARACTER' and 'LOGICAL')}}

  r = r + 1.0 ! CHECK: (r+1)

  r = r * i ! CHECK: r = (r*REAL(i))
  d = d - i ! CHECK: d = (d-REAL(i,Kind=8))
  c = c / i ! CHECK: c = (c/CMPLX(i))
  r = r ** I ! CHECK: r = (r**i)
  d = d ** i ! CHECK: d = (d**i)
  c = c ** i ! CHECK: c = (c**i)

  r = i * r ! CHECK: r = (REAL(i)*r)
  r = r - r ! CHECK: r = (r-r)
  d = D / r ! CHECK: d = (d/REAL(r,Kind=8))
  C = c ** r ! CHECK: c = (c**CMPLX(r))

  d = i + d ! CHECK: d = (REAL(i,Kind=8)+d)
  d = r * d ! CHECK: d = (REAL(r,Kind=8)*d)
  d = d - 2.0D1 ! CHECK: d = (d-20)
  ! FIXME: make F77 only
  d = c / d ! expected-error {{invalid operands to an arithmetic binary expression ('COMPLEX' and 'DOUBLE PRECISION')}}

  c = i + c ! CHECK: c = (CMPLX(i)+c)
  c = r - c ! CHECK: c = (CMPLX(r)-c)
  ! FIXME: make F77 only
  c = d * c ! expected-error {{invalid operands to an arithmetic binary expression ('DOUBLE PRECISION' and 'COMPLEX')}}
  c = c / c ! CHECK: (c/c)
  c = c ** r ! CHECK: c = (c**CMPLX(r))

  i = +(i)
  i = -r ! CHECK: i = INT((-r))
  c = -c

  i = +.FALSE. ! expected-error {{invalid argument type 'LOGICAL' to an arithmetic unary expression}}
  r = -'TRUE' ! expected-error {{invalid argument type 'CHARACTER' to an arithmetic unary expression}}


ENDPROGRAM arithexpressions
