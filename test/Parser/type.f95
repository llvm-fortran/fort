! RUN: %flang -verify -fsyntax-only < %s
PROGRAM typetest
  TYPE Point
    REAL X, Y
  END TYPE Point

  type :: pair
    integer(2) i,j
  end type pair

  type person
    ! FIXME: character*10 name(2)
    ! FIXME: character*20, dimension(5:10) :: save, skills
    integer age
    real, dimension(2,2) :: personality
    integer, save :: id ! expected-error {{use of 'save' attribute specifier in a type construct}}
    real, k :: m ! expected-error {{expected attribute specifier}}
    integer, dimension(3,3) rank ! expected-error {{expected '::'}}
  end type

  type foo 22 ! expected-error {{expected line break or ';' at end of statement}}
    integer :: k
  end type

  type params ! expected-note {{to match this 'type'}}
    integer i
  i = 22 ! expected-error {{expected 'end type'}}

END PROGRAM typetest
