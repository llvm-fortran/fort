! RUN: %flang -fsyntax-only < %s
PROGRAM gototest

10  CONTINUE
    ASSIGN 10 TO I

END PROGRAM
