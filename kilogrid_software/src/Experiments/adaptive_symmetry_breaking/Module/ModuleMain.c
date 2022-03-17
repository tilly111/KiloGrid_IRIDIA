// IMPORTANT NOTES:
// SENDING TO MUCH WILL BLOCK THE CAN AND YOU CANNOT SEND ANY CONTROL MESSAGES ANYMORE!!!!!

// error explanation:
// left/up border does not send bc its encoded as wall = 0 which is not sending 
// bottom/right border ???

#include <avr/io.h>
#include <avr/wdt.h>
#include <util/delay.h>     // delay macros
#include <avr/interrupt.h>
#include <math.h>

#include "module.h"
#include "moduleLED.h"
#include "moduleIR.h"
#include "CAN.h"
#include "mcp2515.h"

#include "../communication.h"

// #define MAX_MESSAGE_REPEAT 1  // defines how often a message is maximum repeated - if in the mean time a other message arrives the timer is resetted 
#define MAX_RESET_TIMER 500

typedef enum{
    false = 0,
    true = 1,
} bool;

typedef struct {
    uint8_t x;
    uint8_t y; 
}vec2D;

volatile uint8_t cell_x[4] = {0, 0, 0, 0};
volatile uint8_t cell_y[4] = {0, 0, 0, 0};
volatile uint8_t cell_role[4] = {0, 0, 0, 0};
volatile uint8_t cell_colour[4] = {0, 0, 0, 0};

volatile uint8_t cell_received_op[4] = {0, 0, 0, 0};


// uint8_t send = 0;
// uint8_t receive_cell = 10;
// uint8_t send_success = 0;
// uint32_t broadcast_timer = 0;

// uint8_t received_option = 0;
// uint8_t received_x = 0;
// uint8_t received_y = 0;

// uint8_t com_range = 0;
// uint8_t option = 0;
// uint8_t my_x = 0;
// uint8_t my_y = 0;

// grid which stores where to send msgs -> this data structure is to big 
// uint8_t sending_grid[10][20][4];

// CAN_message_t test_message;  // for structure of a can msg see communication/mcp2515.h
// kilogrid_address_t dest_cell_address;
kilogrid_address_t cell_address;
IR_message_t test_ir_msg;
CAN_message_t tmp_can_msg;

// uint8_t com_range_module;
// uint8_t send_success_sum;
// uint8_t my_x_module;
// uint8_t my_y_module;
// uint8_t msg_number_current = 0;
// uint8_t msg_number = 20;

// uint8_t send_flag;

// iterators .. init here to see if we exceed storage limits!
uint8_t i_it;
uint8_t k_it;
uint8_t l_it;

uint8_t x_it;
uint8_t y_it;
uint8_t cell_it;

// uint8_t message_repeat_timer = 0;
uint8_t receive_timer = 0;

uint32_t reset_timer[4] = {0, 0, 0, 0};

// Todo: delete this is only for debugging the kilogrid can communication
uint32_t some_send_counter = 0;
uint32_t some_colour_counter = 0;
uint32_t some_cycle_counter = 0;
uint32_t last_cycle_counter = 0;
bool init = false;
uint8_t current_colour[4] = {10, 10, 10, 10};


cell_num_t cell_id[4] = {CELL_00, CELL_01, CELL_02, CELL_03};

// tmp storage - needed bc callbacks
uint8_t tmp_ir_data[4][8];
bool received_ir = false;
uint8_t tmp_can_data[8];  // can only be received by the module, thats why we do not have 4
bool received_can = false;

// data calculation
uint8_t received_commitment[4] = {0,0,0,0};
uint8_t received_com_range[4] = {0,0,0,0};
uint8_t received_robot_x[4] = {0,0,0,0};
uint8_t received_robot_y[4] = {0,0,0,0};

// data for sending
uint8_t opt_to_send_ir[4] = {0,0,0,0};
bool ir_msg_to_send = false;

bool can_msg_to_send = false;

IR_message_t IR_message_tx[4];
IR_message_t IR_msg_rx;

tracking_user_data_t tracking_data;

/*-----------------------------------------------------------------------------------------------*/
/* utility functions                                                                             */
/*-----------------------------------------------------------------------------------------------*/
vec2D module2cell(vec2D module_pos, cell_num_t cn) {
    vec2D cell_pos;
    uint8_t cur_x = module_pos.x * 2;
    uint8_t cur_y = module_pos.y * 2;

    switch (cn) {
        case 0:
            cur_y += 1;
            break;
        case 1:
            cur_x += 1;
            cur_y += 1;
            break;
        case 2:
            break;
        case 3:
            cur_x += 1;
    }
    cell_pos.x = cur_x;
    cell_pos.y = cur_y;

    return cell_pos;
}

