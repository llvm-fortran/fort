! RUN: %flang -fsyntax-only -verify < %s
PROGRAM what ! expected-error@+1 {{expected 'END PROGRAM' statement}}
