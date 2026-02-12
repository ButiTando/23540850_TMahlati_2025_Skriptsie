/**************************************************************************
*   Copyright (C) 2005 by Achal Dhir                                      *
*   achaldhir@gmail.com                                                   *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/
//This file defines all structures and constants
//for both DHCP and DNS Servers
#define MAX_SERVERS 125
#define MAX_DHCP_RANGES 125
#define MAX_DNS_RANGES 32
#define MAX_RANGE_SETS 125
#define MAX_RANGE_FILTERS 32
#define MAX_COND_FORW 125
#define MAX_TCP_CLIENTS 16
#define MAX_WILD_HOSTS 256

#ifndef LOG_MAKEPRI
#define	LOG_MAKEPRI(fac, pri)	(((fac) << 3) | (pri))
#endif

#ifndef SIOCGIFCONF
#include <sys/sockio.h>
#endif

#ifndef _UIO_H_
#include <sys/uio.h>
#endif

/*
//#ifndef _LINUX_IN_H
//#ifndef _NETINET_IN_H
struct in_pktinfo
{
	unsigned int   ipi_ifindex;  // Interface index
	struct in_addr ipi_spec_dst; // Local address
	struct in_addr ipi_addr;     // Header Destination address
};
typedef struct in_pktinfo IN_PKTINFO;
//#endif
//#endif
*/

#ifndef INADDR_NONE
#define INADDR_NONE UINT_MAX
#endif

#ifndef IFF_DYNAMIC
#define IFF_DYNAMIC 0x8000
#endif

#define MYWORD unsigned short
#define MYBYTE unsigned char
#define MYDWORD unsigned int
#define SOCKET_ERROR -1
#define INVALID_SOCKET -1
#define SOCKADDR_IN sockaddr_in
#define SOCKADDR sockaddr
#define SOCKET int
#define BOOL bool
#define LPSOCKADDR sockaddr*
#define closesocket close

enum
{
	CTYPE_NONE,
	CTYPE_DHCP_ENTRY,
	CTYPE_LOCAL_A,
	CTYPE_LOCAL_PTR_AUTH,
	CTYPE_LOCAL_PTR_NAUTH,
	CTYPE_LOCALHOST_A,
	CTYPE_LOCALHOST_PTR,
	CTYPE_SERVER_A_AUTH,
	CTYPE_SERVER_PTR_AUTH,
	CTYPE_SERVER_A_NAUTH,
	CTYPE_SERVER_PTR_NAUTH,
	CTYPE_LOCAL_CNAME,
	CTYPE_EXT_CNAME,
	CTYPE_STATIC_A_AUTH,
	CTYPE_STATIC_PTR_AUTH,
	CTYPE_STATIC_A_NAUTH,
	CTYPE_STATIC_PTR_NAUTH,
	CTYPE_NS,
	CTYPE_SOA,
	CTYPE_AXFR,
	CTYPE_CACHED,
	CTYPE_NON_CACHED,
	CTYPE_QUEUE,
	CTYPE_DNS_CHECK,
	QTYPE_IP,
	QTYPE_HOSTNAME,
	QTYPE_A_EXT,
	QTYPE_A_BARE,
	QTYPE_A_LOCAL,
	QTYPE_A_ZONE,
	QTYPE_P_EXT,
	QTYPE_P_LOCAL,
	QTYPE_P_ZONE,
	QTYPE_A_SUBZONE,
	QTYPE_P_SUBZONE,
	QTYPE_CHILDZONE
};

struct data10
{
	char zone[256];
	MYWORD zLen;
	MYDWORD dns[2];
	MYBYTE currentDNS;
	MYBYTE lastDNS;
};

struct data18
{
	MYBYTE currentInd;
	bool done;
};

struct data16
{
	char wildcard[256];
	MYDWORD ip;
};

struct data7 //cache
{
	char *mapname;
	time_t expiry;
	union
	{
		struct
		{
			MYBYTE cacheType;
			MYBYTE dnsType;
			MYBYTE sockInd;
			MYBYTE dnsIndex;
		};
		struct
		{
			unsigned fixed: 1;
			unsigned local: 1;
			unsigned display: 1;
			unsigned subnetFlg: 1;
			signed rangeInd: 8;
			unsigned dhcpInd: 20;
		};
	};
	union
	{
		char *name;
		MYBYTE *options;
	};
	union
	{
		int bytes;
		MYDWORD ip;
		SOCKADDR_IN *addr;
	};
	union
	{
		MYBYTE *response;
		char *hostname;
		char *query;
	};
	MYBYTE data;
};

