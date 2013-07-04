! RUN: %flang %s 2>&1 | %file_check %s
PROGRAM gototest

1000 CONTINUE   ! CHECK: ; <label>:0
     GOTO 1000  ! CHECK: br label %0

     GOTO 2000  ! CHECK: br label %1
2000 CONTINUE   ! CHECK: ; <label>:1

END
