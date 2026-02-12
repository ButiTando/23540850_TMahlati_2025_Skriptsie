# XY Platform

## TCP Communication

This is just bit and pieces needed to build the client need to communicate with the XY platform.

### 1 Message check sum calculation

```c
    // Function Definitions
    /// \brief Calculates the schecksum of incoming messages
    /// \param data Pointer to the incoming data array
    /// \param offset Offset used to excluded header bytes
    /// \param count The number of bytes used in the checksum
    /// \return The calulated checksum
    U8 MSG_CalculateChecksum(U8* data, int offset, int count)
    {
        int i;
        U8 checksum = 0;

        for (i = offset; i < count; i++)
        {
            checksum += (U8)data[i];
        }
        return checksum;
    }
```

### 2 Message format

```c

/// \struct MSG_Packet Message packet definition (between PC and microcontroller)
/*
@brief: This is used to format messages between the control station and XY platform.
@param syncByte DKW 
@param TCMD Telemetry command
@param TLM     : Telemetry message
@param TCMDRESP: Telemetry command response
@param TLMREPS : Telemetry message response
@param GUI     : 

*/
typedef struct _MSG_PACKET {
 U8 syncByte; ///< Message sync byte
 U8 type; ///< Message type 
 U8 id; ///< Message type, used to determine which command to run.
 U8 length;                ///< Message length
 U8 data[MSG_MAX_PACKET_LENGTH];     ///< Message data
 U8 checksum;            ///< Message checksum
} MSG_PACKET;
```

### 3 Message Sync byte

`#define MSG_SYNC_BYTE           (0xA5)`

### Message Types

```c
    typedef enum _MSG_TYPES
    {
        TCMD,
        TLM,
        TCMDRESP,
        TLMRESP,
        GUI
    } MSG_TYPES
```

### 4 Telecommands

```c
    typedef enum _TELECOMMANDS
    {
        ToggleIo = 0x01,
        SetIoMode,
        PulseIo,
        MotorStep,
        RaisePlatform,
        RotatePlatform
    } TELECOMMANDS;
```

### 5 TCMD Telecommand structure

As seen in 4 there are 6 telecommand. In order for each command, there has to be an exact number of data bytes. This documents the Telecommand message type, its messages ID's, and the expected date and data len.

These are the message formats. All field are uint_8 types

#### Toggle GPIO pin

```c
/*
    @brief Command to toggle a pin
    @param syncByte: 0xA5
    @param type: TCMD
    @param id: ToggleIO
    @param len: 2
    @param data[10]:
    @param    data[0]: \\pin to be set
    @param    data[1]: \\pin state 1 or 0
    @param    data[2]:
    @param    data[3]:
    @param    data[4]:
    @param    data[5]:
    @param    data[6]:
    @param    data[7]:
    @param    data[8]:
    @param    data[9]: 
    @param checksum: Calculated check sum from control station.
*/
```

#### Set GPIO Mode

```c
/*
    @brief Used to set the GPIO mode of a pin
    @param syncByte 0xA5
    @param type TCMD
    @param id SetIoMode
    @param len 2
    @param data[10]
    @param    data[0] pin to be configured
    @param    data[1] pin mode
    @param    data[2] Not set
    @param    data[3] Not set
    @param    data[4] Not set
    @param    data[5] Not set
    @param    data[6] Not set
    @param    data[7] Not set
    @param    data[8] Not set
    @param    data[9] Not set
    @param checksum Calculated check sum from control station.
*/
```

#### Pulse IO

```c
/*
    @brief Used to pulse a particular pin. Turn GPIO Low, delay 10us, Turn GPIO High
    @param syncByte 0xA5
    @param type TCMD
    @param id PulseIO
    @param len 1
    @param data[10]
    @param    data[0] pin to be configured
    @param    data[1] Not set
    @param    data[2] Not set
    @param    data[3] Not set
    @param    data[4] Not set
    @param    data[5] Not set
    @param    data[6] Not set
    @param    data[7] Not set
    @param    data[8] Not set
    @param    data[9] Not set
    @param checksum Calculated check sum from control station.
*/
```

#### Motor step

The steps are a 32bit number. \
Moving away = 1\
Moving closer = 0

```c
/*
    @brief Step a motor.
    @param syncByte 0xA5
    @param type TCMD
    @param id MotorStep
    @param len 8
    @param data[10]
    @param    data[0] motor pin
    @param    data[1] dir
    @param    data[2] steps (most significant 8 bits)
    @param    data[3] steps (2nd most significan 8 bits)
    @param    data[4] steps (3rd most significan 8 bits)
    @param    data[5] steps (least significan 8 bits)
    @param    data[6] direction pin
    @param    data[7] enable pin
    @param    data[8] Not set
    @param    data[9] Not set
    @param checksum Calculated check sum from control station.
*/
```

