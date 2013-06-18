! RUN: %flang -verify < %s
PROGRAM gototest
    INTEGER I
    INTEGER DEST
    REAL R

10  I = 0
    R = 1.0
    ASSIGN 10 TO DEST

    ASSIGN TO DEST ! expected-error {{expected statement label after 'ASSIGN'}}
    ASSIGN 10 DEST ! expected-error {{expected 'TO'}}
    ASSIGN 10 TO 2 ! expected-error {{expected an integer variable after 'TO'}}
    ASSIGN 10 TO X ! expected-error {{expected an integer variable after 'TO'}}
    ASSIGN 10 TO R ! expected-error {{expected an integer variable after 'TO'}}

END PROGRAM
