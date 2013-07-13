! RUN: %flang %s 2>&1 | %file_check %s

SUBROUTINE FOO(STR) ! CHECK: define void @FOO({ i8*, i64 }
  CHARACTER*(*) STR
  STR = 'AGAIN'
END

PROGRAM test
  CHARACTER STR ! CHECK: alloca [1 x i8]
  LOGICAL L

  STR = 'HELLO' ! CHECK: call void @libflang_assignment_char1
  STR = STR
  STR = STR(1:1)

  STR = STR // ' WORLD' ! CHECK: call void @libflang_concat_char1

  L = STR .EQ. STR      ! CHECK: call i32 @libflang_compare_char1
  CONTINUE              ! CHECK: icmp eq i32

  L = STR .NE. STR      ! CHECK: call i32 @libflang_compare_char1
  CONTINUE              ! CHECK: icmp ne i32

  CALL FOO(STR)

END PROGRAM
