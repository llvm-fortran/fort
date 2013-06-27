! RUN: %flang -verify < %s
PROGRAM formattest

1000 FORMAT (A)
     FORMAT (A) ! expected-error {{'FORMAT' statement is missing a statement label}}

END PROGRAM
