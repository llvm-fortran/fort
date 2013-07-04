! RUN: %flang -fsyntax-only -verify < %s

PROGRAM dowhiletest
  DO WHILE(.true.)
  END DO

  DO WHILE(1) ! expected-error {{expected a logical expression instead of an expression with type 'INTEGER'}}
  END DO

  IF(.true.) THEN
    DO WHILE(.false.)

    END IF ! expected-error {{expected 'END DO'}}

END ! expected-error {{expected 'END DO'}}
