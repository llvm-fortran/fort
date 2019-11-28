! RUN: not %fort -flong-nonexistent-command-line-arg %s 2>&1 | FileCheck %s -check-prefix=NON-EXISTENT

! NON-EXISTENT: unknown argument: '-flong-nonexistent-command-line-arg'

