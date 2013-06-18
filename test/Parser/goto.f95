! RUN: %flang -verify < %s
PROGRAM gototest
    INTEGER I

10  I = 0
    GO TO 10
    GO TO 10

    GO TO HELL ! expected-error {{expected statement label}}
    GO TO 2.0  ! expected-error {{expected statement label}}
END PROGRAM
