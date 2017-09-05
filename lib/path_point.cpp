#include "../include/path_point.h"

path_point::path_point(Point3d _p, Point3d _n)
{
  p.x=_p.x;
  p.y=_p.y;
  p.z=_p.z;

  n.x=_n.x;
  n.y=_n.y;
  n.z=_n.z;
}
