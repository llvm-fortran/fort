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

  type person
    integer age ! expected-note {{previous declaration is here}}
    real age ! expected-error {{duplicate member 'age'}}
  endtype

  type(Point) p

  type(zzzzz) zvar ! expected-error {{use of undeclared identifier 'zzzzz'}}
  type(Bar) barvar ! expected-error {{invalid type name 'bar'}}

END PROGRAM typetest
