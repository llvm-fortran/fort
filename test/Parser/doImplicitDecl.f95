! RUN: %flang -fsyntax-only < %s
PROGRAM dotest
    REAL R
    DO 10 I = 1,10
      R = I * R
10  CONTINUE
END
