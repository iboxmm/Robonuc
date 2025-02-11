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
#include <thread>
#include <mutex>
#include <atomic>

#include "std_msgs/String.h" // for /feasibility

#include <sensor_msgs/Joy.h> //for joysitck

#include <r_platform/navi.h> //for /navi_commands

#include "std_msgs/Int8.h" //for /referee_mode

//Create a simple client
#include <actionlib/client/simple_action_client.h>
#include <actionlib/client/terminal_state.h>

//Status Action server - move robot ... /pkg=robonuc_action
#include <robonuc_action/Robot_statusAction.h>

//Camera Orientation Action
#include <robonuc_plat_orientation_action/Robot_PlatformOrientationAction.h>

//Laser Aproximation Action
#include <robonuc_aprox_laser_action/Robot_PlatformLaserAproximationAction.h>

//Bin Picking action
#include <binpicking_action/Robot_binpickingAction.h>

using namespace std;

typedef actionlib::SimpleActionClient<robonuc_action::Robot_statusAction> Client_status;
typedef actionlib::SimpleActionClient<robonuc_plat_orientation_action::Robot_PlatformOrientationAction> Client_orientation;
typedef actionlib::SimpleActionClient<robonuc_aprox_laser_action::Robot_PlatformLaserAproximationAction> Client_Laproximation;

typedef actionlib::SimpleActionClient<binpicking_action::Robot_binpickingAction> Client_Binpicking;

class checker // class checker
{
    std::mutex _mtx;

  public:
    ros::NodeHandle n;
    //r_platform::navi vel_msg;

    std_msgs::Int8 RobotStatus_msg;

    bool robot_allowed = false;

    bool start_auto_picking= false;
    // std::atomic<bool> robot_allowed(false);
    // std::atomic<bool> ready (false);

    int action_mode = 0; //{0,1,2,3,4}

    robonuc_action::Robot_statusGoal goal;
    robonuc_plat_orientation_action::Robot_PlatformOrientationGoal goal_orientation;
    robonuc_aprox_laser_action::Robot_PlatformLaserAproximationGoal goal_aproximation;
    binpicking_action::Robot_binpickingGoal goal_binpicking;

    //move platform
    int linear_, angular_;    // id of angular and linear axis (position in the array)
    float l_scale_, a_scale_; // linear and angular scale
    // ros::Publisher vel_pub = n.advertise<r_platform::navi>("/navi_commands", 20);
    ros::Publisher RobotStatus_pub;


    checker() : linear_(1),
                angular_(3),
                l_scale_(0.025),
                a_scale_(0.025),
                ac("RobotStatusAction", true),
                ac_orientation("GetPlatformOrientation", true),
                ac_laproximation("GetPlatformLaserAproximation", true),
                ac_binpicking("BinPickingAction", true)
    {
        // referee_mode_msg.data = -1;
        RobotStatus_pub = n.advertise<std_msgs::Int8>("RobotStatus", 10);

        ROS_INFO("Waiting for action server to start.");

        ac.waitForServer();
        ac_orientation.waitForServer();
        ac_laproximation.waitForServer();
        ac_binpicking.waitForServer();

        ROS_INFO("Action servers started, can send goals .");

        //robot on mode 1 , navigation
        goal.mode = 1;
        ac.sendGoal(goal);
        action_mode = 1; //pronto para navegação
    }

