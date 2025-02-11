/**
 * @file sensorRS232_v2.cpp
 * @author Tiago Tavares (adapt from binpicking by joana M)
 * @brief 
 * @version 0.1
 * @date 2019-03-31
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#include "../include/sensorRS232.h"

using namespace std;

char distance_laser[15];
float counter = 0.0;

/**
 * @brief  Reads from port until a specific char. 
 * 
 * This functions reads the values received until it finds a paragraph, to storage the intire distance value.
 *
 * @param  fd - The file descriptor
 * @return none
 */
int ReadPortUntilChar(int fd)
{
    char ch;
    int n;
    do
    {
        n=read( fd, &ch, 1);
        if( n == -1 || n == 0 ) continue;   //perror("Err:");
        sprintf(distance_laser,"%s%c",distance_laser,ch);
        // cout << "ch=" << ch<< endl ;
    } while( ch != '\n');       //Reads until a paragraph is found
    //  cout << "dist_laser=" << distance_laser<< endl ;
     
    return 0;
}

int main (int argc, char** argv)
{
    // Initialize ROS
    ros::init (argc, argv, "bin_picking_sensorRS232");

    ros::NodeHandle nh;
    ros::Publisher pub_rs232 = nh.advertise<std_msgs::Float32>("/output_laser_sensor_2", 200);
    // ros::Rate loop_rate(15);

    int  fd;
    vector <float> readings;

    std_msgs::Float32 fix_value; 
    fix_value.data = 511.8;

    fd=OpenPort("/dev/ttyACM2", NULL); //ttyACM0 se n tiver os outros lasers
    
    
    while (ros::ok() & fd == -1) 
    {
        cout << "Error. Could not open laser port" << endl ;
        cout << "Publishing a randam value for debug" << endl ;
        pub_rs232.publish(fix_value); 
        sleep(1);
        // exit(1); 
    }

    ros::Rate loop_rate(200);

    while ( ros::ok() )
    {
        // cout << "aqui" << endl;
        // fd=OpenPort("/dev/ttyACM0", NULL);

        std_msgs::Float32 dist; 
        ReadPortUntilChar(fd);                  //Reads the distance given by the Arduino UNO
        dist.data = atof(distance_laser);

        // cout << "Distance=" << dist.data << endl; 
        
        pub_rs232.publish(dist);
        // if (dist.data > 100.0 && dist.data < 600.0)
        // {
        //     cout << "Distance=" << dist.data << endl; 
            
        //     if (counter>0) {
        //      readings.push_back(dist.data);
        //      }
        //     counter++;
        // }
        distance_laser[0] = '\0';
        loop_rate.sleep();

        // close(fd);
    }
    
    // float average = accumulate(readings.begin(), readings.end(), 0.0)/readings.size();          

    // cout << "The size is: " << readings.size() << endl; 
    // cout << "The average is: " << average << endl; 
    // std_msgs::Float32 dist_average; 
    // dist_average.data = average;
    close(fd);
    // while (ros::ok())
    // {
    //     // pub_rs232.publish(dist_average);
    //     // cout << "Output Laser just published" << endl;
    // }

    return 0;
}