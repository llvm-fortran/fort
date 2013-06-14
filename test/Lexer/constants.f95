! RUN: %flang -verify < %s
PROGRAM constants
  CHARACTER * 11 C ! expected-error@+1 {{unterminated character literal}}
  C = 'hello world
END PROGRAM constants