struct data71 //Lump
{
	char *mapname;
	MYBYTE *response;
	char *hostname;
	char *query;
	SOCKADDR_IN *addr;
	MYBYTE *options;
	MYWORD optionSize;
	int bytes;
	MYBYTE cacheType;
	MYBYTE dnsType;
};

typedef std::map<string, data7*> dhcpMap;

struct data11
{
	char hostname[256];
	MYWORD pref;
};

struct ConnType
{
	SOCKET sock;
	SOCKADDR_IN addr;
	MYDWORD server;
	MYWORD port;
	bool loaded;
	bool ready;
};

#define BOOTP_REQUEST  1
#define BOOTP_REPLY    2

#define DHCP_MESS_NONE       0
#define DHCP_MESS_DISCOVER   1
#define DHCP_MESS_OFFER      2
#define DHCP_MESS_REQUEST	 3
#define DHCP_MESS_DECLINE	 4
#define DHCP_MESS_ACK		 5
#define DHCP_MESS_NAK		 6
#define DHCP_MESS_RELEASE    7
#define DHCP_MESS_INFORM	 8


// DHCP OPTIONS
#define DHCP_OPTION_PAD						0
#define DHCP_OPTION_NETMASK          		1
#define DHCP_OPTION_TIMEOFFSET       		2
#define DHCP_OPTION_ROUTER           		3
#define DHCP_OPTION_TIMESERVER       		4
#define DHCP_OPTION_NAMESERVER       		5
#define DHCP_OPTION_DNS              		6
#define DHCP_OPTION_LOGSERVER        		7
#define DHCP_OPTION_COOKIESERVER     		8
#define DHCP_OPTION_LPRSERVER        		9
#define DHCP_OPTION_IMPRESSSERVER    		10
#define DHCP_OPTION_RESLOCSERVER     		11
#define DHCP_OPTION_HOSTNAME         		12
#define DHCP_OPTION_BOOTFILESIZE     		13
#define DHCP_OPTION_MERITDUMP        		14
#define DHCP_OPTION_DOMAINNAME       		15
#define DHCP_OPTION_SWAPSERVER       		16
#define DHCP_OPTION_ROOTPATH         		17
#define DHCP_OPTION_EXTSPATH         		18
#define DHCP_OPTION_IPFORWARD        		19
#define DHCP_OPTION_NONLOCALSR       		20
#define DHCP_OPTION_POLICYFILTER     		21
#define DHCP_OPTION_MAXREASSEMBLE    		22
#define DHCP_OPTION_IPTTL            		23
#define DHCP_OPTION_PATHMTUAGING     		24
#define DHCP_OPTION_PATHMTUPLATEAU   		25
#define DHCP_OPTION_INTERFACEMTU     		26
#define DHCP_OPTION_SUBNETSLOCAL     		27
#define DHCP_OPTION_BCASTADDRESS     		28
#define DHCP_OPTION_MASKDISCOVERY    		29
#define DHCP_OPTION_MASKSUPPLIER     		30
#define DHCP_OPTION_ROUTERDISCOVERY  		31
#define DHCP_OPTION_ROUTERSOLIC      		32
#define DHCP_OPTION_STATICROUTE      		33
#define DHCP_OPTION_TRAILERENCAPS    		34
#define DHCP_OPTION_ARPTIMEOUT       		35
#define DHCP_OPTION_ETHERNETENCAPS   		36
#define DHCP_OPTION_TCPTTL           		37
#define DHCP_OPTION_TCPKEEPALIVEINT  		38
#define DHCP_OPTION_TCPKEEPALIVEGRBG 		39
#define DHCP_OPTION_NISDOMAIN        		40
#define DHCP_OPTION_NISSERVERS       		41
#define DHCP_OPTION_NTPSERVERS       		42
#define DHCP_OPTION_VENDORSPECIFIC   		43
#define DHCP_OPTION_NETBIOSNAMESERV  		44
#define DHCP_OPTION_NETBIOSDGDIST    		45
#define DHCP_OPTION_NETBIOSNODETYPE  		46
#define DHCP_OPTION_NETBIOSSCOPE     		47
#define DHCP_OPTION_X11FONTS         		48
#define DHCP_OPTION_X11DISPLAYMNGR   		49
#define DHCP_OPTION_REQUESTEDIPADDR  		50
#define DHCP_OPTION_IPADDRLEASE      		51
#define DHCP_OPTION_OVERLOAD         		52
#define DHCP_OPTION_MESSAGETYPE      		53
#define DHCP_OPTION_SERVERID         		54
#define DHCP_OPTION_PARAMREQLIST     		55
#define DHCP_OPTION_MESSAGE          		56
#define DHCP_OPTION_MAXDHCPMSGSIZE   		57
#define DHCP_OPTION_RENEWALTIME      		58
#define DHCP_OPTION_REBINDINGTIME    		59
#define DHCP_OPTION_VENDORCLASSID    		60
#define DHCP_OPTION_CLIENTID         		61
#define DHCP_OPTION_NETWARE_IPDOMAIN        62
#define DHCP_OPTION_NETWARE_IPOPTION        63
#define DHCP_OPTION_NISPLUSDOMAIN    		64
#define DHCP_OPTION_NISPLUSSERVERS   		65
#define DHCP_OPTION_TFTPSERVER       		66
#define DHCP_OPTION_BOOTFILE         		67
#define DHCP_OPTION_MOBILEIPHOME     		68
#define DHCP_OPTION_SMTPSERVER       		69
#define DHCP_OPTION_POP3SERVER       		70
#define DHCP_OPTION_NNTPSERVER       		71
#define DHCP_OPTION_WWWSERVER        		72
#define DHCP_OPTION_FINGERSERVER     		73
#define DHCP_OPTION_IRCSERVER        		74
#define DHCP_OPTION_STSERVER         		75
#define DHCP_OPTION_STDASERVER       		76
#define DHCP_OPTION_USERCLASS        		77
#define DHCP_OPTION_SLPDIRAGENT      		78
#define DHCP_OPTION_SLPDIRSCOPE      		79
#define DHCP_OPTION_CLIENTFQDN       		81
#define DHCP_OPTION_RELAYAGENTINFO     		82
#define DHCP_OPTION_I_SNS     				83
#define DHCP_OPTION_NDSSERVERS       		85
#define DHCP_OPTION_NDSTREENAME      		86
#define DHCP_OPTION_NDSCONTEXT		 		87
#define DHCP_OPTION_AUTHENTICATION			90
#define DHCP_OPTION_CLIENTSYSTEM			93
#define DHCP_OPTION_CLIENTNDI				94
#define DHCP_OPTION_LDAP					95
#define DHCP_OPTION_UUID_GUID				97
#define DHCP_OPTION_USER_AUTH				98
#define DHCP_OPTION_P_CODE					100
#define DHCP_OPTION_T_CODE					101
#define DHCP_OPTION_NETINFOADDRESS			112
#define DHCP_OPTION_NETINFOTAG				113
#define DHCP_OPTION_URL						114
#define DHCP_OPTION_AUTO_CONFIG				116
#define DHCP_OPTION_NAMESERVICESEARCH		117
#define DHCP_OPTION_SUBNETSELECTION			118
#define DHCP_OPTION_DOMAINSEARCH			119
#define DHCP_OPTION_SIPSERVERSDHCP			120
#define DHCP_OPTION_CLASSLESSSTATICROUTE	121
#define DHCP_OPTION_CCC						122
#define DHCP_OPTION_GEOCONF					123
#define DHCP_OPTION_V_IVENDORCLASS			124
#define DHCP_OPTION_V_IVENDOR_SPECIFIC		125
#define DHCP_OPTION_TFPTSERVERIPADDRESS		128
#define DHCP_OPTION_CALLSERVERIPADDRESS		129
#define DHCP_OPTION_DISCRIMINATIONSTRING	130
#define DHCP_OPTION_REMOTESTATISTICSSERVER	131
#define DHCP_OPTION_802_1PVLANID			132
#define DHCP_OPTION_802_1QL2PRIORITY		133
#define DHCP_OPTION_DIFFSERVCODEPOINT		134
#define DHCP_OPTION_HTTPPROXYFORPHONE_SPEC	135
#define DHCP_OPTION_SERIAL					252
#define DHCP_OPTION_BP_FILE					253
#define DHCP_OPTION_NEXTSERVER				254
#define DHCP_OPTION_END						255