#### Raise plaform

The steps are a 16bit number, little endien.\
Moving up = 1\
moving down = 0

```c
/*
    @brief Command used to raise platform.
    @param syncByte 0xA5
    @param type TCMD
    @param id RaisePlatform
    @param len 9
    @param data[10]
    @param    data[0] motor1 pin
    @param    data[1] motor2 pin
    @param    data[2] direction
    @param    data[3] steps (most significant 8 bits)
    @param    data[4] steps (least significan 8 bits)
    @param    data[5] motor1 direction pin
    @param    data[6] motor2 direction pin
    @param    data[7] motor1 direction pin
    @param    data[8] motor2 direction pin
    @param    data[9] Not set
    @param checksum Calculated check sum from control station.
*/
```

#### Rotate platform

```c
/*
    @brief Command to rotate platform
    @param syncByte 0xA5
    @param type TCMD
    @param id RotatePlatform
    @param len 6
    @param data[10]
    @param    data[0] motor pin
    @param    data[1] direction
    @param    data[2] steps (most significan bits)
    @param    data[3] steps (least significan bits)
    @param    data[4] direction pin
    @param    data[5] enable pin
    @param    data[6] Not set
    @param    data[7] Not set
    @param    data[8] Not set
    @param    data[9] Not set
    @param checksum Calculated check sum from control station.
*/
```

### 6 GUI

The assumption is that this is the command sent to the XY platform on start up.

Stop command

```c
/*
    @brief Command to stop operation
    @param syncByte 0xA5
    @param type GUI
    @param id 0
    @param len 0
    @param data[10]
    @param    data[0] Not set
    @param    data[1] Not set
    @param    data[2] Not set
    @param    data[3] Not set
    @param    data[4] Not set
    @param    data[5] Not set
    @param    data[6] Not set
    @param    data[7] Not set
    @param    data[8] Not set
    @param    data[9] Not set
    @param checksum Calculated check sum from control station.
*/
```

Set down and up defaults

```c
/*
    @brief Command to raise and drop platform
    @param syncByte 0xA5
    @param type GUI
    @param id 1
    @param len 2
    @param data[10]
    @param    data[0] down default
    @param    data[1] up default
    @param    data[2] Not set
    @param    data[3] Not set
    @param    data[4] Not set
    @param    data[5] Not set
    @param    data[6] Not set
    @param    data[7] Not set
    @param    data[8] Not set
    @param    data[9] Not set
    @param checksum Calculated check sum from control station.
*/
```

Set towards and away defaults

```c
/*
    @brief Command to move XY platform left and right.
    @param syncByte 0xA5
    @param type GUI
    @param id 1
    @param len 2
    @param data[10]
    @param    data[0] moving closer default
    @param    data[1] moving away default
    @param    data[2] Not set
    @param    data[3] Not set
    @param    data[4] Not set
    @param    data[5] Not set
    @param    data[6] Not set
    @param    data[7] Not set
    @param    data[8] Not set
    @param    data[9] Not set
    @param checksum Calculated check sum from control station.
*/
```

Set rotation defaults

```c
/*
    @brief Command to rotate platform.
    @param syncByte 0xA5
    @param type GUI
    @param id 1
    @param len 1
    @param data[10]
    @param    data[0] moving closer default.
    @param    data[1] Not set
    @param    data[2] Not set
    @param    data[3] Not set
    @param    data[4] Not set
    @param    data[5] Not set
    @param    data[6] Not set
    @param    data[7] Not set
    @param    data[8] Not set
    @param    data[9] Not set
    @param checksum Calculated check sum from control station.
*/
```

### From XY platform IThemba client

#### Stepper motor pins
These are the pins of the different stepper motors.

```c#
    public enum mStepPins
        {
            m1Step = 14, //rotational motor
            el1Step = 23,
            el2Step = 8,
            plStep = 16 //horizontal motor
        }

        public enum mDirPins
        {
            m1Dir = 15, //rotational motor
            el1Dir = 24,
            el2Dir = 7,
            plDir = 20 //horizontal motor
        
        }

        public enum mEnablePins
        {
            m1Enable = 18, //rotational motor
            el1Enable = 25,
            el2Enable = 12,
            plEnable = 21 //horizontal motor        
        }       
```

#### Check sum calculated from client side

```c#

    private byte CalculateChecksum(byte[] data, int offset = 0, int count = -1)
    {
        byte checksum = 0;

        if (count < 0) count = (data.Length - offset);
        for (int i = offset; i < count && i < data.Length; i++)
            checksum += data[i];
        return checksum;
    }

```


