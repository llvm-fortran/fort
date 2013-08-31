! RUN: %flang -interpret %s | %file_check %s
program arrayops
  integer i_arr(4)
  integer i_mat(3,3), i_mat2(3,3)
  real    r_mat(3,3)
  logical l_mat(3,3)
  character*(5) char_arr(2)

  ! assignment using data statement
  data i_arr / 1, 2*0, -69 /
  data i_mat / 1, 0, 0, 0, 1, 0, 0, 0, 1 /
  data char_arr / 'Hello', 'World' /

  print *, i_arr(1), ', ', i_arr(2), ', ', i_arr(3), ', ', i_arr(4)
  continue ! CHECK: 1, 0, 0, -69

  print *, i_mat(1,1), ', ', i_mat(2,1), ', ', i_mat(3,1), ', ', &
           i_mat(1,2), ', ', i_mat(2,2), ', ', i_mat(3,2), ', ', &
           i_mat(1,3), ', ', i_mat(2,3), ', ', i_mat(3,3)
  continue ! CHECK-NEXT: 1, 0, 0, 0, 1, 0, 0, 0, 1

  print *, char_arr(1), char_arr(2) ! CHECK-NEXT: HelloWorld

  i_mat2 = 1

  i_mat = i_mat + i_mat2 * 2
  print *, i_mat(1,1), ', ', i_mat(2,1), ', ', i_mat(3,1), ', ', &
           i_mat(1,2), ', ', i_mat(2,2), ', ', i_mat(3,2), ', ', &
           i_mat(1,3), ', ', i_mat(2,3), ', ', i_mat(3,3)
  continue ! CHECK-NEXT: 3, 2, 2, 2, 3, 2, 2, 2, 3

  l_mat = i_mat <= (i_mat2 + 1)
  print *, l_mat(1,1), ', ', l_mat(2,1), ', ', l_mat(3,1), ', ', &
           l_mat(1,2), ', ', l_mat(2,2), ', ', l_mat(3,2), ', ', &
           l_mat(1,3), ', ', l_mat(2,3), ', ', l_mat(3,3)
  continue ! CHECK-NEXT: false, true, true, true, false, true, true, true, false

  r_mat = 1.0
  r_mat(3,3) = 0.0
  i_mat = i_mat * 2 + r_mat
  print *, i_mat(1,1), ', ', i_mat(2,1), ', ', i_mat(3,1), ', ', &
           i_mat(1,2), ', ', i_mat(2,2), ', ', i_mat(3,2), ', ', &
           i_mat(1,3), ', ', i_mat(2,3), ', ', i_mat(3,3)
  continue ! CHECK-NEXT: 7, 5, 5, 5, 7, 5, 5, 5, 6

end
