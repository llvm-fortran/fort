* RUN: %flang < %s
* an extract from chemm.f
      SUBROUTINE FOO(M, N, ALPHA, BETA)
      REAL M, N, ALPHA, BETA
      PARAMETER (ZERO = 0.0)
      PARAMETER (ONE = 1.0)
*
*     Quick return if possible.
*
      IF ((M.EQ.0) .OR. (N.EQ.0) .OR.
     +    ((ALPHA.EQ.ZERO).AND. (BETA.EQ.ONE))) RETURN
*
*     And when  alpha.eq.zero.
*
      RETURN
      END
