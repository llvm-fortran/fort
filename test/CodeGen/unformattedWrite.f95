! RUN: %flang %s 2>&1 | %file_check %s
PROGRAM test

  ! FIXME: logical values
  WRITE (*,*) 'Hello world!', 2, 3.5, (1.0, 2.0)
  PRINT *, 'Hello world!'

END PROGRAM
