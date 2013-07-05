! RUN: %flang %s 2>&1 | %file_check %s
PROGRAM test
  LOGICAL L            ! CHECK: alloca i1

  L = L                ! CHECK: load i1*
  L = .NOT. L          ! CHECK: xor i1

  L = .TRUE. .EQV. L   ! CHECK: icmp eq i1 true
  L = .FALSE. .NEQV. L ! CHECK: icmp ne i1 false

END PROGRAM
