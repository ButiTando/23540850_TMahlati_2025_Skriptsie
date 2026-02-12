/// \file telecommands.cpp
///
/// \brief This module defines the message used for communication between the 
///		PC and RaPi.
///
/// \details Message definition
///
/// \author Kevin Gema
/// \date 12-Feb-2016
/// \version $Id: $
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
// Includes
#include "message.h"

//-------------------------------------------------------------------------------------------------
// Function Definitions
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
