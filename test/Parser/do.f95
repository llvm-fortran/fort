! RUN: %flang -verify < %s
PROGRAM dotest
    INTEGER I
    REAL R

    R = 1.0
    DO 10 I = 1,10
      R = I * R
10  CONTINUE

    R = 0.0
    DO 20 I = 1,10,2
20    R = R + I

    DO x I = 1,10 ! expected-error {{expected statement label after 'DO'}}
      R = R + I
    CONTINUE

    DO 30 'V' = 1,100,5 ! expected-error {{expected a real or an integer variable}}
      R = R - I
30  CONTINUE

    DO 40 I 1,100 ! expected-error {{expected '='}}
40    R = R * 1

    DO 50 I = 1 100 ! expected-error {{expected ','}}
      R = R + I
50  CONTINUE

    DO 60 I = ! expected-error@+1 {{expected an expression after '='}}
60    CONTINUE

    DO 70 I = 1, ! expected-error@+1 {{expected an expression after ','}}
70    CONTINUE

    DO 80 I = 1,3, ! expected-error@+1 {{expected an expression after ','}}
80    CONTINUE

    ! FIXME:
    !DO 90 I =
    ! 1 , 4
90  CONTINUE

    DO 100 I = 1, ! expected-error@+1 {{expected an expression after ','}}
      I ! expected-error@+1 {{expected '='}}
100 CONTINUE

END PROGRAM
