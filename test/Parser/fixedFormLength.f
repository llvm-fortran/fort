* RUN: %fort -fsyntax-only -ffixed-line-length-90 %s
* RUN: %fort -fsyntax-only -ffixed-line-length-0 %s
* RUN: %fort -fsyntax-only -ffixed-line-length-none %s
* RUN: not %fort -fsyntax-only -ffixed-line-length-50 %s 2>&1 | %file_check %s -check-prefix=LINE-LENGTH
* RUN: not %fort -fsyntax-only %s 2>&1 | %file_check %s -check-prefix=LINE-LENGTH

      PROGRAM test
      CHARACTER *72 STR
* LINE-LENGTH: expected '/'
      DATA STR/'abcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyz'/
      END
