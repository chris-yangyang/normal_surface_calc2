//Author:Camilo Perez
//Date:Oct 22 2015

#include <ros/ros.h>
#include "std_msgs/String.h"
#include "normal_surface_calc2/targetPoints.h"
#include "rgb_visualization/pathData.h"

#include <pcl_ros/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/visualization/pcl_visualizer.h>
#include <pcl/filters/passthrough.h>
#include <pcl/octree/octree.h>
#include <pcl/point_types.h>
#include <pcl/features/normal_3d.h>
#include <pcl/features/integral_image_normal.h>
#include <geometry_msgs/PoseStamped.h>
#include <geometry_msgs/Point.h>

#include <Eigen/Geometry>
#include <Eigen/Dense>
#include "math_helper.h"
#include "string_convertor.h"

#define MAX_ITERATION    100
#define RESOLUTION       0.02   //Octree Resolution
using namespace std;
using namespace cv;
typedef pcl::PointCloud<pcl::PointXYZ> PointCloud;
boost::shared_ptr<pcl::visualization::PCLVisualizer> viewer(new pcl::visualization::PCLVisualizer("3D Viewer"));

pcl::PointXYZ hitPoint;
pcl::PointXYZ initialPoint;
pcl::PointXYZ finalPoint;

pcl::PointXYZ targetPoint;
pcl::PointXYZ normalPoint;
pcl::PointXYZ trackingPoint;
pcl::PointXYZ robotEndEffector;
pcl::PointXYZ robotEndEffector_point;
pcl::PointXYZ kinect_robot_x_axis;
pcl::PointXYZ kinect_robot_y_axis;
pcl::PointXYZ kinect_robot_z_axis;
pcl::PointXYZ kinect_robot_zero;
std::vector<pcl::PointXYZ> xyz_path_coordinate;
std::vector<pcl::PointXYZ> xyz_path_normal;
std::vector<pcl::PointXYZ> xyz_path_normal_point;

//pcl::PointXYZ robot; //CP

Eigen::Quaternionf robot_orientation;
Eigen::Vector3f output3_vector, z_input_vector;
Eigen::Matrix4f transformationMatrix, inverse_transformationMatrix;
Eigen::Vector4f robot_position, kinect_robot_position,kinect_robot_normal_point, robot_normal_point, robot_normal_vector; //robot_orientation,z_vector
Eigen::Vector4f kinect_robot_frame_reference_zero,kinect_robot_frame_reference_x,kinect_robot_frame_reference_y,kinect_robot_frame_reference_z,robot_frame_reference_x,robot_frame_reference_y,robot_frame_reference_z, robot_frame_reference_zero;
int target = 0, saving_flag = 0, path_size=0;
float u = 100, v = 100, z = 0, uTarget = 100, vTarget = 100;   //depth of u and v coordinate

float linePoint[4][3] = {0.0, 0.0, 0.0};
std::vector<int> u_path;
std::vector<int> v_path;
bool new_path=false, new_path_robot=false;
std::vector<Eigen::Vector4f> path_robot_v;
std::vector<Eigen::Vector4f> path_robot_r;

std::vector<Eigen::Vector4f> normals_robot_v;
std::vector<Eigen::Vector4f> normals_robot_r;

Eigen::Vector3f normals_robot_r_normalized;
vector< vector <Point> > vPtSignature;

vector< vector <path_point> > getBackStrokes(vector < vector < Point> > _inputPoints, normal_surface_calc2::targetPoints msg)
{
  vector< vector <path_point> > rtVects;
    int strokesNum=_inputPoints.size();
    int counter=0;
    for(int i=0;i<strokesNum;i++)
    {
       vector<path_point> thisStroke;
       for(int j=0;j<_inputPoints[i].size();j++)
       {
           Point3d _p(msg.path_robot[counter].x, msg.path_robot[counter].y, msg.path_robot[counter].z);
           Point3d _n(msg.normals_robot[i].x, msg.normals_robot[i].y, msg.normals_robot[i].z);
           path_point thisP(_p,_n);
           thisStroke.push_back(thisP);
           counter++;
       }
       rtVects.push_back(thisStroke);
     }
     return rtVects;
}

