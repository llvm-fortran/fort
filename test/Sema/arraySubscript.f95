! RUN: %flang -verify < %s
PROGRAM arrtest
  INTEGER I_ARR(30)
  INTEGER I_ARR2(30,20)

  I_ARR(1) = I_ARR(1,1) ! expected-error{{array subscript must have 1 subscript value}}
  I_ARR2(1) = I_ARR(1) ! expected-error{{array subscript must have 2 subscript values}}
ENDPROGRAM arrtest
