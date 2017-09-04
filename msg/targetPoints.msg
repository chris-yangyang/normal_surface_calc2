# 3D position of target and tracking point using kinect
float32[3] targetPoint           # target point (x,y,z) with respect to kinect frame of reference
float32[3] trackingPoint         # tracking point (x,y,z) with respect to kinect frame of reference
geometry_msgs/Point32[] path_robot
geometry_msgs/Point32[] normals_robot