! RUN: %flang -emit-llvm -o - %s | %file_check %s
PROGRAM datatest
  INTEGER I, J
  REAL X
  INTEGER I_ARR(10)
  LOGICAL L_ARR(4)

  DATA (I_ARR(I), I = 1,10) / 2*0, 5*2, 3*-1 /

  DATA L_ARR / .false., .true., .false., .true. /

  DATA I / 1 / J, X / 2*0 / ! CHECK:      store i32 1
  CONTINUE                  ! CHECK-NEXT: store i32 0
  CONTINUE                  ! CHECK-NEXT: store float 0


END PROGRAM
