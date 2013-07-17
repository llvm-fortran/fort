! RUN: %flang -emit-llvm -o - %s | %file_check %s

SUBROUTINE SUB ! CHECK: define void @SUB()
END            ! CHECK: ret void

SUBROUTINE SUB2(I, R, C, L) ! CHECK: define void @SUB2(i32* %I, float* %R, { float, float }* %C, i32* %L)
  INTEGER I
  REAL R
  COMPLEX C
  LOGICAL L
  INTEGER J

  J = I ! CHECK: load i32* %I
  C = R ! CHECK: load float* %R

  IF(L) THEN ! CHECK: load i32* %L
    J = 0
  END IF

END ! CHECK: ret void

REAL FUNCTION SQUARE(X) ! CHECK: define float @SQUARE(float* %X)
  REAL X                ! CHECK: alloca float
  SQUARE = X * X
  RETURN                ! CHECK: ret float
END

COMPLEX FUNCTION DOUBLE(C)
  COMPLEX C
  DOUBLE = C+C

  CONTINUE ! CHECK: load float*
  CONTINUE ! CHECK: load float*
  CONTINUE ! CHECK: insertvalue { float, float } undef
  CONTINUE ! CHECK: insertvalue { float, float }
  CONTINUE ! CHECK: ret { float, float }
END

PROGRAM test
  REAL R
  COMPLEX C
  PARAMETER (PI = 3.0)
  INTRINSIC REAL, CMPLX

  REAL ExtFunc
  EXTERNAL ExtSub, ExtSub2, ExtFunc

  R = SQUARE(2.0) ! CHECK: store float 2.0
  CONTINUE        ! CHECK: call float @SQUARE(float*

  R = SQUARE(R)   ! CHECK: call float @SQUARE(float*
  R = SQUARE(SQUARE(R))

  R = SQUARE(PI)  ! CHECK: call float @SQUARE(float*

  C = DOUBLE((1.0, 2.0)) ! CHECK: store float 1
  CONTINUE               ! CHECK: store float 2
  CONTINUE               ! CHECK: call { float, float } @DOUBLE
  CONTINUE               ! CHECK: extractvalue { float, float }
  CONTINUE               ! CHECK: extractvalue { float, float }

  C = DOUBLE(DOUBLE(C))

  C = DOUBLE(CMPLX(SQUARE(R)))

  CALL SUB        ! CHECK: call void @SUB

  CALL SUB2(1, 2.0, (1.0, 2.0), .false.)
  CONTINUE ! CHECK: store i32 1
  CONTINUE ! CHECK: store float 2.0
  CONTINUE ! CHECK: store float 1.0
  CONTINUE ! CHECK: store float 2.0
  CONTINUE ! CHECK: store i32 0
  CONTINUE ! call void @SUB2

  R = ExtFunc(ExtFunc(1.0)) ! CHECK: call float @EXTFUNC(float*

  CALL ExtSub      ! CHECK: call void @EXTSUB()
  CALL ExtSub()    ! CHECK: call void @EXTSUB()
  CALL ExtSub2(R, C) ! CHECK: call void @EXTSUB2(float*


END
