! RUN: %flang -interpret %s | %file_check %s

program datatest
  integer i, j
  integer i_arr(10)
  integer i_mat(3,3)

  data i / 1 / j / 0 /
  data (i_arr(i), i = 1,10) / 2*0, 5*2, 3*-1 /
  data ( (i_mat(i,j), i = 1,3), j = 1, 3) &
       / 3*42, 3*11, 3*-88 /


  print *, 'START' ! CHECK: START
  print *, i       ! CHECK-NEXT: 1
  print *, j       ! CHECK-NEXT: 0

  print *, i_arr(1), ', ', i_arr(3), ', ', i_arr(8), ', ', &
           i_arr(10) ! CHECK-NEXT: 0, 2, -1, -1


  print *, i_mat(1,1), ', ', i_mat(2,1), ', ', i_mat(3,1), ', ', &
           i_mat(1,2), ', ', i_mat(2,2), ', ', i_mat(3,2), ', ', &
           i_mat(1,3), ', ', i_mat(2,3), ', ', i_mat(3,3)
  continue ! CHECK-NEXT: 42, 42, 42, 11, 11, 11, -88, -88, -88

end program
