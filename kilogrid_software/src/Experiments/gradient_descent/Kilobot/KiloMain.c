#include <stdlib.h>

#include "kilolib.h"
#include "utils.h"
#include <math.h>
#include "kilob_tracking.h"
#include "kilo_rand_lib.h"
//#include "../communication.h"
//#include "kilob_messaging.h"
//#include "kilob_random_walk.h"

#define PI 3.14159265358979323846
#define UNCOMMITTED 0
#define AGENT_MSG 21
#define GRID_MSG 22
#define GLOBAL_MSG 23  // msg mimicing the global communication
#define VIRTUAL_LOCAL_MSG 24
#define INIT_MSG 10

#define size(x)  (sizeof(x) / sizeof((x)[0]))  // length of array bc c is fun

/*-----------------------------------------------------------------------------------------------*/
/* Timer values to select                                                                        */
/*-----------------------------------------------------------------------------------------------*/
#define NUMBER_OF_SAMPLES 45 // we sample each second
#define SAMPLE_NOISE 10  // discrete sample noise equal distribution NUMBER_OF_SAMPLES + 0~10

#define BROADCAST_SEC 0.5  // try to broadcast every x seconds


#define UPDATE_COMMITMENT_SEC 17.5  // updates commitment every 10 sec - was 1
#define UPDATE_NOISE 5*31 // first number is seconds of update rate


/*-----------------------------------------------------------------------------------------------*/
/* Enums section                                                                                 */
/*-----------------------------------------------------------------------------------------------*/
// Enum for different motion types
typedef enum {
    STOP = 0,
    FORWARD,
    TURN_LEFT,
    TURN_RIGHT,
} motion_t;

// Enum for boolean flags
typedef enum {
    false = 0,
    true = 1,
} bool;

// Emum for new way point calculation
typedef enum {
    SELECT_NEW_WAY_POINT = 1,
    UPDATE_WAY_POINT = 0,
} waypoint_option;


/*-----------------------------------------------------------------------------------------------*/
/* Robot state                                                                                   */
/*-----------------------------------------------------------------------------------------------*/
// robot state variables
uint8_t Robot_GPS_X;  // current x position
uint8_t Robot_GPS_Y;  // current y position
uint32_t Robot_orientation;  // current orientation
uint8_t Robot_GPS_X_last;  // last x position  -> needed for calculating the orientation
uint8_t Robot_GPS_Y_last;  // last y position  -> needed for calculating the orientation
motion_t current_motion_type = STOP;  // current type of motion

// robot goal variables
uint8_t Goal_GPS_X = 12;  // x pos of goal
uint8_t Goal_GPS_Y = 14;  // y pos of goal

const uint8_t MIN_DIST = 4;  // min distance the new way point has to differ from the last one
const uint32_t MAX_WAYPOINT_TIME = 3600; // about 2 minutes -> after this time choose new way point
uint32_t lastWaypointTime;  // time when the last way point was chosen

// stuff for motion
const bool CALIBRATED = true;  // flag if robot is calibrated??
const uint8_t MAX_TURNING_TICKS = 37; /* constant to allow a maximum rotation of 180 degrees with \omega=\pi/5 */
const uint32_t MAX_STRAIGHT_TICKS = 300;

uint32_t turning_ticks = 0;  // TODO chnage back to unsigned int?????
uint32_t last_motion_ticks = 0;
uint32_t straight_ticks = 0;

// Wall Avoidance manouvers
uint32_t wallAvoidanceCounter = 0; // to decide when the robot is stuck...
bool stuck = false;  // needed if the robot gets stuck


/*-----------------------------------------------------------------------------------------------*/
/* Arena variables                                                                               */
/*-----------------------------------------------------------------------------------------------*/
const uint8_t GPS_MAX_CELL_X = 20;
const uint8_t GPS_MAX_CELL_Y = 40;

// communication
uint8_t received_x = 0;
uint8_t received_y = 0;
bool received_msg_kilogrid = false;



/*-----------------------------------------------------------------------------------------------*/
/* Normalizes angle between -180 < angle < 180.                                                  */
/*-----------------------------------------------------------------------------------------------*/
void NormalizeAngle(double* angle){
    while(*angle>180){
        *angle=*angle-360;
    }
    while(*angle<-180){
        *angle=*angle+360;
    }
}


