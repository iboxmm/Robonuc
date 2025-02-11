/**
 * @file integrated_referee.cpp
 * @author Tiago Tavares (tiagoa.tavares@hotmail.com)
 * @brief 
 * @version 0.1
 * @date 2019-04-01
 * 
 * @copyright Copyright (c) 2019
 * 
 */

#include "ros/ros.h"
#include <iostream>

#include "std_msgs/String.h" // for /feasibility

#include <sensor_msgs/Joy.h> //for joysitck

#include <r_platform/navi.h> //for /navi_commands

#include "std_msgs/Int8.h" //for /referee_mode

//for action server --> pkg=robonuc_action
#include <actionlib/client/simple_action_client.h>
#include <actionlib/client/terminal_state.h>
#include <robonuc_action/Robot_statusAction.h>

#include <robonuc_plat_orientation_action/Robot_PlatformOrientationAction.h>

using namespace std;

typedef actionlib::SimpleActionClient<robonuc_action::Robot_statusAction> Client_status;
typedef actionlib::SimpleActionClient<robonuc_plat_orientation_action::Robot_PlatformOrientationAction> Client_orientation;

class checker // class checker
{
  public:
    ros::NodeHandle n;
    std::stringstream ss;
    r_platform::navi vel_msg;

    std_msgs::Int8 referee_mode_msg;

    bool robot_allowed = false;

    bool get_action = false; //variable that change when robot should stop
    robonuc_action::Robot_statusGoal goal;
    robonuc_plat_orientation_action::Robot_PlatformOrientationGoal goal_orientation;

    int linear_, angular_;    // id of angular and linear axis (position in the array)
    float l_scale_, a_scale_; // linear and angular scale

    ros::Publisher vel_pub = n.advertise<r_platform::navi>("/navi_commands", 20);

    ros::Publisher referee_pub = n.advertise<std_msgs::Int8>("/referee_mode", 10);

    checker() : linear_(1),
                angular_(3),
                l_scale_(0.025),
                a_scale_(0.025),
                ac("RobotStatusAction", true),
                ac_orientation("GetPlatformOrientation", true)
    {
        ss.str("");

        referee_mode_msg.data = -1;

        ROS_INFO("Waiting for action server to start.");
        ac.waitForServer();
        ac_orientation.waitForServer();
        ROS_INFO("Action server started, can send goal.");

        goal.mode = 1;
        ac.sendGoal(goal);
    }

    void chatterCallback(const std_msgs::String::ConstPtr &msg)
    {
        //ROS_INFO("[integrated_Referee] Iam reading: [%s]", msg->data);
        // cout << "[integrated_Referee] Iam reading:" << msg->data << endl;
        ss.str(msg->data);
        // cout << "ss=" << ss.str() << endl;
    }

    void joyCallback(const sensor_msgs::Joy::ConstPtr &joy)
    {
        // decrease velocity rate
        if (joy->buttons[4] == 1 && a_scale_ > 0 && l_scale_ > 0.0)
        {
            l_scale_ = l_scale_ - 0.025;
            a_scale_ = a_scale_ - 0.025;
            ROS_INFO("DEC v_rate l[%f] a[%f]", l_scale_, a_scale_);
        }

        //increase velocity rate
        if (joy->buttons[5] == 1 && a_scale_ < 0.5 && l_scale_ < 0.5)
        {
            l_scale_ = l_scale_ + 0.025;
            a_scale_ = a_scale_ + 0.025;
            ROS_INFO("INC v_rate l[%f] a[%f]", l_scale_, a_scale_);
        }

        if (joy->axes[2] <= -0.89) // deadman switch active
        {
            if (joy->buttons[0] == 1)
            {
                robot_allowed = true;
                get_action = true;
                ROS_INFO("Button A pressed! Robot=%d", robot_allowed);
            }
        }
        else
        {
            robot_allowed = false;
        }

        // vel_pub_.publish(vel_msg);
    }