void signature_data_callback(const std_msgs::String::ConstPtr& msg)
{
    if(msg->data=="reset")
    {
       vPtSignature.clear();
       return ;
    }
    vector<string> strokeStrs=string_convertor::split(msg->data, ';');//get different strokes.
    size_t strokesNum=strokeStrs.size();
    if(strokesNum>0)
    {
         vPtSignature.clear();
        vector<vector<Point> >().swap(vPtSignature);
        for(int i=0;i<strokesNum;i++)
        {
          string thisStroke=strokeStrs[i];
          vector<double> points=string_convertor::fromString2Array(thisStroke);
          size_t pointNum=points.size()/2;
          vector<Point> strokePoints;
          for(int j=0;j<pointNum;j++)
              strokePoints.push_back(Point(points[2*j],points[2*j+1]));
          vPtSignature.push_back(strokePoints);
        }
        //published=false;
        u =0;
        v = 0;
        uTarget = 0;
        vTarget = 0;
        if(saving_flag==0) {
          //READ new normal data
          saving_flag = 1;
          u_path.clear();
          v_path.clear();
          u_path=math_helper::getU_Path(vPtSignature);
          v_path=math_helper::getV_Path(vPtSignature);
          path_size=u_path.size();
          new_path=true;
        }
    }
}

Eigen::Matrix4f read_transform_matrix() {
    std::ifstream transform_file;
    std::string kinova_workspace = getenv("CALIBRATION_WORKSPACE");
    std::string transform_file_path = kinova_workspace + "/kinect_wam_transform.txt";
    transform_file.open(transform_file_path.c_str(), std::ios_base::in | std::ios_base::binary);

    if(!transform_file) {
        std::cerr << "Can't open transform file" << std::endl;
        std::exit(-1);
    }

    std::string line;
    Eigen::Matrix4f transform_matrix;
    int i = 0;
    while(getline(transform_file, line) && i < 4) {
        std::istringstream in(line);
        float c1, c2, c3, c4;
        in >> c1 >> c2 >> c3 >> c4;

        transform_matrix(i, 0) = c1;
        transform_matrix(i, 1) = c2;
        transform_matrix(i, 2) = c3;
        transform_matrix(i, 3) = c4;
        ++i;
    }

    //std::cout << transform_matrix <<std::endl;
    return transform_matrix;
    //    return transform_matrix.inverse();
}

void wamPoseCallback(const geometry_msgs::PoseStamped::ConstPtr& poseMessage) {
  robot_orientation.x() =-poseMessage->pose.orientation.x;
  robot_orientation.y() =-poseMessage->pose.orientation.y;
  robot_orientation.z() =-poseMessage->pose.orientation.z;
  robot_orientation.w() =poseMessage->pose.orientation.w;
  robot_position.x()=poseMessage->pose.position.x;
    robot_position.y()=poseMessage->pose.position.y;
    robot_position.z()=poseMessage->pose.position.z;
robot_position.w()=1.0;

output3_vector=robot_orientation._transformVector(z_input_vector);
 robot_normal_vector.x()=0.2*output3_vector.x();
 robot_normal_vector.y()=0.2*output3_vector.y();
 robot_normal_vector.z()=0.2*output3_vector.z();
 robot_normal_vector.w()=1.0;
robot_normal_point=robot_position+robot_normal_vector;
//This is to keep homogeneous coordinates.
robot_normal_point.w()=1.0;
}

void chatterCallback(const rgb_visualization::pathData::ConstPtr& msg2) {
  u = msg2->x;
  v = msg2->y;
  uTarget = msg2->targetx;
  vTarget = msg2->targety;
  if(msg2->savingFlag == 1 && saving_flag==0) {
    //READ new normal data
    saving_flag = 1;
    path_size=msg2->u_path.size();

    u_path.resize(msg2->u_path.size());
    v_path.resize(msg2->v_path.size());

    for (int i = 0; i < path_size; i++) {
      u_path[i]=msg2->u_path[i];
      v_path[i]=msg2->v_path[i];
      //    std::cout << "(u,v)=" << u_path[i]<<" , "<<v_path[i]<< std::endl;
    }
    new_path=true;
  }
  else if(msg2->savingFlag==0)
    {
      saving_flag=0;
    }

}