/*-----------------------------------------------------------------------------------------------*/
/* Function for setting the motor speed.                                                         */
/*-----------------------------------------------------------------------------------------------*/
void set_motion(motion_t new_motion_type) {
    // only update current motion if change
    if(current_motion_type != new_motion_type){
        current_motion_type = new_motion_type;

        switch( new_motion_type ){
            case FORWARD:
                spinup_motors();
                if (CALIBRATED) set_motors(kilo_straight_left,kilo_straight_right);
                else set_motors(67,67);
                break;
            case TURN_LEFT:
                spinup_motors();
                if (CALIBRATED){
                    uint8_t leftStrenght = kilo_turn_left;
                    uint8_t i;
                    for (i=3; i <= 18; i += 3){
                        if (wallAvoidanceCounter >= i){
                            leftStrenght+=2;
                        }
                    }
                    set_motors(leftStrenght,0);
                }else{
                    set_motors(70,0);
                }
                break;
            case TURN_RIGHT:
                spinup_motors();
                if (CALIBRATED){
                    uint8_t rightStrenght = kilo_turn_right;
                    uint8_t i;
                    for (i=3; i <= 18; i += 3){
                        if (wallAvoidanceCounter >= i){
                            rightStrenght+=2;
                        }
                    }
                    set_motors(0,rightStrenght);
                }
                else{
                    set_motors(0,70);
                }
                break;
            case STOP:
            default:
                set_motors(0,0);
        }
    }
}



/*-----------------------------------------------------------------------------------------------*/
/* Function to check if the robot is aganst the wall                                             */
/*-----------------------------------------------------------------------------------------------*/
void check_if_against_a_wall() {
    // handles target when we are at a wall?
    if(hit_wall){
        // drive towards the map center if coliding with a wall??
        if (wallAvoidanceCounter<18) wallAvoidanceCounter += 1;
        else wallAvoidanceCounter = 1;
    } else {
        if (wallAvoidanceCounter > 0){ // flag when the robot hit a wall -> after select new waypoint -> else ignore
            random_walk_waypoint_model(SELECT_NEW_WAY_POINT);
        }
        wallAvoidanceCounter = 0;
    }
}


/*-----------------------------------------------------------------------------------------------*/
/* Function to go to the Goal location, called every cycle,                                      */
/*-----------------------------------------------------------------------------------------------*/
void GoToGoalLocation() {
    // only recalculate movement if the robot has an update on its orientation aka. moved
    // if hit wall do the hit wall case... also stuck ensures that they drive straight forward for
    // a certain amount of time
    if (update_orientation){
        update_orientation = false;
        
        // calculates the difference thus we can see if we have to turn left or right
        double angletogoal = atan2(Goal_GPS_Y-Robot_GPS_Y,Goal_GPS_X-Robot_GPS_X)/PI*180-Robot_orientation;
        NormalizeAngle(&angletogoal);
        
        // see if we are on track
        bool right_direction = false; // flag set if we move towards the right celestial direction
        if(Robot_GPS_Y == Goal_GPS_Y && Robot_GPS_X < Goal_GPS_X){ // right case
            if(Robot_orientation == 0){ right_direction = true;}
        }else if (Robot_GPS_Y > Goal_GPS_Y && Robot_GPS_X < Goal_GPS_X){  // bottom right case
            if(Robot_orientation == -45){ right_direction = true;}
        }else if (Robot_GPS_Y > Goal_GPS_Y && Robot_GPS_X == Goal_GPS_X){  // bottom case
            if(Robot_orientation == -90){ right_direction = true;}
        }else if (Robot_GPS_Y > Goal_GPS_Y && Robot_GPS_X > Goal_GPS_X){  // bottom left case
            if(Robot_orientation == -135){ right_direction = true;}
        }else if (Robot_GPS_Y == Goal_GPS_Y && Robot_GPS_X > Goal_GPS_X){  // left case
            if(Robot_orientation == -180 || Robot_orientation == 180){ right_direction = true;}
        }else if (Robot_GPS_Y < Goal_GPS_Y && Robot_GPS_X > Goal_GPS_X){  // left upper case
            if(Robot_orientation == 135){ right_direction = true;}
        }else if (Robot_GPS_Y < Goal_GPS_Y && Robot_GPS_X == Goal_GPS_X){  // upper case
            if(Robot_orientation == 90){ right_direction = true;}
        }else if (Robot_GPS_Y < Goal_GPS_Y && Robot_GPS_X < Goal_GPS_X){  // right upper case
            if(Robot_orientation == 45){ right_direction = true;}
        }else{
            printf("[%d] ERROR - something wrong in drive cases \n", kilo_uid);
        }
        
        // if we are not in the right direction -> turn
        if (!right_direction){
            // right from target
            if ((int)angletogoal < 0 ) set_motion(TURN_RIGHT);
            // left from target
            else if ((int)angletogoal > 0) set_motion(TURN_LEFT);
            turning_ticks= MAX_TURNING_TICKS;
            last_motion_ticks = kilo_ticks;

        }else{
            set_motion(FORWARD);
        }
    }
    // needed at least for the beginning so that the robot starts moving into the right direction
    // but also to update after turning for a while -> move then straight to update orientation
    switch( current_motion_type ) {
        case TURN_LEFT:
        case TURN_RIGHT:
            if( kilo_ticks > last_motion_ticks + turning_ticks) {
                /* start moving forward */
                last_motion_ticks = kilo_ticks;  // fixed time FORWARD
                if (hit_wall){  // only enforce straight movement when trying to escape a wall
                    straight_ticks = rand() % MAX_STRAIGHT_TICKS + 150;
                    stuck = true;
                }
                set_motion(FORWARD);
            }
            break;
        case FORWARD:
            if(kilo_ticks > last_motion_ticks + straight_ticks){
                last_motion_ticks = kilo_ticks;
                stuck = false;
            }
            break;

        case STOP:
        default:
            set_motion(STOP);
    }
}

