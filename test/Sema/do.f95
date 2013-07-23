! RUN: %flang -fsyntax-only -verify < %s
! RUN: %flang -fsyntax-only -verify -ast-print %s 2>&1 | %file_check %s
PROGRAM dotest
    INTEGER I, II, K
    REAL R
    COMPLEX C
    INTEGER ADDR

    R = 1.0
    C = R
    DO 10 I = 1, 10
      R = I * R
10  CONTINUE ! expected-note {{previous definition is here}}

    END DO ! expected-error {{use of 'END DO' without the do statement}}

    DO 10 I = 1, 5 ! expected-error {{the statement label '10' must be declared after the 'DO' statement}}
20    R = R * R

    DO 25 II = 1, 10
      IF(.true.) THEN
25      CONTINUE ! expected-error {{expected 'END IF'}}
      END IF

    DO 666 I = 1, 10,2  ! expected-error {{use of undeclared statement label '666'}}
      R = I * R

    END DO ! expected-error {{expected a statement with a statement label '666' to mark the end of a do loop}}

    DO 30 C = 1, 3 ! expected-error {{expected a real or an integer variable instead of variable with type 'complex'}}
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
90    IF(I == 0) R = 1.0 ! expected-error {{invalid terminating statement for a DO loop}}

    DO 100 I = 1, 2
100   IF(I == 1) THEN ! expected-error {{invalid terminating statement for a DO loop}}
      END IF

    DO 110 I = 0, 1
      IF(I == 0) THEN
        R = 1.0
110   END IF ! expected-error {{invalid terminating statement for a DO loop}}

    DO 120 I = 1.0, 8.0D1 ! CHECK: INT(1)
120 CONTINUE ! CHECK: INT(80)

    DO I = 1,10
    END DO

    ! allow multiple DOs to finish at the same statement
    ! FIXME: make obsolete in Fortran 90+
    DO 130 I = 1, 10
    DO 130 K = 10, 1, -1
      R = R + I + K
130 CONTINUE

END PROGRAM
