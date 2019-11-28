* RUN: %fort -fsyntax-only %s
* RUN: %fort -fsyntax-only -ast-print %s 2>&1 | FileCheck %s

      PROGRAM D
* CHECK: endd = 0
      INTEGER ENDD
      ENDD = 0
      END D
