! RUN: %flang -verify < %s
PROGRAM test

    WRITE(*,*) 'Hello world'
    PRINT *, 'Hello world'

END PROGRAM
