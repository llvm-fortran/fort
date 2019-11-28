! RUN: %fort -emit-llvm -o - %s | FileCheck %s
PROGRAM test
  CHARACTER STR
  LOGICAL L
  INTEGER I

  INTRINSIC len, len_trim, index, lle, lgt

  I = len('Hello') ! CHECK: store i32 5

  I = len_trim('Hello   ') ! CHECK: @libfort_lentrim_char1

  STR = 'Hello'

  I = index(STR, STR(:)) ! CHECK: call i{{.*}} @libfort_index_char1

  L = lle(STR, 'Hello')
  L = lgt(STR, 'World') ! CHECK: call i32 @libfort_lexcompare_char1

END PROGRAM
