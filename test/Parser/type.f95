! RUN: %flang -fsyntax-only < %s
PROGRAM typetest
  TYPE Point
    REAL X, Y
  END TYPE Point
END PROGRAM typetest
