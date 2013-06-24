! RUN: %flang -verify < %s
PROGRAM intrintest
  INTRINSIC INT, MOD ! expected-note {{previous definition is here}}

  INTRINSIC FAIL ! expected-error {{invalid function name 'FAIL' in an intrinsic statement}}
  INTRINSIC ABS, SAB ! expected-error {{invalid function name 'SAB' in an intrinsic statement}}

  INTRINSIC MOD ! expected-error {{redefinition of 'MOD'}}

  INTEGER I

  !FIXME: I = INT(2.0)
  !FIXME: I = ABS(-3)

END PROGRAM
