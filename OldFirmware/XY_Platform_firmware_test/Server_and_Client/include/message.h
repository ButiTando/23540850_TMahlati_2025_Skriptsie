/// \file message.h
/// \brief This module defines the message used for communication between the 
///		PC and RaPi.
///
/// \details Message definition
///
/// \author Rijan de Nysschen, Kevin Gema
/// \date 04-Mar-2014
/// \version $Id$
//-------------------------------------------------------------------------------------------------

#ifndef _MESSAGE_H_
#define _MESSAGE_H_

#ifdef __cplusplus
extern "C"
{
#endif

//-------------------------------------------------------------------------------------------------
// Includes
#include "types.h"

//-------------------------------------------------------------------------------------------------
// Defines
#define MSG_SYNC_BYTE           (0xA5)   ///< Sync byte
#define MSG_MAX_PACKET_LENGTH   (10)     ///< Max packet data length

//-------------------------------------------------------------------------------------------------
/// \enum MSG_Types Message types (between PC and EGSE)
typedef enum _MSG_TYPES
{
	TCMD,
	TLM,
	TCMDRESP,
	TLMRESP,
    GUI
} MSG_TYPES;

/// \struct MSG_Packet Message packet definition (between PC and microcontroller)
typedef struct _MSG_PACKET {
	U8 syncByte;						///< Message sync byte
	U8 type;							///< Message type
	U8 id;								///< Message type
	U8 length;				            ///< Message length
	U8 data[MSG_MAX_PACKET_LENGTH];     ///< Message data
	U8 checksum;				        ///< Message checksum
} MSG_PACKET;

/// \union RAPI_MSG Message structure definition (between RaPi and PC)
typedef union _MSG_RAPI
{
	MSG_PACKET msgPacket;
	U8 rawdata[sizeof(MSG_PACKET)];
} MSG_RAPI;

/// \brief Calculates the schecksum of incoming messages
/// \param data Pointer to the incoming data array
/// \param offset Offset used to excluded header bytes
/// \param count The number of bytes used in the checksum
/// \return The calulated checksum
U8 MSG_CalculateChecksum(U8* data, int offset, int count);

#ifdef __cplusplus
}
#endif

#endif // _MESSAGE_H_
