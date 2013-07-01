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
