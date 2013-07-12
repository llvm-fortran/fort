! RUN: %flang %s 2>&1 | %file_check %s
PROGRAM test ! CHECK: define i32 @main
  CONTINUE   ! CHECK: br label
END PROGRAM  ! CHECK: ret i32 0
