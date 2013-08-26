! RUN: %flang -emit-llvm -o - %s

PROGRAM test

  integer i_mat(4,4), i_mat2(4,4)
  integer i

  i = 11
  i_mat = i
  i_mat2 = i_mat

END
