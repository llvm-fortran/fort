! RUN: %flang -verify < %s
PROGRAM iftest
  INTEGER I

  IF(.TRUE.) I = 0

  IF(2) I = 0 ! expected-error {{expected a logical expression instead of an expression with type 'INTEGER'}}

  IF(.false.) THEN
    I = 2
  END IF

  ELSE ! expected-error {{'ELSE' statement must be a part of an if construct}}
  ELSE IF(2 == 2) THEN ! expected-error {{'ELSE IF' statement must be a part of an if construct}}
  END IF ! expected-error {{'END IF' statement must be a part of an if construct}}

  IF(2) THEN ! expected-error {{expected a logical expression instead of an expression with type 'INTEGER'}}
    I = 3
  END IF ! expected-error {{'END IF' statement must be a part of an if construct}}

  IF(.true.) THEN
    I = 1
  ELSE IF(2.0) THEN ! expected-error {{expected a logical expression instead of an expression with type 'REAL'}}
    I = 2
  ELSE
    I = 3
  END IF

END PROGRAM
