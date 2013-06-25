! RUN: %flang -verify < %s
! RUN: %flang -verify %s 2>&1 | %file_check %s
PROGRAM expressions
  REAL X,Y,Z,W
  LOGICAL L

  X = 2.0
  Y = 1.0
  Z = 2.0
  W = 3.0

  X = X + Y-Z + W ! CHECK: (((X+Y)-Z)+W)
  X = X+Y * Z ! CHECK: (X+(Y*Z))
  X = X * Y + Z ! CHECK: ((X*Y)+Z)
  X = (X + Y) * Z ! CHECK: ((X+Y)*Z)
  X = X * Y ** Z ! CHECK: (X*(Y**Z))
  X = X + Y ** Z / W ! CHECK: (X+((Y**Z)/W))
  X = X+Y ** (Z / W) ! CHECK: (X+(Y**(Z/W)))

  X = (X + Y) * Z - W ! CHECK: (((X+Y)*Z)-W)
  X = X + Y * -Z ! CHECK: (X+(Y*(-Z)))

  L = X + Y .EQ. Z ! CHECK: ((X+Y)==Z)
  L = X / Y .LT. Z ! CHECK: ((X/Y)<Z)
  L = X - Y .GT. Z ** W ! CHECK: ((X-Y)>(Z**W))

  X = X
  X = (X)
  X = (3 ! expected-error@+1 {{expected ')'}}
  X = ! expected-error@+1 {{expected an expression after '='}}
  X = A ! expected-error {{use of undeclared identifier 'A'}}
ENDPROGRAM expressions
