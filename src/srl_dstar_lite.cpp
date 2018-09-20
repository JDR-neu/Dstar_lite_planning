/**
 * @Author: Palmieri Luigi
 * @Modified Zejiang Zeng <yzy>
 * @Date:   2018-03-19T23:07:30-04:00
 * @Email:  zzeng@terpmail.umd.edu
 * @Filename: srl_dstar_lite.h
 * @Last modified by:   yzy
 * @Last modified time: 2018-09-20T08:30:55-04:00
 * @Copyright (C) 2015, Palmieri Luigi, palmieri@informatik.uni-freiburg.de
 *           - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the BSD license.
 */



#include <Dstar_lite_planning/srl_dstar_lite.h>

#include <pluginlib/class_list_macros.h>

using namespace std;

using namespace base_local_planner;


//extern Config config;
tf::TransformListener* listener;


namespace srl_dstar_lite {


/// ==================================================================================
/// Srl_rhcf_planner::transformPose(geometry_msgs::PoseStamped init_pose)
/// Transforms the init_pose in the planner_frame
/// ==================================================================================
geometry_msgs::PoseStamped SrlDstarLite::transformPose(geometry_msgs::PoseStamped init_pose){

        geometry_msgs::PoseStamped res;

        ROS_DEBUG("Transform Pose in D* Lite, in Frame %s", costmap_frame_.c_str());

        tf::StampedTransform transform;

        try{

                // will transform data in the goal_frame into the costmap_frame_
                listener->waitForTransform( costmap_frame_, init_pose.header.frame_id, ros::Time::now(), ros::Duration(0.20));

                listener->lookupTransform(  costmap_frame_,  init_pose.header.frame_id, ros::Time::now(), transform);

        }
        catch(tf::TransformException) {

                ROS_ERROR("Failed to transform the given pose in the D* Lite Planner frame_id");
                return init_pose;
        }

        tf::Pose source;

        tf::Quaternion q = tf::createQuaternionFromRPY(0,0,tf::getYaw(init_pose.pose.orientation));

        tf::Matrix3x3 base(q);

        source.setOrigin(tf::Vector3(init_pose.pose.position.x, init_pose.pose.position.y, 0));

        source.setBasis(base);

        /// Apply the proper transform
        tf::Pose result = transform*source;



        res.pose.position.x = result.getOrigin().x();
        res.pose.position.y = result.getOrigin().y();
        res.pose.position.z = result.getOrigin().z();

        tf::quaternionTFToMsg(result.getRotation(), res.pose.orientation);

        res.header = init_pose.header;
        res.header.frame_id = costmap_frame_;

        return res;

}


/// ==================================================================================
/// setGoal(double x, double y, double theta,double toll)
/// Method to store the Goal region description into the instance of the class
/// ==================================================================================
void SrlDstarLite::setGoal(double x, double y, double theta, double toll, std::string goal_frame){

        ROS_INFO("Setting Goal in D* Lite, Goal Frame %s", goal_frame.c_str());

        tf::StampedTransform transform;

        try{

                // will transform data in the goal_frame into the costmap_frame_
                listener->waitForTransform( costmap_frame_, goal_frame, ros::Time::now(), ros::Duration(0.20));

                listener->lookupTransform( costmap_frame_, goal_frame, ros::Time::now(), transform);

        }
        catch(tf::TransformException) {

                ROS_ERROR("Failed to transform Gaol Transform in D* Lite Planner");
                return;
        }

        tf::Pose source;

        tf::Quaternion q = tf::createQuaternionFromRPY(0,0, theta);

        tf::Matrix3x3 base(q);

        source.setOrigin(tf::Vector3(x, y, 0));

        source.setBasis(base);

        /// Apply the proper transform
        tf::Pose result = transform*source;


        this->goal_theta_= tf::getYaw( result.getRotation());
        this->goal_x_ = result.getOrigin().x();
        this->goal_y_ = result.getOrigin().y();
        this->toll_goal_ = toll;
        this->goal_init_ = true;

        ROS_WARN("GOAL SET!!");
        ROS_WARN("GOAL SET!!");
        ROS_WARN("GOAL SET!!");
        goal_pose_.position.x = result.getOrigin().x();
        goal_pose_.position.y = result.getOrigin().y();
        goal_pose_.position.z = result.getOrigin().z();

        tf::quaternionTFToMsg(result.getRotation(), goal_pose_.orientation);

        this->goal_init_=true;

}

/// ==================================================================================
/// publishPath(Trajectory *t)
/// method to publish the trajectory t
/// ==================================================================================

void SrlDstarLite::publishPath(std::vector< geometry_msgs::PoseStamped > grid_plan){

        int n_points = (int)grid_plan.size();

        nav_msgs::Path path_;

        ROS_DEBUG("Publishing a path generated by D* Lite");
        ROS_DEBUG("Path Size :%d",n_points);

        path_.header.frame_id = costmap_frame_;
        path_.header.stamp = ros::Time();
        path_.poses.resize(n_points);

        visualization_msgs::Marker path_marker_;
        path_marker_.header.frame_id = costmap_frame_;
        path_marker_.header.stamp = ros::Time();
        path_marker_.ns = "SrlDstarLite";
        path_marker_.id = 1;
        path_marker_.type = visualization_msgs::Marker::POINTS;
        path_marker_.color.a = 1;
        path_marker_.color.r = 0.0;
        path_marker_.color.g = 1.0;
        path_marker_.color.b = 0.0;
        path_marker_.scale.x = 0.5;
        path_marker_.scale.y = 0.5;
        path_marker_.scale.z = 0.5;
        path_marker_.action = 0; // add or modify


        for (int i=0; i<n_points; i++) {

                geometry_msgs::PoseStamped posei = grid_plan[i];

                path_.poses[i] = posei;
                /// Path_marker
                geometry_msgs::Point p;
                p.x = posei.pose.position.x;
                p.y = posei.pose.position.y;
                p.z = 0.05;
                path_marker_.points.push_back(p);


        }

        if(grid_plan.size()>0) {

                pub_path_.publish(path_);
                pub_path_dedicated_.publish(path_marker_);

                ROS_DEBUG(" D* LITE Path Published");

        }
}



/// ==================================================================================
/// set_angle_to_range(double alpha, double min)
/// wrap the angle
/// ==================================================================================
double SrlDstarLite::set_angle_to_range(double alpha, double min)
{

        while (alpha >= min + 2.0 * M_PI) {
                alpha -= 2.0 * M_PI;
        }
        while (alpha < min) {
                alpha += 2.0 * M_PI;
        }
        return alpha;
}



/// ==================================================================================
/// SmoothPlan(list<Node> path)
/// smoothing using splines
/// ==================================================================================
vector<RealPoint> SrlDstarLite::SmoothPlan(list<Node> path){

        ROS_DEBUG("SmoothPlan / getting costmap infos");
        double costmap_resolution = costmap_->getResolution();
        double origin_costmap_x = costmap_->getOriginX();
        double origin_costmap_y = costmap_->getOriginY();


        /// copying the path in a different format
        int initial_path_size = (int)path.size();
        vector<RealPoint> input_path;
        input_path.clear();
        if(initial_path_size == 0)
        {
                ROS_ERROR("Path not valid for smoothing, returning");
                return input_path;
        }




        ROS_DEBUG("SmoothPlan, filling the points");


        std::list<Node>::const_iterator iterator;
        double t, old_x, old_y, old_th, dt;
        dt = 0.1;
        int cnt = 0;
        for (iterator = path.begin(); iterator != path.end(); ++iterator) {

                Node node = *iterator;

                /// giving as input path the cartesian path
                double x = (node.x+0.5)*costmap_resolution + origin_costmap_x;
                double y = (node.y+0.5)*costmap_resolution + origin_costmap_y;


                if(cnt>0) {

                        t = dt;

                        while(t<1) {
                                RealPoint p_new;
                                p_new.x = ( x - old_x)*t + old_x;
                                p_new.y = ( y - old_y)*t + old_y;
                                p_new.theta = 0;
                                input_path.push_back(p_new);
                                ROS_DEBUG("Adding point %f %f ", p_new.x, p_new.y);
                                t=t+dt;
                        }
                }else{

                        RealPoint p;
                        p.x = x;
                        p.y = y;
                        p.theta = 0;
                        input_path.push_back(p);
                        ROS_DEBUG("Adding Initial point %f %f of a segment ", x, y);

                }

                old_x = x;
                old_y = y;
                old_th = 0;
                cnt++;

        }

        // do not smooth if the path has not enough points
        if(initial_path_size<3) {
                ROS_DEBUG("Returning path, without smoothing it");
                return input_path;

        }

        ROS_DEBUG("SmoothPlan, Providing the path to the smoother");
        spline_smoother_->readPathFromStruct(input_path);
        ROS_DEBUG("SmoothPlan, Filtering path");
        spline_smoother_->filterPath(1);
        ROS_DEBUG("SmoothPlan, Smoothing path");
        // spline_smoother_->smoothPath2D();
        spline_smoother_->smoothWhileDistanceLessThan(0.05,1.01);
        ROS_DEBUG("SmoothPlan, getting path");
        vector<RealPoint> smooth_path = spline_smoother_->getSmoothPath();


        return input_path;

}



/// ==================================================================================
/// SmoothPlan(std::vector< geometry_msgs::PoseStamped > grid_plan)
/// method to solve a planning probleme.
/// ==================================================================================
list<Node> SrlDstarLite::ShortcutPlan(list<Node> path){

        int initial_path_size = (int)path.size();

        // do not smooth if the path has not enough points
        if(initial_path_size<3)
                return path;
        // Make a straight path between those points:

        list<Node>::iterator e1, e2, temp, end_iter;
        // checkPoint = starting point of path
        e1 = path.begin();
        end_iter = path.end();
        end_iter--;
        {
                // currentPoint = next point in path
                e2 = e1;
                ++e2;


                double costmap_resolution = costmap_->getResolution();
                double origin_costmap_x = costmap_->getOriginX();
                double origin_costmap_y = costmap_->getOriginY();

                // Until the end
                while (e2 != end_iter)
                {
                        Node p_one = *e1;
                        Node p_two = *e2;

                        double x1d = (p_one.x+0.5)*costmap_resolution+origin_costmap_x;
                        double y1d = (p_one.y+0.5)*costmap_resolution+origin_costmap_y;
                        double x2d = (p_two.x+0.5)*costmap_resolution+origin_costmap_x;
                        double y2d = (p_two.y+0.5)*costmap_resolution+origin_costmap_y;

                        unsigned int x1,y1,x2,y2;
                        costmap_->worldToMap(x1d, y1d, x1, y1);

                        //get the cell coord of the second point

                        costmap_->worldToMap(x2d, y2d, x2, y2);

                        double line_cost = world_model_->lineCostVisual(x1, x2,y1, y2);
                        double last_point_cost = world_model_->pointCost(x2, y2);

                        ROS_DEBUG("Checking smooth plan (%d, %d) - (%d, %d), with cost %f and last point cost %f", x1, y1, x2, y2, line_cost, last_point_cost);
                        /// Only if there is free space allow the shortcut
                        if( line_cost>=0)
                        {
                                ROS_DEBUG("Removing connection");
                                /// no obstacles
                                /// temp = currentPoint
                                temp = e2;
                                // currentPoint = currentPoint->next
                                e2++;

                                path.erase(temp);

                        }

                        else
                        {
                                e1 = e2;
                                ++e2;
                        }
                }

        }


        std::list<Node>::const_iterator iterator;
        ROS_DEBUG("Resulting path after smoothing ");
        for (iterator = path.begin(); iterator != path.end(); ++iterator) {

                Node node = *iterator;

                ROS_DEBUG("Node %d %d ", node.x, node.y);

        }

        ROS_DEBUG("Initial Path Size %d ", initial_path_size );
        ROS_DEBUG("Filtered Path Size %d ", (int)path.size() );


        return path;
}




/// ==================================================================================
/// getFootPrintCost(double x_bot, double y_bot, double orient_bot)
/// method to solve a planning probleme.
/// ==================================================================================
double SrlDstarLite::getFootPrintCost(double x_bot, double y_bot, double orient_bot){


        std::vector<geometry_msgs::Point> footprint_spec;

        double cos_th = cos(orient_bot);
        double sin_th = sin(orient_bot);
        for (unsigned int i = 0; i < footprint_spec_.size(); ++i) {
                geometry_msgs::Point new_pt;
                new_pt.x = x_bot + (footprint_spec_[i].x * cos_th - footprint_spec_[i].y * sin_th);
                new_pt.y = y_bot + (footprint_spec_[i].x * sin_th + footprint_spec_[i].y * cos_th);
                footprint_spec.push_back(new_pt);

        }

        geometry_msgs::Point robot_point;

        robot_point.x = x_bot;
        robot_point.y = y_bot;
        robot_point.z = 0;


        double cost = world_model_->footprintCost(robot_point, footprint_spec, 0.3, 0.5);

        ROS_DEBUG("Footprint at %f %f Cost %f", x_bot, y_bot, cost);

        return cost;
}


/// ==================================================================================
/// plan(std::vector< geometry_msgs::PoseStamped > &grid_plan, geometry_msgs::PoseStamped& start)
/// method to solve a planning probleme.
/// ==================================================================================

int SrlDstarLite::plan(std::vector< geometry_msgs::PoseStamped > &grid_plan, geometry_msgs::PoseStamped& start){
        /// TODO plan using the D* Lite Object


        /// 0. Setting Start and Goal points
        /// start
        unsigned int start_mx;
        unsigned int start_my;
        double start_x = start.pose.position.x;
        double start_y = start.pose.position.y;
        costmap_->worldToMap ( start_x, start_y, start_mx, start_my);
        ROS_DEBUG("Update Start Point %f %f to %d %d", start_x, start_y, start_mx, start_my);
        dstar_planner_->updateStart(start_mx, start_my);
        /// goal
        unsigned int goal_mx;
        unsigned int goal_my;
        costmap_->worldToMap (goal_x_, goal_y_, goal_mx, goal_my);
        ROS_DEBUG("Update Goal Point %f %f to %d %d", goal_x_, goal_y_, goal_mx, goal_my);
        dstar_planner_->updateGoal(goal_mx, goal_my);

        /// 1.Update Planner costs
        int nx_cells, ny_cells;
        nx_cells = costmap_->getSizeInCellsX();
        ny_cells = costmap_->getSizeInCellsY();
        ROS_DEBUG("Update cell costs");


        unsigned char* grid = costmap_->getCharMap();
        for(int x=0; x<(int)costmap_->getSizeInCellsX(); x++) {
                for(int y=0; y<(int)costmap_->getSizeInCellsY(); y++) {
                        int index = costmap_->getIndex(x,y);


                        double c = (double)grid[index];

                        if( c >= COST_POSSIBLY_CIRCUMSCRIBED)
                                dstar_planner_->updateCell(x, y, -1);
                        else if (c == costmap_2d::FREE_SPACE) {
                                dstar_planner_->updateCell(x, y, 1);
                        }else
                        {
                                dstar_planner_->updateCell(x, y, c);
                        }
                }
        }

        // /// Update Cell Costs in the dstar_planner, the cell is updated
        // /// in the costmap frame
        // for(int x = 0; x<nx_cells; x++ )
        //     for(int y = 0; y<ny_cells; y++ ){



        //         // double c = this->getFootPrintCost(x,y,0);
        //         double c = costmap_->getCost(x,y);
        //         if( c >= COST_POSSIBLY_CIRCUMSCRIBED)
        //             dstar_planner_->updateCell(x, y, -1);
        //         else if (c == costmap_2d::FREE_SPACE){
        //             dstar_planner_->updateCell(x, y, 1);
        //         }else
        //         {
        //             dstar_planner_->updateCell(x, y, c);
        //         }
        // }

        ROS_DEBUG("Replan");
        /// dstar_planner_->draw();
        /// 2. Plannig using D* Lite
        dstar_planner_->replan();

        ROS_DEBUG("Get Path");
        /// 3. Get Path
        list<Node> path_to_shortcut = dstar_planner_->getPath();

        list<Node> path;

        if(SHORTCUTTING_ON_)
                path = ShortcutPlan(path_to_shortcut);
        else
                path = path_to_shortcut;

        /// 4. Returning the path generated
        grid_plan.clear();
        grid_plan.push_back(start);

        if(!SMOOTHING_ON_) {

                double costmap_resolution = costmap_->getResolution();
                double origin_costmap_x = costmap_->getOriginX();
                double origin_costmap_y = costmap_->getOriginY();

                std::list<Node>::const_iterator iterator;

                for (iterator = path.begin(); iterator != path.end(); ++iterator) {

                        Node node = *iterator;

                        geometry_msgs::PoseStamped next_node;
                        next_node.header.seq = cnt_make_plan_;
                        next_node.header.stamp = ros::Time::now();
                        next_node.header.frame_id = costmap_frame_;
                        next_node.pose.position.x = (node.x+0.5)*costmap_resolution + origin_costmap_x;
                        next_node.pose.position.y = (node.y+0.5)*costmap_resolution + origin_costmap_y;

                        grid_plan.push_back(next_node);

                }

        }else{

                vector<RealPoint> path_smoothed = SmoothPlan(path);
                int size_path_smoothed = (int)(path_smoothed.size());
                ROS_DEBUG("Size of the smoothed path %d", size_path_smoothed);
                for (int j=0; j<size_path_smoothed; j++) {

                        geometry_msgs::PoseStamped next_node;
                        next_node.header.seq = cnt_make_plan_;
                        next_node.header.stamp = ros::Time::now();
                        next_node.header.frame_id = costmap_frame_;

                        next_node.pose.position.x = path_smoothed[j].x;
                        next_node.pose.position.y = path_smoothed[j].y;

                        next_node.pose.orientation = tf::createQuaternionMsgFromRollPitchYaw(0, 0, path_smoothed[j].theta);

                        grid_plan.push_back(next_node);
                }

        }

        if(path.size()>0) {

                publishPath(grid_plan);

                return true;
        }
        else
                return false;

}


/// ==================================================================================
/// makePlan()
/// ==================================================================================
bool SrlDstarLite::makePlan(const geometry_msgs::PoseStamped& start,
                            const geometry_msgs::PoseStamped& goal, std::vector<geometry_msgs::PoseStamped>& plan ){

        // TODO -> Implement a replanning architecture!!!!


        if(this->initialized_) {

                ROS_DEBUG("Setting Goal");
                this->setGoal((double)goal.pose.position.x, (double)goal.pose.position.y, (double)tf::getYaw(goal.pose.orientation), toll_goal_, goal.header.frame_id );
                /// Grid Planning
                /// To check the frame.. Reading in Odom Frame ...
                std::vector< geometry_msgs::PoseStamped > grid_plan;

                geometry_msgs::PoseStamped s;

                ROS_DEBUG("Trasforming Start");
                s = transformPose(start); // TODO: make sure to have goal and start points in the cost map_ frame


                ROS_DEBUG("Start To plan");
                if(this->plan(grid_plan, s)) {


                        plan.clear();
                        cnt_no_plan_= 0;
                        cnt_make_plan_++;

                        for (size_t i = 0; i < grid_plan.size(); i++) {

                                geometry_msgs::PoseStamped posei;
                                posei.header.seq = cnt_make_plan_;
                                posei.header.stamp = ros::Time::now();
                                posei.header.frame_id = costmap_frame_; /// Check in which frame to publish
                                posei.pose.position.x = grid_plan[i].pose.position.x;
                                posei.pose.position.y = grid_plan[i].pose.position.y;
                                // ROS_DEBUG("Position %f %f", posei.pose.position.x, posei.pose.position.y);
                                posei.pose.position.z = 0;
                                posei.pose.orientation = tf::createQuaternionMsgFromRollPitchYaw(0,0,grid_plan[i].pose.position.z);
                                plan.push_back(posei);
                        }

                        ROS_DEBUG("SRL_DSTAR_LITE Path found");
                        return true;

                }
                else
                {
                        cnt_no_plan_++;
                        ROS_WARN("NO PATH FOUND FROM THE D* Lite PLANNER");
                        return false;
                }



        }else{

                return false;
        }
}



/// ==================================================================================
/// initialize()
/// Method to initialize all the publishers and subscribers
/// ==================================================================================
void SrlDstarLite::initialize(std::string name, costmap_2d::Costmap2DROS* costmap_ros){


        if(!initialized_) {

                // number of RRT* iterations

                // ** RRT* Section **
                // 1 Create the planner algorithm -- Note that the min_time_reachability variable acts both
                //                                       as a model checker and a cost evaluator.
                // cout<<"Debug: Initiliaze planner"<<endl;
                /// Need to be set
                this->node_name_ = name;
                this->initialized_ = false;
                this->goal_theta_=0;
                this->goal_x_=0;
                this->goal_y_=0;
                this->cellheight_=1.5;
                this->cellwidth_=1.5;
                this->goal_init_=false;
                this->cnt_make_plan_ = 0;
                this->cnt_no_plan_ = 0;
                this->SMOOTHING_ON_ = true;
                this->SHORTCUTTING_ON_ = false;
                ros::NodeHandle node("~/SrlDstarLite");
                nh_ =  node;

                ROS_INFO("SrlDstarLite ŝtart initializing");



                listener = new tf::TransformListener();

                costmap_ros_ = costmap_ros;

                costmap_ = costmap_ros_->getCostmap();

                costmap_frame_ = "map";


                try{

                        footprint_spec_ = costmap_ros_->getRobotFootprint();

                        if( (int)(footprint_spec_.size()) > 0)
                                ROS_INFO("footprint_spec_ loaded with %d elements", (int)footprint_spec_.size());

                        world_model_ = new CostmapModel(*costmap_);

                        dstar_planner_ = new Dstar();

                        dstar_planner_->init(0, 0, 10, 10); // First initialization

                        spline_smoother_ = new PathSplineSmoother();

                }
                catch (exception& e)
                {
                        ROS_ERROR("An exception occurred. Exception Nr. %s", e.what() );
                }



                /// all the ROS DATA
                // setup publishers

                pub_path_ = nh_.advertise<nav_msgs::Path>("dstar_planner_path", 1);

                pub_path_dedicated_=nh_.advertise<visualization_msgs::Marker>("dstar_path_dedicated",1000);


                ROS_INFO("ROS publishers and subscribers initialized");


                /// Initializing number of read maps
                cnt_map_readings=0;



                /// ==================
                /// READING PARAMETERS
                /// ==================
                /// define dim of scene
                double x1,x2, y1,y2, csx,csy;

                nh_.getParam("cell_size_x", csx);
                nh_.getParam("cell_size_y", csy);
                nh_.getParam("GOAL_TOLL", this->toll_goal_);
                int tcount,firstsol,deburrt;

                nh_.getParam("max_map_loading_time",this->max_map_loading_time);
                nh_.getParam("planner_frame",this->planner_frame_);

                nh_.getParam("SMOOTHING_ON", this->SMOOTHING_ON_);
                nh_.getParam("SHORTCUTTING_ON", this->SHORTCUTTING_ON_);
                /// store dim of scene
                this->xscene_ = x2-x1;
                this->yscene_ = y2-y1;
                /// store sizes
                this->cellwidth_ = csx;
                this->cellheight_ = csy;

                ROS_INFO("RRT_planner initialized");

                int size_footprint = (int)footprint_spec_.size();

                ROS_INFO("Current size of the footprint %d", size_footprint);

                initialized_ = true;
        }else{


                ROS_WARN("D* Lite planner not initialized");
        }

}


}



//register this planner as a BaseGlobalPlanner plugin
PLUGINLIB_EXPORT_CLASS(srl_dstar_lite::SrlDstarLite, nav_core::BaseGlobalPlanner);
