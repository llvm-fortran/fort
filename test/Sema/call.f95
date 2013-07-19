! RUN: %flang -fsyntax-only -verify < %s

SUBROUTINE SUB(I,J)
END

PROGRAM test
  EXTERNAL FUNC

  CALL SUB(1,2)
  CALL SUB ! expected-error {{too few arguments to subroutine call, expected 2, have 0}}
  CALL SUB(1) ! expected-error {{too few arguments to subroutine call, expected 2, have 1}}
  CALL SUB(1,2,3,4) ! expected-error {{too many arguments to subroutine call, expected 2, have 4}}
  CALL FUNC
END
