! RUN: %flang -verify < %s
PROGRAM constants
  COMPLEX C
  C = (1.0,2.0)
  C = (2.343,1E-4)
  C = (4,3)
  C = (-1,0)

  C = (1, .false.) ! expected-error {{expected an integer or a real constant expression}}
END PROGRAM constants
