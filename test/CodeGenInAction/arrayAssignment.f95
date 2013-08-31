! RUN: %flang -interpret %s | %file_check %s

integer function foo(i)
  integer i
  foo = i
  i = i + 1
end

program test

  integer i_mat(3,3), i_mat2(3,3)
  real r_mat(3,3)
  integer i
  data i_mat2 / 1, 0, 0, 0, 1, 0, 0, 0, 1 /

  i = 42
  i_mat = i

  print *, i_mat(1,1), ', ', i_mat(2,1), ', ', i_mat(3,1), ', ', &
           i_mat(1,2), ', ', i_mat(2,2), ', ', i_mat(3,2), ', ', &
           i_mat(1,3), ', ', i_mat(2,3), ', ', i_mat(3,3)
  continue ! CHECK: 42, 42, 42, 42, 42, 42, 42, 42, 42

  i_mat = foo(i)
  print *, i_mat(1,1), ', ', i_mat(2,1), ', ', i_mat(3,1), ', ', &
           i_mat(1,2), ', ', i_mat(2,2), ', ', i_mat(3,2), ', ', &
           i_mat(1,3), ', ', i_mat(2,3), ', ', i_mat(3,3), ', ', i
  continue ! CHECK-NEXT: 42, 42, 42, 42, 42, 42, 42, 42, 42, 43

  i_mat = i_mat2
  print *, i_mat(1,1), ', ', i_mat(2,1), ', ', i_mat(3,1), ', ', &
           i_mat(1,2), ', ', i_mat(2,2), ', ', i_mat(3,2), ', ', &
           i_mat(1,3), ', ', i_mat(2,3), ', ', i_mat(3,3)
  continue ! CHECK-NEXT: 1, 0, 0, 0, 1, 0, 0, 0, 1

  i_mat = 1.0
  print *, i_mat(1,1), ', ', i_mat(2,1), ', ', i_mat(3,1), ', ', &
           i_mat(1,2), ', ', i_mat(2,2), ', ', i_mat(3,2), ', ', &
           i_mat(1,3), ', ', i_mat(2,3), ', ', i_mat(3,3)
  continue ! CHECK-NEXT: 1, 1, 1, 1, 1, 1, 1, 1, 1

  r_mat = 2.0
  r_mat(1,1) = 3.0
  r_mat(2,2) = 4.0
  i_mat = r_mat
  print *, i_mat(1,1), ', ', i_mat(2,1), ', ', i_mat(3,1), ', ', &
           i_mat(1,2), ', ', i_mat(2,2), ', ', i_mat(3,2), ', ', &
           i_mat(1,3), ', ', i_mat(2,3), ', ', i_mat(3,3)
  continue ! CHECK-NEXT: 3, 2, 2, 2, 4, 2, 2, 2, 2

end
