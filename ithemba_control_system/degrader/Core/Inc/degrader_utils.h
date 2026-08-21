/*
 * degrader_utils.h
 *
 *  Created on: Sep 10, 2025
 *      Author: tando
 */

#ifndef INC_DEGRADER_UTILS_H_
#define INC_DEGRADER_UTILS_H_

#include <stdio.h>
#include "tcp_utils.h"
#include "tcp_client_utils.h"

#define MAX_COMMAND_VALUE 0Xff
#define MIN_COMMAND_VALUE 0X0
#define MAX_RESPONSE_VALUE 0xffff
#define MIN_RESPONSE_VALUE 0X0
#define CHAR_SIZE_OF_RESPONSE 5
#define CHAR_SIZE_OF_COMMAND 3

typedef enum {
	im_processing_command = 0,
	im_awake = 1,
	something_wrong = 2,
	ready_for_command = 3

} degraderProcessStatus;

typedef enum {
	lens_off=0,
	lens_on,
	lens_moving, // limbo state (not on or off.)
}t_lensState;

typedef enum{
	drive_dc_motor,
	drive_stepper_motor,
	read_select_switch,
	read_on_or_off_switch,
	lens_state_ideal,
	action_completed
} lens_select_stateMachine_state;

typedef enum {
	to_in_beam_state,
	away_from_beam_state
} dcMotorDirection;

typedef enum {

	backwards = GPIO_PIN_RESET,
	forwards = GPIO_PIN_SET

} stepperDirection;

/**
 * @union command
 * @brief 8 bit number representing desired lens states.
 *
 * This union allows accessing the command as either:
 *  - A raw 8-bit value (`command`)
 *  - An individual 1-bit field for each lens status and platform network connectivity.
 *
 * Status encoding for all 1-bit fields:
 *   - 0 → OFF (not in beam path)
 *   - 1 → ON  (in beam path)
 *
 */
typedef union {
	uint8_t command;

	struct {
		uint8_t lens_2mm : 1;	// 2mm lens state
		uint8_t lens_3mm : 1;	// 3mmm lens
		uint8_t lens_6mm : 1;	// 6mm lens
		uint8_t lens_8mm : 1;	// 8mm lens
		uint8_t lens_10mm : 1;	// 10mm lens
		uint8_t lens_12mm : 1;	// 12mm lens
		uint8_t lens_30mm : 1;  // 30mm lens
		uint8_t probe_bit : 1;  // Check if degrader is online
	};
} command;

/**
 * @union response
 * @brief Represents a compact 16-bit response word for lens status reporting.
 *
 * This union allows accessing the response as either:
 *  - A raw 16-bit value (`reponse`)
 *  - Individual 2-bit fields for each lens status and completion flag
 *
 * Status encoding for all 2-bit fields:
 *   - 0 → OFF
 *   - 1 → ON
 *   - 2 → UPDATING
 *   - 3 → NOT CHANGED YET
 *
 */
typedef union{
	uint16_t response;
	struct{
		uint16_t lens_status_2mm : 2;
		uint16_t lens_status_3mm : 2;
		uint16_t lens_status_6mm : 2;
		uint16_t lens_status_8mm : 2;
		uint16_t lens_status_10mm : 2;
		uint16_t lens_status_12mm : 2;
		uint16_t lens_status_30mm : 2;
		uint16_t process_status : 2;
	};
} response;

typedef struct Switch{
	GPIO_TypeDef* port;
	uint16_t pin;
} limSwitch;

typedef struct Lens{
	uint8_t position; // position from DC motor.
	uint8_t isSelected;
	limSwitch on; // Lens in beam path.
	limSwitch off; // Lens not in beam path.
	limSwitch limSelectSwitch; // Limit switch triggered when gears are aligned.
} lens;

typedef struct LensState{
	lens selected;
	lens inBeam;
} lensState;

typedef struct DC_Motor{

	uint8_t isOn;

	struct {
		GPIO_TypeDef* port;
		uint16_t pin;
	} EN_Pin;

	struct {
		GPIO_TypeDef* port;
		uint16_t pin;
	} IN1_Pin;

	struct {
		GPIO_TypeDef* port;
		uint16_t pin;
	} IN2_Pin;

} dcMotor;

typedef struct Stepper_Motor{

	uint8_t isOn;
	uint8_t current_stepper_position;

	struct {
		GPIO_TypeDef* port;
		uint16_t pin;
	} Sleep_Pin;

	struct {
		GPIO_TypeDef* port;
		uint16_t pin;
	} Reset_Pin;

	struct {
		GPIO_TypeDef* port;
		uint16_t pin;
	} Enable_Pin;

	struct{
		GPIO_TypeDef* port;
		uint16_t pin;
	}  Step_Pin;

	struct {
		GPIO_TypeDef* port;
		uint16_t pin;
	} Dir_Pin;

	struct {
		GPIO_TypeDef* port;
		uint16_t pin;
	} MS1_Pin;

	struct {
		GPIO_TypeDef* port;
		uint16_t pin;
	} MS2_Pin;

	struct {
		GPIO_TypeDef* port;
		uint16_t pin;
	} MS3_Pin;

}stepper;

typedef struct Degrader_item{
	uint8_t num_lenses;
	int dc_motor_position;
	lens **lenses;
	stepper *stepper_motor;
	dcMotor *dc_motor;
	command desired_lenses_state;
	command current_lenses_state;
	response resp;
	int command_received;
}degraderItems;

typedef struct State_Machine_item{
	lens_select_stateMachine_state state;
	dcMotorDirection dc_direction;
	stepperDirection stepper_direction;
	dcMotor *DCMotor;
	stepper *StepperMotor;
	lens *focusLen;
	int *isSetup;
} lens_state_machine_item;

void dcMotor_init(dcMotor *dc_motor);
void stepper_init(stepper *stepperMotor);
void driveStepper(stepper *stepperMotor, stepperDirection direction);
void stopStepper(stepper *stepperMotor);
void driveDC_Motor(dcMotor *dc_motor, dcMotorDirection dc_motor_state);
void stopDC_Motor(dcMotor *dc_motor);
void degrader_init(degraderItems *item);
void setLens(lens *selected_len, lensState lens_state);
void lens_select_state_machine(lens_state_machine_item *stateitem);
void decode_command(degraderItems *degrader_item, tcp_options *tcpOpt);
void encode_response(char *char_resp, response resp);
void DEGRADER_PROCESS(tcp_options *tcpOpt, degraderItems *degrader_item);

uint8_t read_Switch(limSwitch *limitSwitch);


#endif /* INC_DEGRADER_UTILS_H_ */
