! RUN: %flang < %s
PROGRAM inc
INCLUDE 'thisFileDoesntExist.f95'
END PROGRAM inc
