! RUN: %flang -verify < %s
PROGRAM vartest

  INTEGER :: I ! expected-note {{previous definition is here}}
  INTEGER :: I ! expected-error {{redefinition of 'I'}}

  INTEGER :: X ! expected-note {{previous definition is here}}
  REAL :: X ! expected-error {{redefinition of 'X'}}

  I = K ! expected-error {{use of undeclared identifier 'K'}}

END PROGRAM
