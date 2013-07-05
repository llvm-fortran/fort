! RUN: %flang %s 2>&1 | %file_check %s
PROGRAM test
  INTEGER I
  REAL X
  COMPLEX C
  LOGICAL L

  I = 0
  X = 1.0
  C = (1.0,0.0)

  X = X + I ! CHECK: sitofp i32
  CONTINUE  ! CHECK: fadd float
  CONTINUE  ! CHECK: store float

  I = X - I ! CHECK: sitofp i32
  CONTINUE  ! CHECK: fsub float
  CONTINUE  ! CHECK: fptosi float
  CONTINUE  ! CHECK: store i32

  L = X .EQ. I ! CHECK: sitofp i32
  CONTINUE     ! CHECK: fcmp oeq float

  X = C    ! CHECK: store float
  I = C    ! CHECK: fptosi float
  CONTINUE ! CHECK: store i32
  C = X    ! CHECK: store float
  CONTINUE ! CHECK: store float 0
  C = I    ! CHECK: sitofp i32
  CONTINUE ! CHECK: store float
  CONTINUE ! CHECK: store float 0

  C = C + X
  C = C - I

END
