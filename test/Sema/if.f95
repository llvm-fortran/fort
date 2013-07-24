! RUN: %flang -fsyntax-only -verify < %s
PROGRAM iftest
  INTEGER I

  IF(.TRUE.) I = 0

  IF(2) I = 0 ! expected-error {{statement requires an expression of logical type ('integer' invalid)}}

  IF(.false.) THEN
    I = 2
  END IF

  ELSE ! expected-error {{'ELSE' statement must be a part of an if construct}}
  ELSE IF(2 == 2) THEN ! expected-error {{'ELSE IF' statement must be a part of an if construct}}
  END IF ! expected-error {{'END IF' statement must be a part of an if construct}}

  IF(2) THEN ! expected-error {{statement requires an expression of logical type ('integer' invalid)}}
    I = 3
  END IF

  IF(.true.) THEN
    I = 1
  ELSE IF(3 == 4) THEN
    I = 0
  ELSE IF(2.0) THEN ! expected-error {{statement requires an expression of logical type ('real' invalid)}}
    I = 2
  ELSE
    I = 3
  END IF

  IF(.true.) THEN
    DO I = 1, 10
  END IF ! expected-error {{expected 'END DO'}}

END PROGRAM ! expected-error {{expected 'END DO'}}
