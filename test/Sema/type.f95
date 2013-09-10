! RUN: %flang -verify -fsyntax-only < %s
PROGRAM typetest

  INTEGER Bar ! expected-note {{previous definition is here}}
  TYPE Point
    REAL X, Y ! expected-note {{passing argument to field 'x' declared here}}
  END TYPE Point ! expected-note@-1 {{passing argument to field 'y' declared here}}

  type Triangle
    type(Point) vertices(3)
  end type

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
  type(Triangle) tri
  integer i

  type(zzzzz) zvar ! expected-error {{use of undeclared identifier 'zzzzz'}}
  type(Bar) barvar ! expected-error {{invalid type name 'bar'}}

  p = Point(1.0, 2.0)

  p = i ! expected-error {{assigning to 'type point' from incompatible type 'integer'}}
  i = p ! expected-error {{assigning to 'integer' from incompatible type 'type point'}}
  tri = p ! expected-error {{assigning to 'type triangle' from incompatible type 'type point'}}

  p = Point(.true., p) ! expected-error {{passing 'logical' to parameter of incompatible type 'real'}}
  continue ! expected-error@-1 {{passing 'type point' to parameter of incompatible type 'real'}}
  p = Point(0.0) ! expected-error {{too few arguments to type constructor, expected 2, have 1}}
  p = Point(0.0, 1.0, 2.0) ! expected-error {{too many arguments to type constructor, expected 2, have 3}}
  p = Point() ! expected-error {{too few arguments to type constructor, expected 2, have 0}}

END PROGRAM typetest
