C RUN: %flang -fsyntax-only %s
C RUN: %flang -fsyntax-only -ast-print %s 2>&1 | %file_check %s
       PROGRAM test
       INTEGERNE,DOI(10),DONE,DOWHILE
       DOI=1,10
       ENDDO
C CHECK: done = 1
       DONE=1
C CHECK: done = (1+2)
       DO NE =1+2
       DODONE=1,10
       ENDDO

C CHECK: dowhile = 33
       DOW H ILE=33
       DOWHILE(.true.)
       ENDDO

C CHECK: doi(1) = 1
       DOI(1) = 1
       DOI(2) = 2

       E ND PRO GRAMt e s t
