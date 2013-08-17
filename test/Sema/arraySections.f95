! RUN: %flang -fsyntax-only -verify < %s

PROGRAM arrtest
  INTEGER I_ARR(5), I_MAT(4,4)

  I_ARR = I_ARR(:)
  I_ARR(1:3) = 2
  I_ARR(2:) = 3
  I_ARR(:4) = 1
  I_ARR(:) = I_ARR(:)

  I_ARR(1:4:2) = 0

  I_MAT(:,:) = 0
  I_MAT(:,1) = 1
  I_MAT(2,1:1) = 2
  I_MAT(:, 1:4:2) = 4

  I_MAT(:) = 0 ! expected-error {{array subscript must have 2 subscript expressions}}

ENDPROGRAM arrtest