    void action_Callback()
    {

        //     vel_msg.linear_vel = l_scale_ * joy->axes[linear_];
        //     vel_msg.angular_vel = a_scale_ * joy->axes[angular_];

        // robot_allowed = true; //comentar!
        if (get_action)
        {
            goal.mode = 2;
            ac.sendGoal(goal);
            //wait for the action to return
            bool finished_before_timeout = ac.waitForResult(ros::Duration(15.0));

            if (finished_before_timeout)
            {
                robonuc_action::Robot_statusResultConstPtr myresult = ac.getResult();
                if (myresult->result == true)
                {
                    robot_allowed = true;
                }
                else
                {
                    robot_allowed = false;
                }
            }
            else
            {
                robot_allowed = false;
            }
        }

        if (robot_allowed == true && ss.str() == "Platform should move.")
        {
            vel_msg.linear_vel = 0.025;
            vel_msg.angular_vel = 0;
            //vel_msg.robot = 0; //este campo nao interessa para o decompose_vel (acho eu)
            cout << "[integrated_referee]PLAT will be moved!" << endl;
            vel_pub.publish(vel_msg);

            referee_mode_msg.data = 1; //auto_picking_mode

            get_action = false;
        }
        else if (ss.str() != "Platform should move.")
        {
            goal.mode = 3;
            ac.sendGoal(goal);

            bool finished_before_timeout_mode3 = ac.waitForResult(ros::Duration(15.0));
            robonuc_action::Robot_statusResultConstPtr myresult_mode3 = ac.getResult();

            if (finished_before_timeout_mode3 && (myresult_mode3->result == true))
            {
                //we can action the orientation
                goal_orientation.goal = 1;
                ac_orientation.sendGoal(goal_orientation);

                bool finished_before_timeout_orientation = ac.waitForResult(ros::Duration(45));
                ROS_INFO("AUTO-ORIENTATION DONE");
            }

            robot_allowed = false;
        }
        else
        {
            cout << "[integrated_referee]PLAT will NOT be moved!" << endl;
            cout << "ss=" << ss.str() << endl;
        }

        referee_pub.publish(referee_mode_msg);
    }

  private:
    Client_status ac;
    Client_orientation ac_orientation;
};

int main(int argc, char **argv)
{
    /**
   * The ros::init() function needs to see argc and argv so that it can perform
   * any ROS arguments and name remapping that were provided at the command line.
   * For programmatic remappings you can use a different version of init() which takes
   * remappings directly, but for most command-line programs, passing argc and argv is
   * the easiest way to do it.  The third argument to init() is the name of the node.
   *
   * You must call one of the versions of ros::init() before using any other
   * part of the ROS system.
   */
    ros::init(argc, argv, "integrated_referee");

    /**
   * NodeHandle is the main access point to communications with the ROS system.
   * The first NodeHandle constructed will fully initialize this node, and the last
   * NodeHandle destructed will close down the node.
   */
    ros::NodeHandle n;

    //class checker
    checker my_checker;

    ros::Subscriber feasibility_sub = n.subscribe("/feasibility", 100, &checker::chatterCallback, &my_checker);

    ros::Subscriber joy_sub_ = n.subscribe<sensor_msgs::Joy>("/joy", 20, &checker::joyCallback, &my_checker);

    ros::Rate loop_rate(200);

    while (ros::ok())
    {
        // my_checker.send_msg.data = my_checker.ss.str();
        // ROS_INFO("%s", msg.data.c_str());

        // cout << "[check_feasibility] Iam pub:" << my_checker.send_msg << endl;
        // chatter_pub.publish(my_checker.send_msg);

        cout << "[integrated_referee]robot_allowed=" << my_checker.robot_allowed << endl;
        my_checker.action_Callback();

        ros::spinOnce();

        loop_rate.sleep();
    }

    return 0;
}