vec2D cell2module(vec2D cell_pos) {
    vec2D module_pos;
    module_pos.x = (uint8_t)(cell_pos.x / 2);
    module_pos.y = (uint8_t)(cell_pos.y / 2);

    return module_pos;
}


/*-----------------------------------------------------------------------------------------------*/
/* messaging functions                                                                           */
/*-----------------------------------------------------------------------------------------------*/
void IR_rx(IR_message_t *m, cell_num_t c, distance_measurement_t *d, uint8_t CRC_error) {
    // if(!CRC_error && m->type == TRACKING){
    //     CAN_message_t* msg = next_CAN_message();
    //     // TODO: hunting warnings, why does tracking_user_data_t has only 7 bytes
    //     //  maybe we need to do a shift here ????
    //     tracking_user_data_t usr_data; 
    //     usr_data.byte[0] = m->data[0];
    //     usr_data.byte[1] = m->data[1];
    //     usr_data.byte[2] = m->data[2];
    //     usr_data.byte[3] = m->data[3];
    //     usr_data.byte[4] = m->data[4];
    //     usr_data.byte[5] = m->data[5]; 
    //     usr_data.byte[6] = m->data[6];
    //     if(msg != NULL) { // if the buffer is not full
    //         serialize_tracking_message(msg, c, &usr_data);
    //     }
    // }else if(!CRC_error && m->type == 11){  // VIRTUAL_AGENT_MSG
    //     // set tmp data 
    //     for (uint8_t tmp_it = 0; tmp_it < 8; tmp_it++){
    //         tmp_ir_data[c][tmp_it] = m->data[tmp_it];
    //     }
    //     received_ir = true;
    // }
}


// callback for receiving CAN messages 
void CAN_rx(CAN_message_t *m) { 
    if (m->data[0] == CAN_MODULE_TO_MODULE){  // read msg
        for(uint8_t tmp_it = 0; tmp_it < 8; tmp_it++){
            tmp_can_data[tmp_it] = m->data[tmp_it];
        }
        received_can = true;
    }
    // else {
    // 	// here you can process other messages - should never happen 
    // }
}

//void CAN_tx_success(){
//    send_success = 1;
//    return;
//}


// called when pressing setup in the kilogui
void setup(){
    cell_x[0] = (configuration[0] * 2);
    cell_x[1] = (configuration[0] * 2 + 1);
    cell_x[2] = (configuration[0] * 2);
    cell_x[3] = (configuration[0] * 2 + 1);

    cell_y[0] = (configuration[1] * 2 + 1);
    cell_y[1] = (configuration[1] * 2 + 1);
    cell_y[2] = (configuration[1] * 2);
    cell_y[3] = (configuration[1] * 2);

    cell_role[0] = (configuration[2]);
    cell_role[1] = (configuration[2]);
    cell_role[2] = (configuration[2]);
    cell_role[3] = (configuration[2]);

    cell_colour[0] = (configuration[3]);
    cell_colour[1] = (configuration[4]);
    cell_colour[2] = (configuration[5]);
    cell_colour[3] = (configuration[6]);

    // init tmp receive msgs
    for(k_it = 0; k_it < 4; k_it++){
        for(l_it = 0; l_it < 8; l_it++){
            tmp_ir_data[k_it][l_it] = 0;
        }
    }

    // for (i_it = 0; i_it < 4; i_it++){
    // 	set_LED_with_brightness(cell_id[i_it], WHITE, HIGH);
    // }

}