    void joyCallback(const sensor_msgs::Joy::ConstPtr &joy)
    {
        RobotStatusCallback();
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
                ROS_INFO("Button A pressed! Robot=%d", robot_allowed);
                start_auto_picking= true;
            }
            //ROS_INFO("DEADMAN SWITCH - ON");
        }
        else
        {
            robot_allowed = false;
            if (start_auto_picking)
            {
                ac.cancelAllGoals();
                ac_laproximation.cancelAllGoals();
                ac_orientation.cancelAllGoals();
                ac_binpicking.cancelAllGoals();
                //ROS_INFO("DEADMAN SWITCH - OFF");
                action_mode=1;
                RobotStatusCallback();
                goal.mode = 1;
                ac.sendGoal(goal);
            }
            start_auto_picking = false;

            
        }
        // vel_pub_.publish(vel_msg);
    }

    bool GetlaserAproximation(void)
    {
        //======================LASER APROXIMATION==============

        //manipulator on mode 2
        goal.mode = 2;
        ac.sendGoal(goal);

        bool finished_before_timeout = ac.waitForResult(ros::Duration(15.0)); //wait for the action to return

        if (finished_before_timeout && robot_allowed)
        {
            robonuc_action::Robot_statusResultConstPtr myresult = ac.getResult();
            if (myresult->result != true)
            {
                return false;
            }
        }
        else
        {
            return false;
        }
        //move platform
        goal_aproximation.goal = 1;
        ac_laproximation.sendGoal(goal_aproximation);

        bool finished_before_timeout_aproximation = ac_laproximation.waitForResult(ros::Duration(60));

        if (finished_before_timeout && robot_allowed)
        {
            robonuc_aprox_laser_action::Robot_PlatformLaserAproximationResultConstPtr myresult_l_aprox = ac_laproximation.getResult();

            if (myresult_l_aprox->result != true)
            {
                return false;
            }

            ROS_INFO("AUTO-LASER APROXIMATION DONE");
            return true;
        }
        else
        {
            return false;
        }
    }

    bool GetCameraOrientation(void)
    {
        //=============================Camera ORIENTATION==================
        goal.mode = 3;
        ac.sendGoal(goal);

        bool finished_before_timeout_mode3 = ac.waitForResult(ros::Duration(40));
        //robonuc_action::Robot_statusResultConstPtr myresult_mode = ac.getResult();

        bool finished_before_timeout_orientation = false;

        ROS_INFO("BEFORE IF, %d, %d", robot_allowed, finished_before_timeout_mode3);

        if (finished_before_timeout_mode3 && robot_allowed)
        {
            //we can action the orientation
            ROS_INFO("[after if: ASK ORIENTATION ACTION");
            goal_orientation.goal = 1;
            ac_orientation.sendGoal(goal_orientation);
            ROS_INFO("GOAL SENT");

            finished_before_timeout_orientation = ac_orientation.waitForResult(ros::Duration(60));
            if (finished_before_timeout_orientation)
            {
                robonuc_plat_orientation_action::Robot_PlatformOrientationResultConstPtr myresult_c_orientation = ac_orientation.getResult();

                if (myresult_c_orientation->result != true)
                {
                    return false;
                }
                ROS_INFO("AUTO-ORIENTATION DONE");
                return true;
            }
            else
            {
                return false;
            }
        }
        else
        {
            return false;
        }
    }
    bool GetBinPicking(void)
    {
        goal.mode = 2;
        ac.sendGoal(goal);
        ROS_INFO("GOAL SENT for BINPICKING ACTION");

        bool finished_before_timeout_mode3 = ac.waitForResult(ros::Duration(25.0));
        robonuc_action::Robot_statusResultConstPtr myresult_mode0 = ac.getResult();
        if (finished_before_timeout_mode3)
        {
            //return true;
        }
        else
        {
            return false;
        }
        goal_binpicking.mode = 1;
        ac_binpicking.sendGoal(goal_binpicking);
        bool finished_before_timeout_mode4 = ac_binpicking.waitForResult(ros::Duration(120.0));
        binpicking_action::Robot_binpickingResultConstPtr myresult_mode4 = ac_binpicking.getResult();

        if (myresult_mode4->result != true)
        {
            return false;
        }


        return true;
    }

    void action_Callback()
    {

        if (robot_allowed)
        {
            //======================LASER APROXIMATION==============
            action_mode = 2;
            if (!GetlaserAproximation())
            {
                robot_allowed = false;
                return;
            }
            //=============================Camera ORIENTATION==================
            action_mode = 3;
            ROS_INFO("GO TO ORIENTATION");
            if (!GetCameraOrientation())
            {
                robot_allowed = false;
                return;
            }
            action_mode = 4;
            //=====================PICKING=================
            if (!GetBinPicking())
            {
                robot_allowed = false;
                return;
            }

            action_mode = 1; //return navigation
            goal.mode = 1;
            ac.sendGoal(goal);
            robot_allowed = false;
        }
        else
        {
            //ROS_INFO("[integrated_referee]PLAT will NOT be moved!");
        }
    }

    void RobotStatusCallback(void)
    {
        RobotStatus_msg.data=action_mode;
        RobotStatus_pub.publish(RobotStatus_msg);
    }

    // referee_pub.publish(referee_mode_msg);

  private:
    Client_status ac;
    Client_orientation ac_orientation;
    Client_Laproximation ac_laproximation; //ActionClient_LaserAproimation
    Client_Binpicking ac_binpicking;
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

    ros::Subscriber joy_sub_ = n.subscribe<sensor_msgs::Joy>("/joy", 20, &checker::joyCallback, &my_checker);

    ros::Rate loop_rate(100);

    std::thread thread([&]() {
        while (ros::ok())
        {
            my_checker.action_Callback();
        }
    });

    ros::spin();
    thread.join();

    while (false) //ros::ok())
    {
        // my_checker.send_msg.data = my_checker.ss.str();
        // ROS_INFO("%s", msg.data.c_str());

        // cout << "[check_feasibility] Iam pub:" << my_checker.send_msg << endl;
        // chatter_pub.publish(my_checker.send_msg);

        //cout << "[integrated_referee]robot_allowed=" << my_checker.robot_allowed << endl;
        // my_checker.action_Callback();

        ros::spinOnce();

        loop_rate.sleep();
    }

    return 0;
}