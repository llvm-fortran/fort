! RUN: %flang -verify < %s
PROGRAM formattest
1000 FORMAT (A)
1001 FORMAT (I1)
1002 FORMAT (JK) ! expected-error {{'JK' isn't a valid format descriptor}}


END PROGRAM
