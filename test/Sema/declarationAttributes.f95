! RUN: %fort -fsyntax-only -verify < %s
! RUN: %fort -fsyntax-only -verify -ast-print %s 2>&1 | FileCheck %s

program declAttrTest

  implicit none

  real, external :: sub ! CHECK: real subroutine sub()
  logical, intrinsic :: lle

  integer, dimension(10,10) :: i_mat
  real, dimension(10) :: m(20), k

  integer, external, external :: foo ! expected-error {{duplicate 'external' attribute specifier}}
  integer, dimension(20), &
           dimension(40) :: vector ! expected-error {{duplicate 'dimension' attribute specifier}}

  integer, save x ! expected-error {{expected '::'}}

  real :: r = 1.5
  save r ! expected-error {{the specification statement 'save' cannot be applied to the variable 'r' more than once}}

  if(lle('a','b')) then
  end if

  ! FIXME: support other attributes.

end
