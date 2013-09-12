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
  type(Point) pa(3)

  p = Point(1.0,0.0)
  p = p ! CHECK: store { float, float } {{.*}}, { float, float }*
  pa(1) = p
  p = pa(1)

  p%x = 1.0
  p%y = p%x
  pa(1)%x = p%y

  ! FIXME: t%vertices(1) = p
  t%color = 0

end program
