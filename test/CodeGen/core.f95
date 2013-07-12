! RUN: %flang %s 2>&1 | %file_check %s
PROGRAM test
  STOP       ! CHECK: call void @libflang_stop()
END PROGRAM
