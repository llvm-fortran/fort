! RUN: %flang -verify < %s
& ! expected-error {{continuation character used out of context}}
PROGRAM continuations
  & ! expected-error {{continuation character used out of context}}
END PROGRAM continuations
