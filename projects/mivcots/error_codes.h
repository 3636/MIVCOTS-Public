#pragma once

#define SUCCESS 0


// Class - Mask: 0xFF000000
#define ERR_MIVCOTS				0x01000000
#define ERR_ANALYSIS_CHILD		0x02000000
#define ERR_DATABASE			0x04000000
#define ERR_GUI					0x08000000


// Function - Mask: 0x00FF0000
#define ERR_SETUP				0x00020000
#define ERR_LOOP				0x00040000
#define ERR_INIT				0x00010000


// Error - Mask: 0x0000FFFF
#define ERR_NULLPTR				0x00000002
#define ERR_OUTOFRANGE			0x00000004
#define ERR_ELEMENTEXISTS		0x00000008
#define ERR_EMPTYQUEUE			0x00000010
#define ERR_MUTEXERR			0x00000020
#define ERR_NOTFOUND			0x00000040
#define ERR_INITERR				0x00000080
#define ERR_INVALIDMSGFORMAT	0x00000100
#define ERR_VALUEERR			0x00000200
#define ERR_EMPTYCACHE			0x00000400
#define ERR_INVALID_UPDATECOUNT 0x00000800
#define ERR_ANALYSIS_DELAY		0x00001000
#define ERR_NON_INCREASING_TIME 0x00002000

//Database Errors
#define ERR_NODATAINDB			0x00000001