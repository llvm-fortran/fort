! RUN: %flang -verify -fsyntax-only < %s
PROGRAM typetest
  TYPE Point
    REAL X, Y
  END TYPE Point

  TYPE foo 22 ! expected-error {{expected line break or ';' at end of statement}}
    INTEGER K
  END TYPE
END PROGRAM typetest
