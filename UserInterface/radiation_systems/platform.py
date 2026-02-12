import socket 
from enum import Enum

MSG_MAX_PACKET_LENGTH = 10
TO_XY_PLATFORM_PORT = 8888
class MSG_TYPES(Enum):
    TCMD = 0
    TLM = 1
    TCMDRESP = 2
    TLMRESP = 3
    GUI = 4

class TELECOMMANDS(Enum):
    ToggleIo = 0x01
    SetIoMode = 0x02
    PulseIo = 0x03
    MotorStep = 0x04
    RaisePlatform = 0x05
    RotatePlatform = 0x06

class mStepPins(Enum):
    m1Step = 14  # rotational motor
    el1Step = 23
    el2Step = 8
    plStep = 16  # horizontal motor

class mDirPins(Enum):
    m1Dir = 15  # rotational motor
    el1Dir = 24
    el2Dir = 7
    plDir = 20  # horizontal motor

class mEnablePins(Enum):
    m1Enable = 18  # rotational motor
    el1Enable = 25
    el2Enable = 12
    plEnable = 21  # horizontal motor

# Platform object
class Platform():

    def calculate_checksum(self,packet, offset, count):
        checksum = 0
        for i in range(offset, offset + count):
            checksum = (checksum + packet[i]) % 256
        return checksum

    def build_packet(self, msg_type, cmd_id, data_list):
        length = len(data_list)
        if length > MSG_MAX_PACKET_LENGTH:
            raise ValueError("Data length exceeds maximum")
        
        packet = bytearray(4 + MSG_MAX_PACKET_LENGTH + 1)  # sync, type, id, len, data[10], checksum
        packet[0] = 0xA5
        packet[1] = msg_type
        packet[2] = cmd_id
        packet[3] = length
        
        for i in range(length):
            packet[4 + i] = data_list[i]
        
        # Checksum from index 1 to (3 + length) inclusive, i.e., range(1, 4 + length)
        checksum_count = 3 + length  # number of bytes: type + id + len + data[0..length-1]
        packet[4 + MSG_MAX_PACKET_LENGTH] = self.calculate_checksum(packet, 1, checksum_count)
        
        return packet

    def xyplatform_toggle_gpio(self, pin, state):
        data = [pin, state]
        return self.build_packet(MSG_TYPES.TCMD.value, TELECOMMANDS.ToggleIo.value, data)

    def xyplatform_set_gpio_mode(self, pin, mode):
        data = [pin, mode]
        return self.build_packet(MSG_TYPES.TCMD.value, TELECOMMANDS.SetIoMode.value, data)

    def xyplatform_pulse_io(self,pin):
        data = [pin]
        return self.build_packet(MSG_TYPES.TCMD.value, TELECOMMANDS.PulseIo.value, data)

    def xyplatform_motor_step(self, motor_pin, dir, steps, dir_pin, enable_pin):
        # steps is 32-bit, big-endian
        data = [
            motor_pin,
            dir,
            (steps >> 24) & 0xFF,
            (steps >> 16) & 0xFF,
            (steps >> 8) & 0xFF,
            steps & 0xFF,
            dir_pin,
            enable_pin
        ]
        return self.build_packet(MSG_TYPES.TCMD.value, TELECOMMANDS.MotorStep.value, data)

    def xyplatform_raise_platform(self,motor1_pin, motor2_pin, direction, steps, motor1_dir_pin, motor2_dir_pin, motor1_enable_pin, motor2_enable_pin):
        # steps is 16-bit, assuming big-endian (doc inconsistency, but following field descriptions)
        data = [
            motor1_pin,
            motor2_pin,
            direction,
            (steps >> 8) & 0xFF,  # MS
            steps & 0xFF,         # LS
            motor1_dir_pin,
            motor2_dir_pin,
            motor1_enable_pin,
            motor2_enable_pin
        ]
        return self.build_packet(MSG_TYPES.TCMD.value, TELECOMMANDS.RaisePlatform.value, data)

    def xyplatform_rotate_platform(self, motor_pin, direction, steps, dir_pin, enable_pin):
        # steps is 16-bit, big-endian
        data = [
            motor_pin,
            direction,
            (steps >> 8) & 0xFF,
            steps & 0xFF,
            dir_pin,
            enable_pin
        ]
        return self.build_packet(MSG_TYPES.TCMD.value, TELECOMMANDS.RotatePlatform.value, data)

    def xyplatform_stop_command(self):
        return self.build_packet(MSG_TYPES.GUI.value, 0, [])

    def xyplatform_set_up_down_defaults(self, down_default, up_default):
        data = [down_default, up_default]
        return self.build_packet(MSG_TYPES.GUI.value, 1, data)

    def xyplatform_set_move_defaults(self, closer_default, away_default):
        data = [closer_default, away_default]
        return self.build_packet(MSG_TYPES.GUI.value, 1, data)

    def xyplatform_set_rotation_default(self, rotation_default):
        data = [rotation_default]
        return self.build_packet(MSG_TYPES.GUI.value, 1, data)
    
    def send_new_pos(self, horizontal_pos, vertical_pos):
        print(f'New horizontal: {horizontal_pos} | New vertical: {vertical_pos}')

    def send_command():
        pass