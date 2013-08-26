! RUN: %flang -interpret %s | %file_check %s

integer function foo(i)
  integer i
  foo = i
  i = i + 1
end

program test

  integer i_mat(3,3)
  integer i

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

end
