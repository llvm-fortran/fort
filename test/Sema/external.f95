! RUN: %flang -verify < %s
PROGRAM exttest
  EXTERNAL SUB

  INTEGER FUNC
  REAL FUNC2
  EXTERNAL FUNC, FUNC2 ! expected-note {{previous definition is here}}

  INTEGER FUNC ! expected-error {{redefinition of 'FUNC'}}

END PROGRAM