//#define DHCP_VENDORDATA_SIZE		 272
//#define DHCP_VENDORDATA_SIZE		 64
//#define DHCP_VENDORDATA_SIZE		 784
//#define DHCP_PACKET_SIZE			1024
//#define DHCP_MIN_SIZE				 44
//#define DHCP_MAX_CLIENTS			 254
#define IPPORT_DHCPS   67
#define IPPORT_DHCPC   68
#define VM_STANFORD  0x5354414EUL
#define VM_RFC1048   0x63825363UL

struct data3
{
	MYBYTE opt_code;
	MYBYTE size;
	MYBYTE value[256];
};

struct data4
{
	char opName[40];
	MYBYTE opTag;
	MYBYTE opType;
	bool required;
};

struct data13 //dhcp range
{
	MYBYTE rangeSetInd;
	MYDWORD rangeStart;
	MYDWORD rangeEnd;
	MYDWORD mask;
	MYBYTE *options;
	time_t *expiry;
	data7 **dhcpEntry;
};

struct data14 //rangeSet
{
	MYBYTE active;
	MYBYTE *macStart[MAX_RANGE_FILTERS];
	MYBYTE *macEnd[MAX_RANGE_FILTERS];
	MYBYTE macSize[MAX_RANGE_FILTERS];
	MYBYTE *vendClass[MAX_RANGE_FILTERS];
	MYBYTE vendClassSize[MAX_RANGE_FILTERS];
	MYBYTE *userClass[MAX_RANGE_FILTERS];
	MYBYTE userClassSize[MAX_RANGE_FILTERS];
	MYDWORD subnetIP[MAX_RANGE_FILTERS];
	MYDWORD targetIP;
};

