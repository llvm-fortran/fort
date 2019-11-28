      PROGRAM text
C RUN: %fort -fsyntax-only %s
C RUN: %fort -fsyntax-only -ast-print %s 2>&1 | FileCheck --strict-whitespace %s
      CHARACTER *256 MESS(09)
      DATA MESS(01)/
     .' He llo
     .Wo 
     . - rld'/
C CHECK: ' He lloWo  - rld'
      DATA MESS(02)/
     .' Blah blah blah
     .Blah blah blah.'/
      DATA MESS(03)/
     .' Blah blah blah
     .Blah blah blah
     .Blah blah blah.'/
      DATA MESS(08)/
     .' ''UNKOWN'
     .' STATUS.'/
      END
