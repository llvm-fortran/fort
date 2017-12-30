! RUN: not %fort -fsyntax-only -verify %s 2>&1 | %file_check %s
module mod
end module
module ! CHECK: expected identifier after 'module'
end module