struct data17
{
	MYBYTE macArray[MAX_RANGE_SETS];
	MYBYTE vendArray[MAX_RANGE_SETS];
	MYBYTE userArray[MAX_RANGE_SETS];
	MYBYTE subnetArray[MAX_RANGE_SETS];
	bool macFound;
	bool vendFound;
	bool userFound;
	bool subnetFound;
};

struct data19
{
	SOCKET sock;
	SOCKADDR_IN remote;
	socklen_t sockLen;
	linger ling;
	int bytes;
	int memSize;
	char contentType[32];
	char *dp;
};

struct msg_control
{
	ulong cmsg_len;
	int cmsg_level;
	int cmsg_type;
	in_pktinfo pktinfo;
};

#if defined(__hppa__) || \
    defined(__m68k__) || defined(mc68000) || defined(_M_M68K) || \
    (defined(__MIPS__) && defined(__MISPEB__)) || \
    defined(__ppc__) || defined(__POWERPC__) || defined(_M_PPC) || \
    defined(__sparc__)

struct dhcp_header
{
	MYBYTE bp_op;
	MYBYTE bp_htype;
	MYBYTE bp_hlen;
	MYBYTE bp_hops;
	MYDWORD bp_xid;
	struct
	{
		unsigned bp_secs:16;
		unsigned bp_broadcast:1;
		unsigned bp_spare:7;
		unsigned bp_spare1:8;
	};
	MYDWORD bp_ciaddr;
	MYDWORD bp_yiaddr;
	MYDWORD bp_siaddr;
	MYDWORD bp_giaddr;
	MYBYTE bp_chaddr[16];
	char bp_sname[64];
	MYBYTE bp_file[128];
	MYBYTE bp_magic_num[4];
};

#else

struct dhcp_header
{
	MYBYTE bp_op;
	MYBYTE bp_htype;
	MYBYTE bp_hlen;
	MYBYTE bp_hops;
	MYDWORD bp_xid;
	struct
	{
		unsigned bp_secs:16;
		unsigned bp_spare:7;
		unsigned bp_broadcast:1;
		unsigned bp_spare1:8;
	};
	MYDWORD bp_ciaddr;
	MYDWORD bp_yiaddr;
	MYDWORD bp_siaddr;
	MYDWORD bp_giaddr;
	MYBYTE bp_chaddr[16];
	char bp_sname[64];
	MYBYTE bp_file[128];
	MYBYTE bp_magic_num[4];
};
#endif

struct dhcp_packet
{
	dhcp_header header;
	MYBYTE vend_data[1024 - sizeof(dhcp_header)];
};

