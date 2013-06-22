! RUN: %flang -verify < %s
PROGRAM vartest
  PARAMETER (Y = 2.0) ! expected-note {{previous definition is here}}

  PARAMETER (PI = 3.14) ! expected-note {{previous definition is here}}
  PARAMETER (PI = 4.0) ! expected-error {{redefinition of 'PI'}}

  INTEGER :: I ! expected-note {{previous definition is here}}
  INTEGER :: I ! expected-error {{redefinition of 'I'}}

  INTEGER :: X ! expected-note {{previous definition is here}}
  REAL :: X ! expected-error {{redefinition of 'X'}}

  REAL :: Y ! expected-error {{redefinition of 'Y'}}

  I = K ! expected-error {{use of undeclared identifier 'K'}}

END PROGRAM
