! RUN: not %fort -fsyntax-only %s 2>&1 | %file_check %s
module ! CHECK: expected identifier after 'module'
end module