struct data20
{
	MYBYTE options[sizeof(dhcp_packet)];
	MYWORD optionSize;
	MYDWORD ip;
	MYDWORD mask;
	MYBYTE rangeSetInd;
};

struct data9 //dhcpRequst
{
	MYDWORD lease;
	union
	{
		char raw[sizeof(dhcp_packet)];
		dhcp_packet dhcpp;
	};
	char hostname[256];
	char chaddr[64];
	MYDWORD server;
	MYDWORD reqIP;
	int bytes;
	SOCKADDR_IN remote;
	socklen_t sockLen;
	MYWORD messsize;
	MYBYTE *vp;
	data7 *dhcpEntry;
	data3 agentOption;
	data3 clientId;
	data3 subnet;
	data3 vendClass;
	data3 userClass;
	MYDWORD subnetIP;
	MYDWORD targetIP;
	MYDWORD rebind;
	MYDWORD dns;
	msghdr msg;
	iovec iov[1];
	msg_control msgcontrol;
	MYBYTE paramreqlist[256];
	MYBYTE opAdded[256];
	MYBYTE req_type;
	MYBYTE resp_type;
	MYBYTE sockInd;
};

struct DhcpConnType
{
	SOCKET sock;
	SOCKADDR_IN addr;
	MYDWORD server;
	MYWORD port;
	bool loaded;
	bool ready;
	MYDWORD mask;
	int reUseVal;
	int reUseSize;
	union
	{
		int broadCastVal;
		bool pktinfoVal;
	};
	union
	{
		int broadCastSize;
		unsigned int pktinfoSize;
	};
};

#if defined(__LP64__) || defined(_WIN64) || (defined(__x86_64__) && !defined(__ILP32__) ) || defined(_M_X64) || defined(__ia64) || defined (_M_IA64) || defined(__aarch64__) || defined(__powerpc64__)
#define MY_MAX_TIME UINT_MAX
#define MY_EXE_SIZE "64bit"
struct data8 //client
{
	union
	{
		MYDWORD filler;
		struct
		{
			unsigned dhcpInd: 20;
			unsigned bp_hlen: 8;
			unsigned fixed: 1;
			unsigned local: 1;
			unsigned display: 1;
			unsigned subnetFlg: 1;
		};
	};
	MYDWORD ip;
	time_t expiry;
	MYBYTE bp_chaddr[16];
	char hostname[64];
};
#else
#define MY_MAX_TIME INT_MAX
#define MY_EXE_SIZE "32bit"
struct data8 //client
{
	union
	{
		MYDWORD filler;
		struct
		{
			unsigned dhcpInd: 20;
			unsigned bp_hlen: 8;
			unsigned fixed: 1;
			unsigned local: 1;
			unsigned display: 1;
			unsigned subnetFlg: 1;
		};
	};
	MYDWORD ip;
	time_t expiry;
	MYDWORD filler1;
	MYBYTE bp_chaddr[16];
	char hostname[64];
};
#endif

/*
struct data8 //client
{
	MYWORD dhcpInd;
	MYBYTE bp_hlen;
	unsigned local: 8;
	unsigned source: 16;
	unsigned ip: 32;
	unsigned expiry: 32;
	MYBYTE bp_chaddr[16];
	char hostname[64];
};
*/

struct data1
{
	DhcpConnType dhcpListener;
	DhcpConnType dhcpConn[MAX_SERVERS];
	ConnType dnsUdpConn[MAX_SERVERS];
	ConnType forwConn;
	ConnType dnsTcpConn[MAX_SERVERS];
	ConnType httpConn;
	MYDWORD allServers[MAX_SERVERS];
	MYDWORD allMasks[MAX_SERVERS];
	char staticNames[MAX_SERVERS][256];
	MYDWORD listenServers[MAX_SERVERS];
	MYDWORD listenMasks[MAX_SERVERS];
	MYDWORD staticServers[MAX_SERVERS];
	MYDWORD staticMasks[MAX_SERVERS];
	MYDWORD dns[MAX_SERVERS];
	SOCKET maxFD;
	SOCKET maxFDPlusOne;
	MYBYTE currentDNS;
	bool ready;
	bool busy;
};

