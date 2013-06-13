! RUN: %flang < %s
PROGRAM constants
  COMPLEX C
  C = (1.0,2.0)
  C = (2.343,1E-4)
  C = (4,3)
  C = (-1,0)
END PROGRAM constants
