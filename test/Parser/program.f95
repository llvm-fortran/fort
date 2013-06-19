! RUN: %flang -verify < %s
PROGRAM what ! expected-error@+1 {{expected 'END PROGRAM' statement}}
