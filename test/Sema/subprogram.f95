! RUN: %flang -verify < %s

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
END

FUNCTION FUNC3()
  INTEGER FUNC3
  INTEGER FUNC3 ! expected-error {{the return type for a function 'FUNC3' was already specified}}
  FUNC3 = 1
END

REAL FUNCTION FUNC4()
  INTEGER FUNC4 ! expected-error {{the return type for a function 'FUNC4' was already specified}}
  FUNC4 = 22
  FUNC4 = .false. ! expected-error {{assigning to 'REAL' from incompatible type 'LOGICAL'}}
END
