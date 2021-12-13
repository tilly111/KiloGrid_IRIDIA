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

volatile uint8_t cell_x[4] = {0, 0, 0, 0};
volatile uint8_t cell_y[4] = {0, 0, 0, 0};
volatile uint8_t cell_role[4] = {0, 0, 0, 0};
volatile uint8_t cell_colour[4] = {0, 0, 0, 0};

volatile uint8_t cell_received_op[4] = {0, 0, 0, 0};


uint8_t send = 0;
uint8_t receive_cell = 10;
uint8_t send_success = 0;
uint32_t broadcast_timer = 0;

uint8_t received_com_range = 0;
uint8_t received_option = 0;
uint8_t received_x = 0;
uint8_t received_y = 0;

uint8_t com_range = 0;
uint8_t option = 0;
uint8_t my_x = 0;
uint8_t my_y = 0;

// grid which stores where to send msgs -> this data structure is to big 
uint8_t sending_grid[10][20][4];

CAN_message_t test_message;  // for structure of a can msg see communication/mcp2515.h
kilogrid_address_t test_dest;

uint8_t com_range_module;
uint8_t send_success_sum;
uint8_t my_x_module;
uint8_t my_y_module;
uint8_t msg_number_current = 0;
uint8_t msg_number = 20;

uint8_t send_flag;

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


cell_num_t cell_id[4] = {CELL_00, CELL_01, CELL_02, CELL_03};

IR_message_t IR_message_tx[4];
IR_message_t IR_msg_rx;

tracking_user_data_t tracking_data;


// TODO cell2module
// TODO module2cell



void IR_rx(IR_message_t *m, cell_num_t c, distance_measurement_t *d, uint8_t CRC_error) {
    if(!CRC_error && m->type == TRACKING){
        CAN_message_t* msg = next_CAN_message();
        if(msg != NULL) { // if the buffer is not full
            serialize_tracking_message(msg, c, m->data);
        }
    }else if(!CRC_error && m->type == 62){  // TODO 62 is hopefully free 
        received_option = m->data[0];
        received_com_range = m->data[1];
        received_x = m->data[2];
        received_y = m->data[3];
        msg_number_current = m->data[4];

        if(msg_number_current != msg_number){
        	// case new message
        	msg_number = msg_number_current;
        }else{
        	// message already seen -> discard 
        	return;
        }

        // reception for individual cells 
        if(received_y % 2 == 1 && received_x % 2 == 0){
            receive_cell = 0;   
        }else if(received_y % 2 == 1 && received_x % 2 == 1){
            receive_cell = 1;   
        }else if(received_y % 2 == 0 && received_x % 2 == 0){
            receive_cell = 2;    
        }else if(received_y % 2 == 0 && received_x % 2 == 1){
            receive_cell = 3;    
        }

        //send_success = 0;  // reset to broadcast the message again
        // message_repeat_timer = 0;
    }
}

void CAN_rx(CAN_message_t *m) { 
    if (m->data[0]== 55){  // set msg
        cell_received_op[0] = m->data[1];
        cell_received_op[1] = m->data[2];
        cell_received_op[2] = m->data[3];
        cell_received_op[3] = m->data[4];
        // receive_timer = m->data[5];
        for(uint8_t cell_it_cb = 0; cell_it_cb < 4; cell_it_cb++){
        	if(cell_received_op[cell_it_cb] != 0){
        		reset_timer[cell_it_cb] = 0;
        	}
        }
    }else {
    	cell_received_op[0] = 3;
    	cell_received_op[1] = 3;
    	cell_received_op[2] = 3;
    	cell_received_op[3] = 3;
    }
}

//void CAN_tx_success(){
//    send_success = 1;
//    return;
//}

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

    for (int i = 0; i < 4; ++i){
    	set_LED_with_brightness(cell_id[i], WHITE, HIGH);
    }

}