void callback(const PointCloud::ConstPtr& msg) {
    pcl::octree::OctreePointCloudSearch<pcl::PointXYZ> octree (RESOLUTION);
    pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_filtered(new pcl::PointCloud<pcl::PointXYZ>);  //pointcloud filter
    pcl::PointCloud<pcl::Normal>::Ptr normals (new pcl::PointCloud<pcl::Normal>);

    /////////////////////////////////
    // Create the filtering object
    /////////////////////////////////
    pcl::PassThrough<pcl::PointXYZ> pass;
    pass.setInputCloud(msg);
    pass.setFilterFieldName("x");
    pass.setFilterLimits(-1.5, 0.9);
    pass.setKeepOrganized( true );
    pass.filter(*cloud_filtered);

    //std::cerr << "Cloud after filtering: " << std::endl;

    pcl::IntegralImageNormalEstimation<pcl::PointXYZ,pcl::Normal>ne;
    ne.setNormalEstimationMethod(ne.AVERAGE_3D_GRADIENT);//COVARIANCE_MATRIX
    ne.setMaxDepthChangeFactor(0.02f);
    ne.setNormalSmoothingSize(10.0f);
    ne.setInputCloud(cloud_filtered);
    ne.compute(*normals);
    //int colIndex=u;
    //int rowIndex=v;
    // pcl::Normal normal;
    //ne.computePointNormal((int)u,(int)v,normal);
    //std::cerr << "Cloud after filtering: " << std::endl;



    //CP:
    if(new_path)
      {
	xyz_path_coordinate.resize(path_size);
	xyz_path_normal.resize(path_size);
	xyz_path_normal_point.resize(path_size);

	//resize robot vectors
	path_robot_v.resize(path_size);
	path_robot_r.resize(path_size);
	normals_robot_v.resize(path_size);
	normals_robot_r.resize(path_size);

	for(int i = 0; i <path_size; i++) {
	  xyz_path_coordinate[i]=cloud_filtered->points[v_path[i] * cloud_filtered->width + u_path[i]];

  //	  xyz_path_normal[i]=normals->points[v_path[i]* cloud_filtered->width + u_path[i]];
	  xyz_path_normal[i].x=normals->points[v_path[i]* cloud_filtered->width + u_path[i]].normal_x;
	  xyz_path_normal[i].y=normals->points[v_path[i]* cloud_filtered->width + u_path[i]].normal_y;
	  xyz_path_normal[i].z=normals->points[v_path[i]* cloud_filtered->width + u_path[i]].normal_z;

	  //	   xyz_path_normal_point[i]=xyz_path_coordinate+0.2*xyz_path_normal;
	  xyz_path_normal_point[i].x=xyz_path_coordinate[i].x+0.2*xyz_path_normal[i].x;
	  xyz_path_normal_point[i].y=xyz_path_coordinate[i].y+0.2*xyz_path_normal[i].y;
	  xyz_path_normal_point[i].z=xyz_path_coordinate[i].z+0.2*xyz_path_normal[i].z;

	}

	//CP--

	//std::cerr << "Fill eigen vector: " << std::endl;
	//Fill the eigen vector
	for (int h = 0; h < path_size; h++) {
	  path_robot_v[h].x()=xyz_path_coordinate[h].x;
	  path_robot_v[h].y()=xyz_path_coordinate[h].y;
	  path_robot_v[h].z()=xyz_path_coordinate[h].z;
	  path_robot_v[h].w()=1.0;
	  //Normals to homogeneous representation
	  normals_robot_v[h].x()=xyz_path_normal[h].x;
	  normals_robot_v[h].y()=xyz_path_normal[h].y;
	  normals_robot_v[h].z()=xyz_path_normal[h].z;
	  normals_robot_v[h].w()=1.0;


	}


	//std::cerr << "matrix multipl: " << std::endl;

	//transform kinect path points into robot path points
	for (int k = 0; k < path_size; k++) {
	  path_robot_r[k]=transformationMatrix*path_robot_v[k];
	  //std::cout << "This is the coordinate in the  robot frame of reference" << path_robot_r[k] << std::endl;

	  //CP Working
	  normals_robot_r[k]=transformationMatrix*normals_robot_v[k];
	  //std::cout << "this is the normal in the robot frame of reference" << normals_robot_r[k] << std::endl;



	}
	new_path_robot=true;
	//std::cerr << "TERMINO matrixmultp: " << std::endl;


	new_path=false;
      }

//


    trackingPoint = cloud_filtered->points[(int)v * cloud_filtered->width + (int)u];
    targetPoint = cloud_filtered->points[(int)vTarget * cloud_filtered->width + (int)uTarget];

    float x_normal=normals->points[(int)vTarget * cloud_filtered->width + (int)uTarget].normal_x;

    float y_normal=normals->points[(int)vTarget * cloud_filtered->width + (int)uTarget].normal_y;

    float z_normal=normals->points[(int)vTarget * cloud_filtered->width + (int)uTarget].normal_z;


    //std::cerr << "This is the x component of the normal: "<<x_normal << std::endl;
    normalPoint=targetPoint;

    normalPoint.x=targetPoint.x+x_normal*0.2;
    normalPoint.y=targetPoint.y+y_normal*0.2;
    normalPoint.z=targetPoint.z+z_normal*0.2;



    //std::cout << "Here is the robot Position" << robot_position << std::endl;
    kinect_robot_position=inverse_transformationMatrix*robot_position;
    kinect_robot_normal_point=inverse_transformationMatrix*robot_normal_point; //CP

    //adding robot frame inside the kinect frame

    //end adding robot frame inside the kinect frame


    //std::cout << "Here is the kinect robot Position" << kinect_robot_position << std::endl;

    robotEndEffector.x=kinect_robot_position.x();
    robotEndEffector.y=kinect_robot_position.y();
    robotEndEffector.z=kinect_robot_position.z();

    robotEndEffector_point.x=kinect_robot_normal_point.x();
    robotEndEffector_point.y=kinect_robot_normal_point.y();
    robotEndEffector_point.z=kinect_robot_normal_point.z();




    std::basic_string<char> name = "arrow";
    std::basic_string<char> name2 = "arrow2";

    std::basic_string<char> name3 = "robot_frame_reference_x";
    std::basic_string<char> name4 = "robot_frame_reference_y";
    std::basic_string<char> name5 = "robot_frame_reference_z";


    //viewer->addPointCloud<pcl::PointXYZ>(cloud_filtered, "sample cloud2");
    viewer->addPointCloudNormals<pcl::PointXYZ,pcl::Normal>(cloud_filtered,normals,100,0.02,"sample cloud2",0);

    for (int j = 0; j < xyz_path_coordinate.size(); j++) {
      viewer->addSphere( xyz_path_coordinate[j], 0.015, 0.0, 1.0, 0.0, "point"+boost::lexical_cast<std::string>(j));
      viewer->addArrow<pcl::PointXYZ>( xyz_path_normal_point[j],xyz_path_coordinate[j],0.0,1.0,0.0,false,"normal"+boost::lexical_cast<std::string>(j));
    }


    viewer->addSphere(robotEndEffector, 0.025, 1.0, 0.0, 0.0, "robotEndEffector");
    viewer->addSphere(targetPoint, 0.025, 1.0, 0.0, 0.0, "targetPoint");
    viewer->addArrow<pcl::PointXYZ>(normalPoint,targetPoint,1.0,0.0,0.0,false,name);
    viewer->addArrow<pcl::PointXYZ>(robotEndEffector_point,robotEndEffector,1.0,0.0,0.0,false,name2);
  viewer->addArrow<pcl::PointXYZ>(kinect_robot_x_axis,kinect_robot_zero,1.0,0.0,0.0,false,name3);
  viewer->addArrow<pcl::PointXYZ>(kinect_robot_y_axis,kinect_robot_zero,1.0,0.0,0.0,false,name4);
  viewer->addArrow<pcl::PointXYZ>(kinect_robot_z_axis,kinect_robot_zero,1.0,0.0,0.0,false,name5);



    viewer->spinOnce(100);
    viewer->removePointCloud("sample cloud2");
    viewer->removeShape("targetPoint");
    for (int j = 0; j < xyz_path_coordinate.size(); j++) {
   viewer->removeShape("point"+boost::lexical_cast<std::string>(j));
   viewer->removeShape("normal"+boost::lexical_cast<std::string>(j));
    }

    viewer->removeShape("robotEndEffector");
    viewer->removeShape("arrow2");
    viewer->removeShape("arrow");
    viewer->removeShape("robot_frame_reference_x");
    viewer->removeShape("robot_frame_reference_y");
    viewer->removeShape("robot_frame_reference_z");

}

