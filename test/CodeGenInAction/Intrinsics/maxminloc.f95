! RUN: %flang -interpret %s | %file_check %s

program maxminloctest

  intrinsic maxloc, minloc
  integer i_arr(5)
  real r_arr(5)

  print *, 'START' ! CHECK: START
  i_arr = (/ 4, 7, 2, 1, 0 /)
  i = maxloc(i_arr, 1)
  print *, i ! CHECK-NEXT: 2

  i = minloc((/ 0, 5, 42, -54, 1 /), 1)
  print *, i ! CHECK-NEXT: 4

  r_arr = (/ 1.0, 2.0, 0.0, 9.0, 9.0 /)
  i = maxloc(r_arr,1)
  print *, i ! CHECK-NEXT: 4

end
