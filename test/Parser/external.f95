! RUN: %flang -verify < %s
PROGRAM exttest
  EXTERNAL FUNC, FUNC2

  EXTERNAL X Y ! expected-error {{expected ','}}
  EXTERNAL 'ABS' ! expected-error {{expected identifier}}

END PROGRAM
