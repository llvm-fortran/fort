! RUN: %flang -verify < %s
! RUN: %flang -verify %s 2>&1 | %file_check %s
SUBROUTINE FOO(X,Y)
  REAL X,Y
END

SUBROUTINE BAR(ARG, ARG) ! expected-error {{redefinition of 'ARG'}}
  REAL ARG
END ! expected-note@-2 {{previous definition is here}}

FUNCTION FUNC(ARG) ! expected-note {{previous definition is here}}
  REAL ARG
  REAL ARG ! expected-error {{redefinition of 'ARG'}}
END

FUNCTION FUNC2() ! expected-note {{previous definition is here}}
END

FUNCTION FUNC2() ! expected-error {{redefinition of 'FUNC2'}}
END

SUBROUTINE SUBB ! expected-note {{previous definition is here}}
  INTEGER SUBB ! expected-error {{redefinition of 'SUBB'}}
  REAL FUNC2
  IF(SUBB .EQ. 0) FUNC2 = 0 ! expected-error {{expected a variable}}
END

FUNCTION FUNC3()
  INTEGER FUNC3
  INTEGER FUNC3 ! expected-error {{the return type for a function 'FUNC3' was already specified}}
  FUNC3 = 1
END

REAL FUNCTION FUNC4()
  INTEGER FUNC4 ! expected-error {{the return type for a function 'FUNC4' was already specified}}
  FUNC4 = 22 ! CHECK: FUNC4 = REAL(22)
  FUNC4 = .false. ! expected-error {{assigning to 'REAL' from incompatible type 'LOGICAL'}}
END

FUNCTION FUNC5()
  FUNC5 = 1.0 ! CHECK: FUNC5 = 1
  if(FUNC5 .EQ. 1.0) FUNC5 = 2.0
END

FUNCTION FUNC6()
  FUNC6 = .true. ! expected-error {{assigning to 'REAL' from incompatible type 'LOGICAL'}}
END
