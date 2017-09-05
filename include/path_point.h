#include <iostream>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <opencv2/opencv.hpp>

using namespace std;
using namespace cv;
class path_point
{
    public:
       path_point(Point3d _p, Point3d _n);
       //~math_helper();
       Point3d p;
       Point3d n;
    private:

};
