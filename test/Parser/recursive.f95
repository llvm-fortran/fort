! RUN: %flang -fsyntax-only -verify < %s

RECURSIVE SUBROUTINE SUB()
END

RECURSIVE FUNCTION FOO()
END

RECURSIVE PROGRAM main ! expected-error {{expected 'function' or 'subroutine' after 'recursive'}}
END