int main(int argc, char** argv) {
    ros::init(argc, argv, "getPointCloud");
    ros::NodeHandle nh;

    ros::Subscriber sub2 = nh.subscribe("path_data", 100, chatterCallback);
    ros::Subscriber sub3 = nh.subscribe("/zeus/wam/pose", 1, wamPoseCallback);
    ros::Publisher chatter_pub3 = nh.advertise<normal_surface_calc2::targetPoints>("targetPoints", 100);
    ros::Subscriber sub4 = nh.subscribe("/chris/strokes", 1000, signature_data_callback);
    //ros::spinOnce();
    ros::Publisher pubTask2 = nh.advertise<std_msgs::String>("/chris/final_drawing_task", 1, true);//task will be only published once

    viewer->setBackgroundColor(0, 0, 0);
    viewer->setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 1, "sample cloud");
    viewer->addCoordinateSystem(1.0);
    viewer->initCameraParameters();

    ros::Subscriber sub = nh.subscribe<PointCloud>("camera/depth_registered/points", 1, callback); //camera/depth_registered/points

    ros::Rate loop_rate(10);
    ROS_INFO("Main loop Normal_surface_calc");

    transformationMatrix = read_transform_matrix();
    //std::cout << "Here is the transformation matrix" << transformationMatrix << std::endl;
    inverse_transformationMatrix=transformationMatrix.inverse();
    //std::cout << "Here is the inverse transformation matrix" << inverse_transformationMatrix << std::endl;
    /*z_vector.x()=0;
    z_vector.y()=0;
    z_vector.z()=1;
    z_vector.w()=0;
    */

    z_input_vector.x()=0;
    z_input_vector.y()=0;
    z_input_vector.z()=1;


    //robot_frame_reference
    robot_frame_reference_x.x()=0.2;
    robot_frame_reference_x.y()=0.0;
    robot_frame_reference_x.z()=0.0;
    robot_frame_reference_x.w()=1.0;

    robot_frame_reference_y.x()=0.0;
    robot_frame_reference_y.y()=0.2;
    robot_frame_reference_y.z()=0.0;
    robot_frame_reference_y.w()=1.0;

    robot_frame_reference_z.x()=0.0;
    robot_frame_reference_z.y()=0.0;
    robot_frame_reference_z.z()=0.2;
    robot_frame_reference_z.w()=1.0;

    robot_frame_reference_zero.x()=0.0;
    robot_frame_reference_zero.y()=0.0;
    robot_frame_reference_zero.z()=0.0;
    robot_frame_reference_zero.w()=1.0;




    kinect_robot_frame_reference_x=inverse_transformationMatrix*robot_frame_reference_x;
    kinect_robot_frame_reference_y=inverse_transformationMatrix*robot_frame_reference_y;
    kinect_robot_frame_reference_z=inverse_transformationMatrix*robot_frame_reference_z;
    kinect_robot_frame_reference_zero=inverse_transformationMatrix*robot_frame_reference_zero;



    kinect_robot_x_axis.x=kinect_robot_frame_reference_x.x();
    kinect_robot_x_axis.y=kinect_robot_frame_reference_x.y();
    kinect_robot_x_axis.z=kinect_robot_frame_reference_x.z();

    kinect_robot_y_axis.x=kinect_robot_frame_reference_y.x();
    kinect_robot_y_axis.y=kinect_robot_frame_reference_y.y();
    kinect_robot_y_axis.z=kinect_robot_frame_reference_y.z();

    kinect_robot_z_axis.x=kinect_robot_frame_reference_z.x();
    kinect_robot_z_axis.y=kinect_robot_frame_reference_z.y();
    kinect_robot_z_axis.z=kinect_robot_frame_reference_z.z();

    kinect_robot_zero.x=kinect_robot_frame_reference_zero.x();
    kinect_robot_zero.y=kinect_robot_frame_reference_zero.y();
    kinect_robot_zero.z=kinect_robot_frame_reference_zero.z();
    /*
    //CP:ERASE
  robot_orientation.x() =0.0;// poseMessage->pose.orientation.x;
  robot_orientation.y() =0.0; //poseMessage->pose.orientation.y;
  robot_orientation.z() =0.0; //poseMessage->pose.orientation.z;
  robot_orientation.w() =1.0; //poseMessage->pose.orientation.w;
  robot_position.x()=0.3;//poseMessage->pose.position.x;
  robot_position.y()=0.0;//poseMessage->pose.position.y;
  robot_position.z()=0.4;//poseMessage->pose.position.z;
robot_position.w()=1.0;

output3_vector=robot_orientation._transformVector(z_input_vector);
// output3_vector=robot_orientation.toRotationMatrix()*z_input_vector;

std::cout << "Here is the ZZZ vector after multiply by rotation" << output3_vector << std::endl;
 robot_normal_vector.x()=0.2*output3_vector.x();
 robot_normal_vector.y()=0.2*output3_vector.y();
 robot_normal_vector.z()=0.2*output3_vector.z();
 robot_normal_vector.w()=1.0;
//robot_normal_vector=robot_orientation.conjugate()*z_vector*robot_orientation;
robot_normal_point=robot_position+robot_normal_vector;
 robot_normal_point.w()=1.0;

std::cout << "Esto debe ser 0.6 en Z" << robot_normal_point << std::endl;


//END ERASE
*/

    while(ros::ok()) {
        normal_surface_calc2::targetPoints msg_targetPoints;

	/*ROS_INFO("u:%f \n", u);
	ROS_INFO("u:%f \n", v);
        ROS_INFO("uTarget:%f \n", uTarget);
        ROS_INFO("vTarget:%f \n", vTarget);
        ROS_INFO("Tracking Point(x, y, z):(%f, %f, %f) \n", trackingPoint.x, trackingPoint.y, trackingPoint.z);
        ROS_INFO("Target Point(x, y, z):(%f, %f, %f) \n", targetPoint.x, targetPoint.y, targetPoint.z);
	*/
	//	std::cout << "Murio antes del resize" <<std::endl;

	msg_targetPoints.path_robot.resize(path_size);
	msg_targetPoints.normals_robot.resize(path_size);
	//std::cout << "Murio despues del resize" <<std::endl;

	for (int i = 0; i < path_size; i++) {
	  //CP--
	  //std::cout << "Murio antes de asignar" <<std::endl;
	  if(new_path_robot)
	    {
	    //std::cout << "Este es la coordenada en robot" << path_robot_r[i] << std::endl;
	      msg_targetPoints.path_robot[i].x=path_robot_r[i].x();
	      msg_targetPoints.path_robot[i].y=path_robot_r[i].y();
	      msg_targetPoints.path_robot[i].z=path_robot_r[i].z();

	      /* msg_targetPoints.path_robot[i].x=1.0;
		 msg_targetPoints.path_robot[i].y=1.0;
		 msg_targetPoints.path_robot[i].z=1.0;*/

	      // std::cout << "Este es el vector antes de normalizar" << normals_robot_r[i] << std::endl;
	      //normals_robot_r_normalized = normals_robot_r[i].normalized();
	      normals_robot_r_normalized.x()=normals_robot_r[i].x();
	      normals_robot_r_normalized.y()=normals_robot_r[i].y();
	      normals_robot_r_normalized.z()=normals_robot_r[i].z();
	      normals_robot_r_normalized=normals_robot_r_normalized.normalized();
	      //std::cout << "Este es el vector despues de normalizar" << normals_robot_r_normalized << std::endl;
	      //msg_targetPoints.normals_robot[i].x=normals_robot_r[i].x();
	      //msg_targetPoints.normals_robot[i].y=normals_robot_r[i].y();
	      //msg_targetPoints.normals_robot[i].z=normals_robot_r[i].z();

	      //Uncomment this if you want dynamic  normals

	      msg_targetPoints.normals_robot[i].x=normals_robot_r_normalized.x();
	      msg_targetPoints.normals_robot[i].y=normals_robot_r_normalized.y();
	      msg_targetPoints.normals_robot[i].z=normals_robot_r_normalized.z();


	      //Uncomment this if you want fix normals in z direction
	      /* msg_targetPoints.normals_robot[i].x=0;
	      msg_targetPoints.normals_robot[i].y=0;
	      msg_targetPoints.normals_robot[i].z=1;
	      */

	    }

	}



        if(pcl::isFinite(targetPoint) || pcl::isFinite(trackingPoint)) {
            if(pcl::isFinite(targetPoint)) {
                msg_targetPoints.targetPoint[0] = targetPoint.x;
                msg_targetPoints.targetPoint[1] = targetPoint.y;
                msg_targetPoints.targetPoint[2] = targetPoint.z;
            }
            if(pcl::isFinite(trackingPoint)) {
                msg_targetPoints.trackingPoint[0] = trackingPoint.x;
                msg_targetPoints.trackingPoint[1] = trackingPoint.y;
                msg_targetPoints.trackingPoint[2] = trackingPoint.z;
            }
            chatter_pub3.publish(msg_targetPoints);
            vector< vector <path_point> > destPoints=getBackStrokes(vPtSignature,msg_targetPoints);
            std_msgs::String msg;
            msg.data = string_convertor::constructPubStr(destPoints);
            pubTask2.publish(msg);
            //published=true;
        }
        ros::spinOnce();
        loop_rate.sleep();
    }
}
