! RUN: %flang -fsyntax-only -verify < %s

program test
  integer x,y,z,w
  integer i_arr
  common x,y, /a/ z,w
  common i,r,c
  common i_arr(22)

  complex c

end program

program sub1
  dimension i(10)
  ! FIXME: proper diagnostic
  common i(10) ! expected-error {{the specification statement 'dimension' cannot be applied to the array variable 'i'}}
end
