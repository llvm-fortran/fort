* RUN: %flang -fsyntax-only %s
* RUN: %flang -fsyntax-only -ast-print %s 2>&1 | %file_check %s
      LOGICALFUNCTIONFOO()
* CHECK: foo = .false.
        FOO = .false.
        RETURN
      END

* FIXME!!!
      LOGICAL(2)FUNCTIONBAR()
* CHECK: bar = logical(.true.,Kind=2)
        BAR = .true.
        RETURN
      END
