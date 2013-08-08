C RUN: %flang -fsyntax-only %s
C RUN: %flang -fsyntax-only -ast-print %s 2>&1 | %file_check %s
       PROGRAM test
       INTEGERDONE
       DOI=1,10
       ENDDO
C CHECK: done = 1
       DONE=1
C CHECK: done = (1+2)
       DO NE =1+2
       DODONE=1,10
       ENDDO
       E ND PRO GRAMt e s t
