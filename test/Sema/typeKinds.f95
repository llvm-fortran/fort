! RUN: %flang -fsyntax-only -verify < %s
PROGRAM vartest

  REAL(Kind = 4) R
  REAL(Kind = 8) D
  REAL(Kind = 0) A ! expected-error {{invalid kind selector '0' for type 'real'}}

END PROGRAM
