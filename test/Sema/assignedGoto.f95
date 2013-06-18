! RUN: %flang -verify < %s
PROGRAM gototest
    INTEGER I
    INTEGER DEST

10  I = 0
    ASSIGN 10 TO DEST

    ASSIGN 20 TO DEST
20  I = 20

    ASSIGN 666 TO DEST ! expected-error {{use of undeclared statement label '666'}}

    GO TO DEST (10, 20, 30)
30  GOTO DEST

    GO TO DEST (999) ! expected-error {{use of undeclared statement label '999'}}

END PROGRAM
