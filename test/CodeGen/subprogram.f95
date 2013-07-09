! RUN: %flang %s 2>&1 | %file_check %s

SUBROUTINE SUB ! CHECK: define void @SUB()
END            ! CHECK: ret void

SUBROUTINE SUB2(I, R, C, L) ! CHECK: define void @SUB2(i32* %I, float* %R, { float, float }* %C, i1* %L)
  INTEGER I
  REAL R
  COMPLEX C
  LOGICAL L
  INTEGER J

  J = I ! CHECK: load i32* %I
  C = R ! CHECK: load float* %R

  IF(L) THEN ! CHECK: load i1* %L
    J = 0
  END IF

END ! CHECK: ret void

REAL FUNCTION SQUARE(X) ! CHECK: define float @SQUARE(float* %X)
  REAL X                ! CHECK: alloca float
  SQUARE = X * X
  RETURN                ! CHECK: ret float
END
