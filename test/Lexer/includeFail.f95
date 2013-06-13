! RUN: %flang < %s
PROGRAM inc
INCLUDE 'thisFileDoesntExist.f95' ! expected-error {{Couldn't find the file 'thisFileDoesntExist.f95'}}
END PROGRAM inc
