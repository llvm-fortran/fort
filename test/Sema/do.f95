! RUN: %flang -verify < %s
PROGRAM dotest
    INTEGER I
    REAL R
    COMPLEX C
    INTEGER ADDR

    R = 1.0
    C = R
    DO 10 I = 1, 10
      R = I * R
10  CONTINUE

    DO 10 I = 1, 5 ! expected-error {{the statement label '10' must be declared after the 'DO' statement}}
20    R = R * R

    DO 666 I = 1, 10,2  ! expected-error {{use of undeclared statement label '666'}}
      R = I * R

    DO 30 C = 1, 3 ! expected-error {{expected a real or an integer variable instead of variable with type 'COMPLEX'}}
30  CONTINUE

    DO 40 I = 'A', 'Z' ! expected-error {{expected a real or an integer expression}}
40  CONTINUE ! expected-error@-1 {{expected a real or an integer expression}}

    DO 50 I = 1, 20
50  GOTO 40 ! expected-error {{invalid terminating statement for a DO loop}}

    ASSIGN 50 TO ADDR
    DO 60 I = 1, 40
60  GOTO ADDR ! expected-error {{invalid terminating statement for a DO loop}}

    DO 70 I = 0, 2
70  DO 80 ADDR = 0, 10 ! expected-error {{invalid terminating statement for a DO loop}}
80  CONTINUE

    DO 90 I = 0, 3
90    IF(I == 0) R = 1.0

    DO 100 I = 1, 2
100   IF(I == 1) THEN ! expected-error {{invalid terminating statement for a DO loop}}
      END IF

    DO 110 I = 0, 1
      IF(I == 0) THEN
        R = 1.0
110   END IF ! expected-error {{invalid terminating statement for a DO loop}}

END PROGRAM