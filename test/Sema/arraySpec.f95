! RUN: %flang -verify < %s
! RUN: %flang -verify %s 2>&1 | %file_check %s

SUBROUTINE SUB(ARR, ARR2, ARR3)
  INTEGER ARR(*)
  INTEGER ARR2(*,*) ! expected-error {{the dimension declarator '*' must be used only in the last dimension}}
  REAL ARR3(10,*)
  INTEGER I

  I = ARR(1) ! CHECK: I = ARR(1)
  I = ARR3(3,2) ! CHECK: I = INT(ARR3(3, 2))
END

PROGRAM arrtest
  INTEGER I_ARR(30, 10:20, 20)
  INTEGER I_ARR2(I_ARR(1,2,3)) ! expected-error {{expected an integer constant expression}}
  INTEGER I_ARR3(.false.:2) ! expected-error {{expected an integer constant expression}}
ENDPROGRAM arrtest
