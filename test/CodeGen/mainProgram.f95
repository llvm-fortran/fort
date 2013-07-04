! RUN: %flang %s 2>&1 | %file_check %s
PROGRAM test ! CHECK: define i32 @main
  STOP       ! CHECK: br label %program_exit
END PROGRAM  ! CHECK: ret i32 0