void loop() {
    // compute ir msg 
    if (received_ir){
        can_msg_to_send = false;
        for(cell_it = 0; cell_it < 4; cell_it++){
            // msg structure: robot_commitment, communication range, robot_x, robot_y, msg_number_sent
            if (tmp_ir_data[cell_it][0] != 0){
                    received_commitment[cell_it] = tmp_ir_data[cell_it][0];
                    received_com_range[cell_it] = tmp_ir_data[cell_it][1];
                    received_robot_x[cell_it] = tmp_ir_data[cell_it][2];
                    received_robot_y[cell_it] = tmp_ir_data[cell_it][3];
                    ir_msg_to_send = true;
            }
            // clear 
            for(k_it = 0; k_it < 4; k_it++){
                tmp_ir_data[cell_it][k_it] = 0;
            }
        }
    }

    // compute can msg
    // can data structure: flag, x_sender, y_sender, com_range, commitment 
    if (received_can){
        received_can = false;
        for(cell_it = 0; cell_it < 4; cell_it++){
            if (sqrt(pow(fabs(tmp_can_data[1] - cell_x[cell_it]), 2) + pow(fabs(tmp_can_data[2] - cell_y[cell_it]), 2)) < tmp_can_data[3]) {
                ir_msg_to_send = true;
                opt_to_send_ir[cell_it] = tmp_can_data[4];
            } else{
            	opt_to_send_ir[cell_it] = current_colour[cell_it];  // this should be nothing, only called for cells when receiving a can message, and it is not in range
            	
            }
        }
    }

    // set ir msg
    if (ir_msg_to_send){
        ir_msg_to_send = false;
        for (cell_it = 0; cell_it < 4; cell_it++){
            current_colour[cell_it] = opt_to_send_ir[cell_it];
            // TODO: do proper sending of the message, now only colour
        }
    }

    // // set can msg 
    // if (can_msg_to_send){
    //     can_msg_to_send = false;
    //     for(cell_it = 0; cell_it < 4; cell_it++){
    //         if (received_commitment[cell_it] != 0){
    //             init_CAN_message(&tmp_can_msg);
    //             tmp_can_msg.id = 55;  // should be fine
    //             tmp_can_msg.data[0] = 55; // message id
    //             tmp_can_msg.data[1] = received_robot_x[cell_it]; // x sender 
    //             tmp_can_msg.data[2] = received_robot_y[cell_it]; // y sender 
    //             tmp_can_msg.data[3] = received_com_range[cell_it]; // range
    //             tmp_can_msg.data[4] = received_commitment[cell_it]; // commitment 
    //             tmp_can_msg.data[5] = some_cycle_counter - last_cycle_counter; // debug info
    //             tmp_can_msg.data[6] = 0;
    //             tmp_can_msg.data[7] = 0;
    //             last_cycle_counter = some_cycle_counter;
                
    //             cell_address.type = ADDR_BROADCAST; // see communication/kilogrid.h for further information
    //             cell_address.x = 0;  // is the position of a module imo??
    //             cell_address.y = 0;
    //             CAN_message_tx(&tmp_can_msg, cell_address); 
                // TODO: set cells of this module as well!!!!!!!
    //             // reset 
    //             received_commitment[cell_it] = 0;
    //         }
    //     }
    // }


    some_cycle_counter += 1; 
    some_send_counter += 1;
    for (i_it = 0; i_it < 4; i_it++){
        if (cell_x[i_it] == 5 && cell_y[i_it] == 20){
            if (some_send_counter % 100 == 0){ //  % 200 == 0
                current_colour[i_it] = (current_colour[i_it] + 1) % 3;
                init_CAN_message(&tmp_can_msg);
                tmp_can_msg.id = current_colour[i_it];  // dont know if id is important - maybe to check if msg arrived twice or so - max 65,535
                tmp_can_msg.data[0] = CAN_MODULE_TO_MODULE; // message id - see CAN_message_type_t @ CAN.h and process_CAN_message @ module.c 
                tmp_can_msg.data[1] = 12; // x sender 
                tmp_can_msg.data[2] = 25; // y sender 
                tmp_can_msg.data[3] = 5; // range
                tmp_can_msg.data[4] = current_colour[i_it]; // information
                tmp_can_msg.data[5] = some_cycle_counter - last_cycle_counter; // debug info
                tmp_can_msg.data[6] = 0;
                tmp_can_msg.data[7] = 0;
                last_cycle_counter = some_cycle_counter;
                
                cell_address.type = ADDR_BROADCAST; // see communication/kilogrid.h for further information
                cell_address.x = 0;  // is the position of a module imo??
                cell_address.y = 0;
                CAN_send_broadcast_message(&tmp_can_msg);
                //add_CAN_message_to_buffer();

                // for all cells in my module
                current_colour[0] = current_colour[i_it];
                current_colour[1] = current_colour[i_it];
                current_colour[2] = current_colour[i_it];
                current_colour[3] = current_colour[i_it];
            }
        }
        // if (cell_x[i_it] == 5 && cell_y[i_it] == 5){
        //     if (some_send_counter % 200 == 0){ // 
        //         current_colour[i_it] = (current_colour[i_it] + 1) % 3;
        //         init_CAN_message(&tmp_can_msg);
        //         tmp_can_msg.id = 55;  // dont know if id is important - maybe to check if msg arrived twice or so - max 65,535
        //         tmp_can_msg.data[0] = 55; // message id
        //         tmp_can_msg.data[1] = 5; // x sender 
        //         tmp_can_msg.data[2] = 5; // y sender 
        //         tmp_can_msg.data[3] = 5; // range
        //         tmp_can_msg.data[4] = current_colour[i_it]; // information
        //         tmp_can_msg.data[5] = some_cycle_counter - last_cycle_counter; // debug info
        //         tmp_can_msg.data[6] = 0;
        //         tmp_can_msg.data[7] = 0;
        //         last_cycle_counter = some_cycle_counter;
                
        //         cell_address.type = ADDR_BROADCAST; // see communication/kilogrid.h for further information
        //         cell_address.x = 0;  // is the position of a module imo??
        //         cell_address.y = 0;
        //         CAN_message_tx(&tmp_can_msg, cell_address); 
        //         _delay_ms(1);
        //         //add_CAN_message_to_buffer();

        //         // for all cells in my module
        //         current_colour[0] = current_colour[i_it];
        //         current_colour[1] = current_colour[i_it];
        //         current_colour[2] = current_colour[i_it];
        //         current_colour[3] = current_colour[i_it];
        //     }
        // }
        // if (cell_x[i_it] == 12 && cell_y[i_it] == 30){
        //     if (some_send_counter % 200 == 0){ // 
        //         current_colour[i_it] = (current_colour[i_it] + 1) % 3;
        //         init_CAN_message(&tmp_can_msg);
        //         tmp_can_msg.id = 55;  // dont know if id is important - maybe to check if msg arrived twice or so - max 65,535
        //         tmp_can_msg.data[0] = 55; // message id
        //         tmp_can_msg.data[1] = 12; // x sender 
        //         tmp_can_msg.data[2] = 30; // y sender 
        //         tmp_can_msg.data[3] = 10; // range
        //         tmp_can_msg.data[4] = current_colour[i_it]; // information
        //         tmp_can_msg.data[5] = some_cycle_counter - last_cycle_counter; // debug info
        //         tmp_can_msg.data[6] = 0;
        //         tmp_can_msg.data[7] = 0;
        //         last_cycle_counter = some_cycle_counter;
                
        //         cell_address.type = ADDR_BROADCAST; // see communication/kilogrid.h for further information
        //         cell_address.x = 0;  // is the position of a module imo??
        //         cell_address.y = 0;
        //         CAN_message_tx(&tmp_can_msg, cell_address); 
        //         _delay_ms(1);
        //         //add_CAN_message_to_buffer();

        //         // for all cells in my module
        //         current_colour[0] = current_colour[i_it];
        //         current_colour[1] = current_colour[i_it];
        //         current_colour[2] = current_colour[i_it];
        //         current_colour[3] = current_colour[i_it];
        //     }
        // }
    }

    if(some_cycle_counter % 100 == 0){
    	// init_ModuleCAN(module_uid_x_coord, module_uid_y_coord);
    	// for(cell_it = 0; cell_it < 4; cell_it++){  // todo: delete, only for debugging 
    	// 	current_colour[cell_it] = debug_till();
	    // }
	    // // tracking cells which receive stuff
     //    CAN_message_t* msg = next_CAN_message();
     //    tracking_user_data_t usr_data; 
     //    usr_data.byte[0] = cell_x[0];
     //    usr_data.byte[1] = cell_y[0];
     //    usr_data.byte[2] = 1;
     //    usr_data.byte[3] = 2;
     //    usr_data.byte[4] = 3;
     //    usr_data.byte[5] = 4; 
     //    usr_data.byte[6] = 5;
     //    if(msg != NULL) { // if the buffer is not full
     //        serialize_tracking_message(msg, cell_id[0], &usr_data);
     //    }
    }





    // set colour imo only needed for debugging 
    for(cell_it = 0; cell_it < 4; cell_it++){
        switch(current_colour[cell_it]){
            case 0:
                set_LED_with_brightness(cell_id[cell_it], RED, HIGH);
                break;
            case 1:
                set_LED_with_brightness(cell_id[cell_it], GREEN, HIGH);
                break;
            case 2:
                set_LED_with_brightness(cell_id[cell_it], BLUE, HIGH);
                break;
            case 3:
                set_LED_with_brightness(cell_id[cell_it], MAGENTA, HIGH);
                break;
            default:
                set_LED_with_brightness(cell_id[cell_it], WHITE, HIGH);
                break;
        }
    }

    _delay_ms(5);
}



int main() {
    module_init();

    // register function callbacks
    module_CAN_message_rx = CAN_rx;

    //module_CAN_message_tx_success = CAN_message_tx_success_my;
    //module_CAN_message_tx_success = CAN_tx_success;  // seems not to work

    module_IR_message_rx = IR_rx;

    module_start(setup, loop);

    return 0;
}
