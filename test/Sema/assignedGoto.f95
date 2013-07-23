! RUN: %flang -fsyntax-only -verify < %s
PROGRAM gototest
    INTEGER I
    INTEGER DEST
    REAL R

10  I = 0
    ASSIGN 10 TO DEST

    ASSIGN 20 TO DEST
20  I = 20

    ASSIGN 10 TO R ! expected-error {{expected an integer variable instead of a variable with type 'real'}}

    ASSIGN 666 TO DEST ! expected-error {{use of undeclared statement label '666'}}

    GO TO DEST (10, 20, 30)
30  GOTO DEST

    GO TO DEST (999) ! expected-error {{use of undeclared statement label '999'}}

    GOTO R ! expected-error {{expected an integer variable instead of a variable with type 'real'}}

END PROGRAM
