! RUN: %flang -emit-llvm -o - %s | %file_check %s

program typeTest

  type Point
    real x,y
  end type

  type Triangle
    type(Point) vertices(3)
    integer color
  end type

  type(Point) p    ! CHECK: alloca { float, float }
  type(Triangle) t ! CHECK: alloca { [3 x { float, float }], i32 }

end program
