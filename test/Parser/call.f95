! RUN: %flang -verify < %s

PROGRAM test
  EXTERNAL SUB
  EXTERNAL FUNC

  CALL SUB
  CALL SUB()
  CALL FUNC(1,2)

  CALL void ! expected-error {{expected a function after 'CALL'}}
  CALL 22 ! expected-error {{expected identifier}}

  CALL FUNC(1,2 ! expected-error {{expected ')'}}
  CALL FUNC 1,2) ! expected-error {{expected '('}}
END
