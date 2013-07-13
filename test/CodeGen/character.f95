! RUN: %flang %s 2>&1 | %file_check %s
PROGRAM test
  CHARACTER STR ! CHECK: alloca [1 x i8]

  STR = 'HELLO' ! CHECK: call void @libflang_assignment_char1

END PROGRAM