struct data2
{
	char servername[128];
	char servername_fqn[256];
	char zone[256];
	MYBYTE zLen;
	MYDWORD failureCount;
	MYDWORD failureCycle;
	bool ifspecified;
	ConnType dhcpReplConn;
	MYDWORD oldservers[MAX_SERVERS];
	MYDWORD specifiedServers[MAX_SERVERS];
	MYDWORD zoneServers[2];
	MYDWORD httpClients[8];
	char logFileName[256];
	data13 dhcpRanges[MAX_DHCP_RANGES];
	data14 rangeSet[MAX_RANGE_SETS];
	MYDWORD mask;
	MYDWORD lease;
	SOCKET fixedSocket;
	MYDWORD serial;
	MYDWORD dhcpSize;
	time_t dhcpRepl;
	MYDWORD rangeStart;
	MYDWORD rangeEnd;
	MYBYTE *options;
	MYWORD dhcpInd;
	MYBYTE replication;
	MYBYTE dhcpLogLevel;
    struct ifreq IfcBuf[MAX_SERVERS];
	MYBYTE ifc_len;
	pid_t ppid;
	bool hasFilter;
	char rangeCount;
};

struct data15
{
	union
	{
		MYDWORD ip;
		//unsigned ip:32;
		MYBYTE octate[4];
	};
};

//Function Prototypes
bool checkIP(data9 *req, data17 *rangeData, MYDWORD ip);
bool checkMask(MYDWORD);
bool checkRange(data17*, char);
bool detectChange();
bool getSection(const char*, char*, MYBYTE, char*);
bool isInt(char*);
bool isIP(char*);
FILE *openSection(const char*, MYBYTE);
MYBYTE pIP(void*, MYDWORD);
MYBYTE pInt(void*, MYDWORD);
MYBYTE pUShort(void*, MYWORD);
MYBYTE addServer(MYDWORD*, MYBYTE, MYDWORD);
char getRangeInd(MYDWORD);
char* myTrim(char*, char*);
char* myGetToken(char*, MYBYTE);
char* cloneString(char*);
char* getHexValue(MYBYTE*, char*, MYBYTE*);
char* genHostName(char*, MYBYTE*, MYBYTE);
char* hex2String(char*, MYBYTE*, MYBYTE);
char* IP2String(char*, MYDWORD);
char* IP2arpa(char*, MYDWORD);
char* IP62String(char*, MYBYTE*);
char* myUpper(char* string);
char* myLower(char* string);
char* readSection(char*, FILE*);
data7* findDHCPEntry(char*);
data7 *createCache(data71 *lump);
MYDWORD alad(data9*);
MYDWORD calcMask(MYDWORD, MYDWORD);
MYDWORD chaddr(data9*);
MYDWORD resad(data9*);
MYDWORD sdmess(data9*);
MYDWORD sendRepl(data9 *req);
MYDWORD* findServer(MYDWORD*, MYBYTE, MYDWORD);
int getIndex(char, MYDWORD);
void addDHCPRange(char *dp);
void addVendClass(MYBYTE rangeSetInd, char *vendClass, MYBYTE vendClassSize);
void addUserClass(MYBYTE rangeSetInd, char *userClass, MYBYTE userClassSize);
void addMacRange(MYBYTE rangeSetInd, char *macRange);
void addOptions(data9 *req);
void calcRangeLimits(MYDWORD, MYDWORD, MYDWORD*, MYDWORD*);
void catch_int(int sig_num);
void checkSize(MYBYTE);
void closeConn();
void debug(const char *mess, int i);
bool getInterfaces(data1*);
void getSecondary();
void *init(void*);
void lockOptions(FILE*);
void loadOptions(FILE*, const char*, data20*);
void logDHCPMess(char*, MYBYTE);
void logDHCPMess(const char *title, const char *mess);
void mySplit(char*, char*, char*, char);
void *sendHTTP(void*);
void procHTTP(data19*);
void pvdata(data9*, data3*);
void recvRepl(data9*);
void runProg();
void lockIP(MYDWORD);
void sendScopeStatus(data19 *req);
void sendStatus(data19 *req);
void setTempLease(data7*);
void setLeaseExpiry(data7*);
void setLeaseExpiry(data7*, MYDWORD);
void *updateStateFile(void*);
MYWORD fUShort(void*);
MYWORD gdmess(data9*, MYBYTE);
MYWORD myTokenize(char*, char*, const char*, bool);
MYWORD pQu(char*, char*);
MYWORD qLen(char*);
