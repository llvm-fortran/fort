! RUN: %flang %s 2>&1 | %file_check %s
PROGRAM dowhiletest
  INTEGER I

  I = 0
  DO WHILE(I .LT. 10) ! CHECK: icmp slt
    I = I + 1         ! CHECK: br i1
  END DO              ! CHECK: br label %do-while

END PROGRAM
