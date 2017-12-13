! RUN: %fort -emit-llvm -o - %s | %file_check %s
PROGRAM test
  STOP       ! CHECK: call void @libfort_stop()
END PROGRAM
