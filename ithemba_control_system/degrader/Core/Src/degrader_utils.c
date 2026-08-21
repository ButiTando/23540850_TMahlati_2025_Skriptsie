/*
 * degrader_utils.c
 *
 *  Created on: Sep 10, 2025
 *      Author: tando
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include "main.h"
#include "degrader_utils.h"
#include "tcp_utils.h"
#include "tcp_client_utils.h"
#include "stm32f746xx.h"

// Variables
static int processing_command = 0;

static lens_state_machine_item lensStateMachineItem = {
		.DCMotor = NULL,
		.StepperMotor = NULL,
		.dc_direction = -1,
		.stepper_direction = -1,
		.focusLen = NULL,
		.state = lens_state_ideal
};

// Private prototype functions
static void step_lense_state_machine(degraderItems *degrader_item, lens_state_machine_item *lensStateMachineItem);
static void get_current_state(degraderItems *degrader_item);

void degrader_init(degraderItems *item){
//	Initialise all motors
	dcMotor_init(item->dc_motor);
	stepper_init(item->stepper_motor);
//	Get current DC motor position
//	Read each select switch on the platform to determine the position of the dc motor.
	for(int i = 0; i < item->num_lenses; i++){
		int select_switch_value = read_Switch(&item->lenses[i]->limSelectSwitch);
		if(select_switch_value == GPIO_PIN_SET){
			item->dc_motor_position = item->lenses[i]->position;
			break;
		}

	}
	__NOP();
//	If DC motor position could not be found send the DC motor to the home position (2mm lens).
	if(item->dc_motor_position == -1){
		item->resp.process_status = im_processing_command;
		driveStepper(item->stepper_motor, backwards);

		while(read_Switch(&item->lenses[0]->limSelectSwitch) != GPIO_PIN_SET){
			__NOP(); // Do nothing until the lim switch is set.
		}

		stopStepper(item->stepper_motor);
		item->dc_motor_position = item->lenses[0]->position;
		item->resp.process_status = ready_for_command;
	}


//	Get current state of each lens.
	item->current_lenses_state.lens_2mm = read_Switch(&item->lenses[0]->on);
	item->current_lenses_state.lens_3mm = read_Switch(&item->lenses[1]->on);
	item->current_lenses_state.lens_6mm = read_Switch(&item->lenses[2]->on);
	item->current_lenses_state.lens_8mm = read_Switch(&item->lenses[3]->on);
	item->current_lenses_state.lens_10mm = read_Switch(&item->lenses[4]->on);
	item->current_lenses_state.lens_12mm = read_Switch(&item->lenses[5]->on);
	item->current_lenses_state.lens_30mm = read_Switch(&item->lenses[6]->on);

//	Set response
	item->resp.lens_status_2mm = item->current_lenses_state.lens_2mm;
	item->resp.lens_status_3mm = item->current_lenses_state.lens_3mm;
	item->resp.lens_status_6mm = item->current_lenses_state.lens_6mm;
	item->resp.lens_status_8mm = item->current_lenses_state.lens_8mm;
	item->resp.lens_status_10mm = item->current_lenses_state.lens_10mm;
	item->resp.lens_status_12mm = item->current_lenses_state.lens_12mm;
	item->resp.lens_status_30mm = item->current_lenses_state.lens_30mm;

//	Set the desired state to the current state
	item->desired_lenses_state.command = item->current_lenses_state.command;
	item->resp.response = item->desired_lenses_state.command;

//	Initialise lens state machine object
	lensStateMachineItem.DCMotor = item->dc_motor;
	lensStateMachineItem.StepperMotor = item->stepper_motor;

}

void driveDC_Motor(dcMotor *dc_motor, dcMotorDirection dc_motor_direction){

	// Looking at the back of the stepper motor.
	if (dc_motor_direction == away_from_beam_state){
		HAL_GPIO_WritePin(dc_motor->EN_Pin.port, dc_motor->EN_Pin.pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(dc_motor->IN1_Pin.port, dc_motor->IN1_Pin.pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(dc_motor->IN2_Pin.port, dc_motor->IN2_Pin.pin, GPIO_PIN_SET);
	}

	else if (dc_motor_direction == to_in_beam_state){
		HAL_GPIO_WritePin(dc_motor->EN_Pin.port, dc_motor->EN_Pin.pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(dc_motor->IN1_Pin.port, dc_motor->IN1_Pin.pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(dc_motor->IN2_Pin.port, dc_motor->IN2_Pin.pin, GPIO_PIN_RESET);
	}
}

void stopDC_Motor(dcMotor *dc_motor){
	HAL_GPIO_WritePin(dc_motor->EN_Pin.port, dc_motor->EN_Pin.pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(dc_motor->IN1_Pin.port, dc_motor->IN2_Pin.pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(dc_motor->IN2_Pin.port, dc_motor->IN1_Pin.pin, GPIO_PIN_RESET);
}

void dcMotor_init(dcMotor *dc_motor){
	stopDC_Motor(dc_motor);
}

void stepper_init(stepper *stepperMotor){

//	Put the stepper to sleep.
	stopStepper(stepperMotor);

//	Configure step resolution
//	Full step select
	HAL_GPIO_WritePin(stepperMotor->MS1_Pin.port, stepperMotor->MS1_Pin.pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(stepperMotor->MS2_Pin.port, stepperMotor->MS2_Pin.pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(stepperMotor->MS3_Pin.port, stepperMotor->MS3_Pin.pin, GPIO_PIN_RESET);

//	Set reset pin high to keep the driver on
	HAL_GPIO_WritePin(stepperMotor->Reset_Pin.port, stepperMotor->Reset_Pin.pin, GPIO_PIN_SET);
//	Set direction to forward
	HAL_GPIO_WritePin(stepperMotor->Dir_Pin.port, stepperMotor->Dir_Pin.pin, forwards);
//	No control over the enable pin, its already connected to ground. :(


}

void driveStepper(stepper *stepper_motor, stepperDirection direction){
//	Start the PWM signal to drive the stepper
//  Set direction
	HAL_GPIO_WritePin(stepper_motor->Dir_Pin.port, stepper_motor->Dir_Pin.pin, direction);

//	Awake stepper driver
	HAL_GPIO_WritePin(stepper_motor->Sleep_Pin.port, stepper_motor->Sleep_Pin.pin, GPIO_PIN_SET);
}

void stopStepper(stepper *stepper_motor){
	HAL_GPIO_WritePin(stepper_motor->Sleep_Pin.port, stepper_motor->Sleep_Pin.pin, GPIO_PIN_RESET);
}

void setLens(lens *selected_len, lensState lens_state){
	__NOP();
}

uint8_t read_Switch(limSwitch *limitSwitch){
//	The pins are active low
	return HAL_GPIO_ReadPin(limitSwitch->port, limitSwitch->pin);
}

void lens_select_state_machine(lens_state_machine_item *state_item){
	__NOP();
	switch(state_item->state){

		case lens_state_ideal:
			__NOP();
			if(state_item->focusLen != NULL){
				state_item->state = drive_stepper_motor;
			}

			break;

		case drive_stepper_motor:
			driveStepper(state_item->StepperMotor, state_item->stepper_direction);
			state_item->state = read_select_switch;
			break;

		case read_select_switch:
			int switch_value = read_Switch(&state_item->focusLen->limSelectSwitch); // active high
			if(switch_value){
				int small_delay_t1 = HAL_GetTick();
				while( (HAL_GetTick()-small_delay_t1)<5){
					__NOP();
				}
				stopStepper(state_item->StepperMotor);
				state_item->state = drive_dc_motor;
			}

			break;

		case drive_dc_motor:
			uint32_t small_delay = HAL_GetTick();
			driveDC_Motor(state_item->DCMotor, state_item->dc_direction);
			while(HAL_GetTick()-small_delay < 5){
				__NOP();
			}
			state_item->state = read_on_or_off_switch;

			break;

		case read_on_or_off_switch:
			if(state_item->dc_direction == to_in_beam_state){
				int switch_state = read_Switch(&state_item->focusLen->on);
				if(switch_state){
					stopDC_Motor(state_item->DCMotor);
					state_item->state = action_completed;
				}
			}

			else if(state_item->dc_direction == away_from_beam_state){
				int switchValue = read_Switch(&state_item->focusLen->off);
				if(switchValue){
					stopDC_Motor(state_item->DCMotor);
					state_item->state = action_completed;
				}
			}

			else{
//				Something went wrong in the state machine.
				stopDC_Motor(state_item->DCMotor);
				stopStepper(state_item->StepperMotor);
				state_item->state = lens_state_ideal;
			}

			break;

		case action_completed:
			state_item->state = lens_state_ideal;
			state_item->focusLen = NULL;
			state_item->isSetup = 0;
			break;

		default:
			stopDC_Motor(state_item->DCMotor);
			stopStepper(state_item->StepperMotor);
			state_item->state = lens_state_ideal;
			break;

	}

}

// Convert new string command into command type.
void decode_command(degraderItems *degrader_item, tcp_options *tcpOpt){

//	Get the string command from tcpOpt
//	Assuming the command is correct.
	char commandStr[2]="";
	snprintf(commandStr,strlen(tcpOpt->received_command),"%s",tcpOpt->received_command);
	int command = -1;
	command = atoi(tcpOpt->received_command);
//	validate command
	if(command >= MIN_COMMAND_VALUE && command <= MAX_COMMAND_VALUE){
		degrader_item->desired_lenses_state.command = command;
		tcpOpt->command_received_flag = 0;
		degrader_item->command_received = 1;
	}
	else{
		tcpOpt->command_received_flag = 0;
	}
	__NOP();

}

void encode_response(char *char_resp, response resp){
//	A 16-bit number has 5 characters
//	A response string has 5 characters + null terminator
	int response_len = 6;
	char_resp[response_len-1] = '\0';
	snprintf(char_resp,response_len,"%d", resp.response);

}

static void step_lense_state_machine(degraderItems *degrader_item, lens_state_machine_item *lensStateMachineItem){
//	Check if current state and desired state are equal
	__NOP();

	if(degrader_item->desired_lenses_state.probe_bit != 0){
		degrader_item->resp.process_status = im_awake;
		degrader_item->desired_lenses_state.probe_bit = 0;
		processing_command = 0;
		degrader_item->resp.process_status = ready_for_command;
		degrader_item->desired_lenses_state = degrader_item->current_lenses_state;
	}

	if((degrader_item->current_lenses_state.command != degrader_item->desired_lenses_state.command && degrader_item->command_received == 1)){
		processing_command = 1;
		degrader_item->resp.process_status = im_processing_command;

		if(degrader_item->current_lenses_state.lens_2mm != degrader_item->desired_lenses_state.lens_2mm){
			static int isSetup = 0;
			degrader_item->resp.lens_status_2mm = lens_moving;
			if(!isSetup){
	//			focus on 2mm lens
				lensStateMachineItem->focusLen = degrader_item->lenses[0];
	//			set direction of stepper motor
				if(degrader_item->dc_motor_position != degrader_item->lenses[0]->position){
					lensStateMachineItem->stepper_direction = (degrader_item->dc_motor_position < degrader_item->lenses[0]->position) ? forwards : backwards;
					lensStateMachineItem->dc_direction = (degrader_item->desired_lenses_state.lens_2mm == 1) ? to_in_beam_state: away_from_beam_state;
				}
				else{
					lensStateMachineItem->state = drive_dc_motor;
					lensStateMachineItem->dc_direction = (degrader_item->desired_lenses_state.lens_2mm == 1) ? to_in_beam_state: away_from_beam_state;
				}
				isSetup = 1;
			}

//			Check if the lens has been set
			if(lensStateMachineItem->state == action_completed){
				isSetup = 0;
				degrader_item->dc_motor_position = degrader_item->lenses[0]->position;
				get_current_state(degrader_item);
				degrader_item->resp.lens_status_2mm = degrader_item->current_lenses_state.lens_2mm;
				if(degrader_item->current_lenses_state.command == degrader_item->desired_lenses_state.command){
					degrader_item->command_received = 0;
				}

			}

			lens_select_state_machine(lensStateMachineItem);
		}

		else if(degrader_item->current_lenses_state.lens_3mm != degrader_item->desired_lenses_state.lens_3mm){
			static int isSetup = 0;
			degrader_item->resp.lens_status_3mm = lens_moving;
			if(!isSetup){
	//			focus on 3mm lens
				lensStateMachineItem->focusLen = degrader_item->lenses[1];
	//			set direction of stepper motor
				if(degrader_item->dc_motor_position != degrader_item->lenses[1]->position){
					lensStateMachineItem->stepper_direction = (degrader_item->dc_motor_position < degrader_item->lenses[1]->position) ? forwards : backwards;
					lensStateMachineItem->dc_direction = (degrader_item->desired_lenses_state.lens_3mm == 1) ? to_in_beam_state: away_from_beam_state;
				}
				else{
					lensStateMachineItem->state = drive_dc_motor;
					lensStateMachineItem->dc_direction = (degrader_item->desired_lenses_state.lens_3mm == 1) ? to_in_beam_state: away_from_beam_state;
				}

				isSetup = 1;
			}

//			Check if the lens has been set
			if(lensStateMachineItem->state == action_completed){
				isSetup = 0;
				degrader_item->dc_motor_position = degrader_item->lenses[1]->position;
				get_current_state(degrader_item);
				degrader_item->resp.lens_status_3mm = degrader_item->current_lenses_state.lens_3mm;
				if(degrader_item->current_lenses_state.command == degrader_item->desired_lenses_state.command){
					degrader_item->command_received = 0;
				}
			}

			lens_select_state_machine(lensStateMachineItem);
		}

		else if(degrader_item->current_lenses_state.lens_6mm != degrader_item->desired_lenses_state.lens_6mm){
			static int isSetup = 0;
			degrader_item->resp.lens_status_6mm = lens_moving;
			if(!isSetup){
	//			focus on 6mm lens
				lensStateMachineItem->focusLen = degrader_item->lenses[2];
	//			set direction of stepper motor
				if(degrader_item->dc_motor_position != degrader_item->lenses[2]->position){
					lensStateMachineItem->stepper_direction = (degrader_item->dc_motor_position < degrader_item->lenses[2]->position) ? forwards : backwards;
					lensStateMachineItem->dc_direction = (degrader_item->desired_lenses_state.lens_6mm == 1) ? to_in_beam_state: away_from_beam_state;
				}
				else{
					lensStateMachineItem->state = drive_dc_motor;
					lensStateMachineItem->dc_direction = (degrader_item->desired_lenses_state.lens_6mm == 1) ? to_in_beam_state: away_from_beam_state;
				}

				isSetup = 1;
			}

//			Check if the lens has been set
			if(lensStateMachineItem->state == action_completed){
				isSetup = 0;
				degrader_item->dc_motor_position = degrader_item->lenses[2]->position;
				get_current_state(degrader_item);
				degrader_item->resp.lens_status_6mm = degrader_item->current_lenses_state.lens_6mm;
				if(degrader_item->current_lenses_state.command == degrader_item->desired_lenses_state.command){
					degrader_item->command_received = 0;
				}
			}

			lens_select_state_machine(lensStateMachineItem);
		}


		else if(degrader_item->current_lenses_state.lens_8mm != degrader_item->desired_lenses_state.lens_8mm){
			static int isSetup = 0;
			degrader_item->resp.lens_status_8mm = lens_moving;
			if(!isSetup){
	//			focus on 8mm lens
				lensStateMachineItem->focusLen = degrader_item->lenses[3];
	//			set direction of stepper motor
				if(degrader_item->dc_motor_position != degrader_item->lenses[3]->position){
					lensStateMachineItem->stepper_direction = (degrader_item->dc_motor_position < degrader_item->lenses[3]->position) ? forwards : backwards;
					lensStateMachineItem->dc_direction = (degrader_item->desired_lenses_state.lens_8mm == 1) ? to_in_beam_state: away_from_beam_state;
				}
				else{
					lensStateMachineItem->state = drive_dc_motor;
					lensStateMachineItem->dc_direction = (degrader_item->desired_lenses_state.lens_8mm == 1) ? to_in_beam_state: away_from_beam_state;
				}

				isSetup = 1;
			}

//			Check if the lens has been set
			if(lensStateMachineItem->state == action_completed){
				isSetup = 0;
				degrader_item->dc_motor_position = degrader_item->lenses[3]->position;
				get_current_state(degrader_item);
				degrader_item->resp.lens_status_8mm = degrader_item->current_lenses_state.lens_8mm;
				if(degrader_item->current_lenses_state.command == degrader_item->desired_lenses_state.command){
					degrader_item->command_received = 0;
				}
			}

			lens_select_state_machine(lensStateMachineItem);
		}

		else if(degrader_item->current_lenses_state.lens_10mm != degrader_item->desired_lenses_state.lens_10mm){
			static int isSetup = 0;
			degrader_item->resp.lens_status_10mm = lens_moving;
			if(!isSetup){
	//			focus on 10mm lens
				lensStateMachineItem->focusLen = degrader_item->lenses[4];
	//			set direction of stepper motor
				if(degrader_item->dc_motor_position != degrader_item->lenses[4]->position){
					lensStateMachineItem->stepper_direction = (degrader_item->dc_motor_position < degrader_item->lenses[4]->position) ? forwards : backwards;
					lensStateMachineItem->dc_direction = (degrader_item->desired_lenses_state.lens_10mm == 1) ? to_in_beam_state: away_from_beam_state;
				}
				else{
					lensStateMachineItem->state = drive_dc_motor;
					lensStateMachineItem->dc_direction = (degrader_item->desired_lenses_state.lens_10mm == 1) ? to_in_beam_state: away_from_beam_state;
				}

				isSetup = 1;
			}

//			Check if the lens has been set
			if(lensStateMachineItem->state == action_completed){
				isSetup = 0;
				degrader_item->dc_motor_position = degrader_item->lenses[4]->position;
				get_current_state(degrader_item);
				degrader_item->resp.lens_status_10mm = degrader_item->current_lenses_state.lens_10mm;
				if(degrader_item->current_lenses_state.command == degrader_item->desired_lenses_state.command){
					degrader_item->command_received = 0;
				}
			}

			lens_select_state_machine(lensStateMachineItem);
		}

		else if(degrader_item->current_lenses_state.lens_12mm != degrader_item->desired_lenses_state.lens_12mm){
			static int isSetup = 0;
			degrader_item->resp.lens_status_12mm = lens_moving;
			if(!isSetup){
	//			focus on 12mm lens
				lensStateMachineItem->focusLen = degrader_item->lenses[5];
	//			set direction of stepper motor
				if(degrader_item->dc_motor_position != degrader_item->lenses[5]->position){
					lensStateMachineItem->stepper_direction = (degrader_item->dc_motor_position < degrader_item->lenses[5]->position) ? forwards : backwards;
					lensStateMachineItem->dc_direction = (degrader_item->desired_lenses_state.lens_12mm == 1) ? to_in_beam_state: away_from_beam_state;
				}
				else{
					lensStateMachineItem->state = drive_dc_motor;
					lensStateMachineItem->dc_direction = (degrader_item->desired_lenses_state.lens_12mm == 1) ? to_in_beam_state: away_from_beam_state;
				}

				isSetup = 1;
			}

//			Check if the lens has been set
			if(lensStateMachineItem->state == action_completed){
				isSetup = 0;
				degrader_item->dc_motor_position = degrader_item->lenses[5]->position;
				get_current_state(degrader_item);
				degrader_item->resp.lens_status_12mm = degrader_item->current_lenses_state.lens_12mm;
				if(degrader_item->current_lenses_state.command == degrader_item->desired_lenses_state.command){
					degrader_item->command_received = 0;
				}
			}

			lens_select_state_machine(lensStateMachineItem);
		}

		else if(degrader_item->current_lenses_state.lens_30mm != degrader_item->desired_lenses_state.lens_30mm){
			static int isSetup = 0;
			degrader_item->resp.lens_status_30mm = lens_moving;
			if(!isSetup){
	//			focus on 30mm lens
				lensStateMachineItem->focusLen = degrader_item->lenses[6];
	//			set direction of stepper motor
				if(degrader_item->dc_motor_position != degrader_item->lenses[6]->position){
					lensStateMachineItem->stepper_direction = (degrader_item->dc_motor_position < degrader_item->lenses[6]->position) ? forwards : backwards;
					lensStateMachineItem->dc_direction = (degrader_item->desired_lenses_state.lens_30mm == 1) ? to_in_beam_state: away_from_beam_state;
				}
				else{
					lensStateMachineItem->state = drive_dc_motor;
					lensStateMachineItem->dc_direction = (degrader_item->desired_lenses_state.lens_30mm == 1) ? to_in_beam_state: away_from_beam_state;
				}

				isSetup = 1;
			}

//			Check if the lens has been set
			if(lensStateMachineItem->state == action_completed){
				isSetup = 0;
				degrader_item->dc_motor_position = degrader_item->lenses[6]->position;
				get_current_state(degrader_item);
				degrader_item->resp.lens_status_30mm = degrader_item->current_lenses_state.lens_30mm;
				if(degrader_item->current_lenses_state.command == degrader_item->desired_lenses_state.command){
					degrader_item->command_received = 0;
				}
			}

			lens_select_state_machine(lensStateMachineItem);
		}
	}

	else{
		processing_command = 0;
		degrader_item->resp.process_status = ready_for_command;
//		Todo: send done processing to server
	}

}

void DEGRADER_PROCESS(tcp_options *tcpOpt, degraderItems *degrader_item){

//	Check if a new command was received.
	if(tcpOpt->command_received_flag == 1){
//		check if still processing a previous command
		__NOP();
		if(processing_command==1){
//			Send response with busy flag
			degrader_item->resp.process_status = im_processing_command;
		}
		else{
			decode_command(degrader_item, tcpOpt);
		}
	}

	step_lense_state_machine(degrader_item, &lensStateMachineItem);
}

static void get_current_state(degraderItems *degrader_item){
	//	Get current state of each lens.
	degrader_item->current_lenses_state.lens_2mm = read_Switch(&degrader_item->lenses[0]->on);
	degrader_item->current_lenses_state.lens_3mm = read_Switch(&degrader_item->lenses[1]->on);
	degrader_item->current_lenses_state.lens_6mm = read_Switch(&degrader_item->lenses[2]->on);
	degrader_item->current_lenses_state.lens_8mm = read_Switch(&degrader_item->lenses[3]->on);
	degrader_item->current_lenses_state.lens_10mm = read_Switch(&degrader_item->lenses[4]->on);
	degrader_item->current_lenses_state.lens_12mm = read_Switch(&degrader_item->lenses[5]->on);
	degrader_item->current_lenses_state.lens_30mm = read_Switch(&degrader_item->lenses[6]->on);
}








































