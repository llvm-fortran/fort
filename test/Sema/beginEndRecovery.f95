! RUN: %flang -fsyntax-only -verify < %s

program recovery

  do i = 1, 10
    if (i == 0) then ! expected-note {{to match this 'if'}}
  end do ! expected-error {{expected 'end if'}}

  do while(.true.) ! expected-error@+3 {{expected a do termination statement with a statement label '100'}}
    do 100 i = 1, 10   ! expected-note {{to match this 'do'}}
      if (i == 0) then ! expected-note {{to match this 'if'}}
  end do ! expected-error {{expected 'end if'}}

  i = 0

  if(i == 1) then
    do i = 1, 10 ! expected-note {{to match this 'do'}}
  end if ! expected-error {{expected 'end do'}}

  if(i == 1) then
    do i = 1, 10 ! expected-note {{to match this 'do'}}
  else ! expected-error {{expected 'end do'}}
  end if

  if(i == 1) then
    do 100 i = 1,10 ! expected-note {{to match this 'do'}}
  else if(i == 3) then ! expected-error {{expected a do termination statement with a statement label '100'}}
  end if

  do 200 i = 1,10
    do j = 1,10  ! expected-note {{to match this 'do'}}
300   print *,i
200 continue ! expected-error {{expected 'end do'}}

100 continue
end
