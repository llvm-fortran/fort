! RUN: %flang -fsyntax-only -verify < %s

program declAttrTest

  implicit none
  real, external :: sub
  integer, dimension(10,10) :: i_mat
  real, dimension(10) :: m(20), k

  integer, external, external :: foo ! expected-error {{duplicate 'external' attribute specifier}}
  integer, dimension(20), &
           dimension(40) :: vector ! expected-error {{duplicate 'dimension' attribute specifier}}



  ! FIXME: apply the attributes.

end
