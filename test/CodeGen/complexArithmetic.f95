! RUN: %flang %s 2>&1 | %file_check %s
PROGRAM test
  COMPLEX C   ! CHECK: alloca { float, float }
  DOUBLE COMPLEX DC ! CHECK: alloca { double, double }
  LOGICAL L

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

  L = (0.0, 1.0) .EQ. C ! CHECK: fcmp oeq float 0
  CONTINUE              ! CHECK: fcmp oeq float 1
  CONTINUE              ! CHECK: and i1

  L = (1.0, 0.0) .NE. C ! CHECK: fcmp une float 1
  CONTINUE              ! CHECK: fcmp une float 0
  CONTINUE              ! CHECK: or i1

  C = (1.0, 1.0)
  C = C ** 1
  C = C ** 2
  C = C ** 3 ! CHECK: call { float, float } @libflang_cpowif
  C = C ** C ! CHECK: call { float, float } @libflang_cpowf

  DC = (2d0, 1d0) + DC ! CHECK: fadd double 2
  CONTINUE             ! CHECK: fadd double 1
END
