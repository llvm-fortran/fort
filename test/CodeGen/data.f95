! RUN: %flang -emit-llvm -o - %s | %file_check %s
PROGRAM datatest
  INTEGER I, J
  REAL X

  DATA I / 1 / J, X / 2*0 / ! CHECK:      store i32 1
  CONTINUE                  ! CHECK-NEXT: store i32 0
  CONTINUE                  ! CHECK-NEXT: store float 0
END PROGRAM
