! RUN: %flang -fsyntax-only -I%test_dir/Lexer/ < %s
PROGRAM inc
INCLUDE 'includedFile.f95'
END PROGRAM inc
