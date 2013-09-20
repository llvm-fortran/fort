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

subroutine sub2
  integer i
  common /a/i
  save /a/
  save /b/ ! expected-error {{use of undeclared common block 'b'}}
end

subroutine sub3
  integer i
  common /a/i
  save i ! expected-error {{}}
end
