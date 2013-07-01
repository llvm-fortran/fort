! RUN: %flang -verify < %s

SUBROUTINE FOO
END

SUBROUTINE BAR(X,Y)
  INTEGER X,Y
END SUBROUTINE

FUNCTION F()
END

FUNCTION F2(X,Y)
  REAL X,Y
END FUNCTION

SUBROUTINE SUB( ! expected-error {{expected ')'}}

END

FUNCTION FUNC ! expected-error {{expected '('}}
ENDFUNCTION