void update_robot_state(){
    // update reading of sensor - also check for hit_wall flag - setting it inside callback
    // function could lead to inconsistancy!!
//    if (received_option == 42){  // robot sensed wall -> dont update the received option
//        hit_wall = true;
//    }else{
//        current_ground = received_option;
//        hit_wall = false;
//    }
    
    // update current and last position
    if (received_x != Robot_GPS_X || received_y != Robot_GPS_Y){
        Robot_GPS_X_last = Robot_GPS_X;
        Robot_GPS_X = received_x;
        Robot_GPS_Y_last = Robot_GPS_Y;
        Robot_GPS_Y = received_y;
        update_orientation = true;
    }
    
    // calculate orientation of the robot based on the last and current visited cell -> rough estimate
    double angleOrientation = atan2(Robot_GPS_Y-Robot_GPS_Y_last, Robot_GPS_X-Robot_GPS_X_last)/PI*180;
    NormalizeAngle(&angleOrientation);
    Robot_orientation = (uint32_t) angleOrientation;
    
    received_msg_kilogrid = false;
}

/*-----------------------------------------------------------------------------------------------*/
/* Init function                                                                                 */
/*-----------------------------------------------------------------------------------------------*/
void setup(){
    // Initialise motors
    set_motors(0,0);
    
    // Initialise random seed
    
    // Initialise motion variables
    last_motion_ticks = rand() % MAX_STRAIGHT_TICKS + 1;

    
    // init robot state
    Robot_GPS_X_last = GPS_MAX_CELL_X/2;
    Robot_GPS_Y_last = GPS_MAX_CELL_Y/2;
    Robot_orientation = 0;
    // initialise the GSP to the middle of the environment, to avoid to trigger wall avoidance immediately
    Robot_GPS_X = GPS_MAX_CELL_X/2;
    Robot_GPS_Y = GPS_MAX_CELL_Y/2;
        
    // Intialize time to 0
    kilo_ticks = 0;
}


/*-------------------------------------------------------------------*/
/* Callback function for message reception                           */
/*-------------------------------------------------------------------*/
void message_rx( message_t *msg, distance_measurement_t *d ) {
    received_x = msg->data[0];
    received_y = msg->data[1];
    received_msg_kilogrid = true;
    return;
}

/*-----------------------------------------------------------------------------------------------*/
/* Callback function for message transmission - this methods do not need to be implemented for   */
/* the simulation bc inter robot communication is handled by the kilogrid                        */
/*-----------------------------------------------------------------------------------------------*/
message_t *message_tx() {
    return NULL;
}

/*-----------------------------------------------------------------------------------------------*/
/* Callback function for successful transmission  - this methods do not need to be implemented   */
/* for the simulation bc inter robot communication is handled by the kilogrid                    */
/*-----------------------------------------------------------------------------------------------*/
void tx_message_success() {
    return;
}

/*-----------------------------------------------------------------------------------------------*/
/* Main loop                                                                                     */
/*-----------------------------------------------------------------------------------------------*/
void loop() {
    if(received_msg_kilogrid){
        update_position();
    }
    // move towards random location
    GoToGoalLocation();

}

/*-----------------------------------------------------------------------------------------------*/
/* Main function                                                                                 */
/*-----------------------------------------------------------------------------------------------*/
int main(){
    kilo_init();  // init hardware of the kilobot
    kilo_message_tx = message_tx;  // register callback - pointer to message which should be send - roughly every 0.5 s
    kilo_message_tx_success = tx_message_success;  // triggered when transmission is successfull
    kilo_message_rx = message_rx;  // callback for received messages

    kilo_start(setup, loop);
    return 0;
}