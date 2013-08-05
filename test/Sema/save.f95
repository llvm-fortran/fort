! RUN: %flang -verify -fsyntax-only < %s

SUBROUTINE FOO
  INTEGER I
  SAVE
END

SUBROUTINE BAR
  INTEGER I, J
  SAVE I, J
END

SUBROUTINE BAM(KA) ! expected-note {{'ka' is an argument defined here}}
  INTEGER KA, L
  PARAMETER (MK=1) ! expected-note {{'mk' is a parameter constant defined here}}
  SAVE L, KA ! expected-error {{specification statement requires a local variable}}
  SAVE MK ! expected-error {{specification statement requires a local variable}}
END

SUBROUTINE BAZ()
  INTEGER I, L
  SAVE I, L
  SAVE I ! expected-error {{the specification statement 'save' cannot be applied to the variable 'i' more than once}}
END

SUBROUTINE FEZ()
  INTEGER I, var
  SAVE var
  SAVE ! expected-error {{the specification statement 'save' cannot be applied to the variable 'var' more than once}}
END