void loop(){
    
	// reset input broadcast message after 10 repeats TODO CHECK 
	for(i_it = 0; i_it < 4; i_it++){
		if(cell_received_op[i_it] != 0){
			reset_timer[i_it] = reset_timer[i_it] + 1;
			if(reset_timer[i_it] > MAX_RESET_TIMER){
				cell_received_op[i_it] = 0;
			}
		}	
	}
		

	for(int i = 0; i < 4; ++i){

        IR_message_tx[i].type = 1;
        IR_message_tx[i].data[0] = cell_x[i];
        IR_message_tx[i].data[1] = cell_y[i];
        IR_message_tx[i].data[2] = cell_role[i];
        IR_message_tx[i].data[3] = cell_colour[i];  // this should be the option .. 1,2,3 or 0 if wall?!?

        set_IR_message(&IR_message_tx[i], i);

        if (send_success == 1 && cell_x[i] == my_x && cell_y[i] == my_y){
    		set_LED_with_brightness(cell_id[i], YELLOW, HIGH);
        }else{
	        switch(cell_received_op[i]){
	            case 0:
	                // this is wall
	                set_LED_with_brightness(cell_id[i], WHITE, HIGH);
	                break;
	            case 1:
	                // option 1
	                set_LED_with_brightness(cell_id[i], RED, HIGH);
	                break;
	            case 2:
	                // option 2
	                set_LED_with_brightness(cell_id[i], GREEN, HIGH);
	                break;
	            case 3:
	                // option 3 
	                set_LED_with_brightness(cell_id[i], BLUE, HIGH);
	                break;
	            default:
	            	set_LED_with_brightness(cell_id[i], MAGENTA, HIGH);
	                break;
	        }
	    }

	    // send_success is used as flag for broadcasting/ forwarding the broadcast of a robot 
	    // thus it is set to 1 if msg transmission was successful and reseted 
	    // when we receive a new robot msg aka in IR_rx
        
        // received a msg and send 
        if (receive_cell == i){  // && message_repeat_timer < MAX_MESSAGE_REPEAT
			// some parameters we get from the robot 
			// TODO put this in IR_rx -> save for every cell and reset after sending .. thus each cell can send stuff at the same time 
			// and not only once per module !!!!!
		    com_range = received_com_range;
	        option = received_option;
	        my_x = received_x;
	        my_y = received_y;
        	
        	// reset sending_grid 
        	for(i_it = 0; i_it < 10; i_it++){
                for(k_it = 0; k_it < 20; k_it++){
                    for(l_it = 0; l_it < 4; l_it++){
                        sending_grid[i_it][k_it][l_it] = 0;
                    }
                }
            }

            
            // calculate receiving cells
            for(x_it = my_x - com_range; x_it <= my_x + com_range; x_it++){
                for(y_it = my_y - com_range; y_it <= my_y + com_range; y_it++){
                    // check borders
                    if(x_it >= 0 && x_it < 20 && y_it >= 0 && y_it < 40){
                        // check distance -> use L2/euclidean norm!
                        if(sqrt(pow(fabs(x_it-my_x),2) + pow(fabs(y_it-my_y),2)) < com_range){
                            // which cells do we have to address
                            if(y_it % 2 == 1 && x_it % 2 == 0){
                                sending_grid[(int)(x_it/2)][(int)(y_it/2)][0] = option;    
                            }else if(y_it % 2 == 1 && x_it % 2 == 1){
                                sending_grid[(int)(x_it/2)][(int)(y_it/2)][1] = option;    
                            }else if(y_it % 2 == 0 && x_it % 2 == 0){
                                sending_grid[(int)(x_it/2)][(int)(y_it/2)][2] = option;    
                            }else if(y_it % 2 == 0 && x_it % 2 == 1){
                                sending_grid[(int)(x_it/2)][(int)(y_it/2)][3] = option;    
                            }
                        }
                    }
                }
            }  
            
            // iterate through all cells - to check if we need to send a msg to this cell
            com_range_module = (uint8_t)(com_range/2 + 1);
            send_success_sum = 0;
            my_x_module = (uint8_t)(my_x/2);
            my_y_module = (uint8_t)(my_y/2);
            // TODO only loop through close by modules 
            for(x_it = my_x_module - com_range_module; x_it < my_x_module + com_range_module; x_it++){
                for(y_it = my_y_module - com_range_module; y_it < my_y_module + com_range_module; y_it++){
                	// check borders - modules
                	if(x_it >= 0 && x_it < 10 && y_it >= 0 && y_it < 20){
	                    send_flag = 0;

	                    init_CAN_message(&test_message);

	                    test_message.id = 55;  // dont know if id is important - maybe to check if msg arrived twice or so - max 65,535
	                    test_message.data[0] = 55; // maybe this is the msg type (must be larger than 25 to dont overwrite something and less than 64 see communication/CAN.h)
	                    for (cell_it = 0; cell_it < 4; cell_it++){
	                        test_message.data[cell_it + 1] = sending_grid[x_it][y_it][cell_it]; // set if broadcast to cell cell_it
	                        //send_flag = 1;
	                        if(sending_grid[x_it][y_it][cell_it] != 0){
	                            send_flag = 1;
	                        }
	                    }
	                    
	                    test_dest.type = ADDR_INDIVIDUAL; // see communication/kilogrid.h for further information
	                    test_dest.x = x_it;  // is the position of a module imo??
	                    test_dest.y = y_it;

	                    if(send_flag == 1){
	                        send_success_sum = send_success_sum + CAN_message_tx(&test_message, test_dest); // CAN_message_t *message, kilogrid_address_t dest 
	                        _delay_ms(10);  // TODO change back to 10
	                    }
	                }
                }
            } 

            // apperently the module cannot send itself a msg so we have to set the broad cast manualy
            cell_received_op[0] = option;
	        cell_received_op[1] = option;
	        cell_received_op[2] = option;
	        cell_received_op[3] = option;

	        // also init reset timer
	        reset_timer[0] = 0;
	        reset_timer[1] = 0;
	        reset_timer[2] = 0;
	        reset_timer[3] = 0;

	        // reset after sending a msg
	        receive_cell = 10;
	        
        }
	}
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
