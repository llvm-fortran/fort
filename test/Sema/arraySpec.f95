! RUN: %flang -fsyntax-only -verify < %s
! RUN: %flang -fsyntax-only -verify -ast-print %s 2>&1 | %file_check %s

SUBROUTINE SUB(ARR, ARR2, ARR3)
  INTEGER ARR(*)
  INTEGER ARR2(*,*) ! expected-error {{the dimension declarator '*' must be used only in the last dimension}}
  REAL ARR3(10,*)
  INTEGER I

  I = ARR(1) ! CHECK: I = ARR(1)
  I = ARR3(3,2) ! CHECK: I = INT(ARR3(3, 2))
END

SUBROUTINE BUS(L, ARR, ARR2)
  INTEGER L
  INTEGER I
  REAL ARR2(L)
  DIMENSION ARR(L)

  ! ARR is REAL by implicit
  I = ARR(1) ! CHECK: I = INT(ARR(1))
  I = ARR2(1) ! CHECK: I = INT(ARR2(1))
END

SUBROUTINE USB(LENGTH)
  INTEGER LENGTH
  INTEGER M_ARM  ! expected-note {{declared here}}
  DIMENSION M_ARM(*) ! expected-error {{use of dimension declarator '*' for a local variable 'M_ARM'}}

  REAL X_ARM(LENGTH)
  INTEGER I
  I = LENGTH
  X_ARM(1) = 1.0
END

PROGRAM arrtest
  INTEGER I_ARR(30, 10:20, 20)
  INTEGER I_ARR2(I_ARR(1,2,3)) ! expected-error {{expected an integer constant expression}}
  INTEGER I_ARR3(.false.:2) ! expected-error {{expected an integer constant expression}}

  INTEGER I_ARRM(*) ! expected-error {{use of dimension declarator '*' for a local variable 'I_ARRM'}}
  INTEGER I ! expected-note@-1 {{declared here}}

ENDPROGRAM arrtest
