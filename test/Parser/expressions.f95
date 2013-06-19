! RUN: %flang -verify < %s
PROGRAM expressions
  REAL X

  X = 2.0
  X = X
  X = (X)
  X = (3 ! expected-error@+1 {{expected ')'}}
  X = ! expected-error@+1 {{expected an expression after '='}}
ENDPROGRAM expressions
