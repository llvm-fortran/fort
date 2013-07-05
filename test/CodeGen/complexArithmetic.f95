! RUN: %flang %s 2>&1 | %file_check %s
PROGRAM test
  COMPLEX C   ! CHECK: alloca { float, float }

  C = C       ! CHECK: getelementptr inbounds { float, float }*
  CONTINUE    ! CHECK: load float*

  C = +C
  C = -C      ! CHECK: fsub float
  CONTINUE    ! CHECK: fsub float

  C = C + C   ! CHECK: fadd float
  CONTINUE    ! CHECK: fadd float

  C = C - C   ! CHECK: fsub float
  CONTINUE    ! CHECK: fsub float

  C = C * C   ! CHECK: fmul float
  CONTINUE    ! CHECK: fmul float
  CONTINUE    ! CHECK: fsub float
  CONTINUE    ! CHECK: fmul float
  CONTINUE    ! CHECK: fmul float
  CONTINUE    ! CHECK: fadd float

  C = C / C   ! CHECK: fmul float
  CONTINUE    ! CHECK: fmul float
  CONTINUE    ! CHECK: fadd float
  CONTINUE    ! CHECK: fmul float
  CONTINUE    ! CHECK: fmul float
  CONTINUE    ! CHECK: fadd float
  CONTINUE    ! CHECK: fmul float
  CONTINUE    ! CHECK: fmul float
  CONTINUE    ! CHECK: fsub float
  CONTINUE    ! CHECK: fdiv float
  CONTINUE    ! CHECK: fdiv float

  C = (1, 2) + C ! CHECK: fadd float 1
  CONTINUE       ! CHECK: fadd float 2

  C = (-7.0, 7.0) - C ! CHECK: fsub float -7
  CONTINUE            ! CHECK: fsub float 7
END
