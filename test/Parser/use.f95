! RUN: %fort -fsyntax-only -verify %s
program p
  use mdl ! expected-error {{unknown module 'mdl'}}
  implicit none
end program
