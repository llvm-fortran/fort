! RUN: %fort -emit-llvm -o - %s | FileCheck %s
PROGRAM gototest
    INTEGER I

10  I = 0
20  GOTO (10,30) I ! CHECK: switch i32

30  I = 1

END PROGRAM
