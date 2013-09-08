! RUN: %flang -verify -fsyntax-only < %s
PROGRAM typetest

  INTEGER Bar ! expected-note {{previous definition is here}}
  TYPE Point
    REAL X, Y
  END TYPE Point

  TYPE foo
    INTEGER K
  END TYPE bar ! expected-error {{expected type name 'foo'}}

  type Bar ! expected-error {{redefinition of 'bar'}}
    real x
  endtype
END PROGRAM typetest
