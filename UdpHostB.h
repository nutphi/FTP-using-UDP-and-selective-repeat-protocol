/*************************************************************************************
*								 File Name	: UdpHostB.h		   			 	     *
*	Usage : Handles Client request for Uploading, downloading and listing of files   *
**************************************************************************************/
#ifndef UDP_HOSTB_H
#define UDP_HOSTB_H

#pragma comment(lib, "Ws2_32.lib")
#define HOSTNAME_LENGTH 20
#define FILENAME_LENGTH 20
#define REQUEST_PORT 5001
#define RESPONSE_PORT 7001			/* Router Port Number */
#define BUFFER_LENGTH 2048			/* Message format buffer size */
#define DATA_BUFFER_LENGTH 2012		/* Data Buffer Size */
#define TIMEOUT_USEC 3000000			/* Time-out value 3000000*/
#define MAX_TRIES 20		/* Maximum allowed Re-transmission of Data Packet */
#define LAST_PACKET_MAX_TRIES 3		/* Maximum allowed Re-transmission of Last Packet */
#define TRACE 1

#define WINDOWS_SIZE 3
#define SEQUENCE_SIZE 7

using namespace std;
/* Type of Messages */
typedef enum
{
	REQ_GET = 1, REQ_PUT, REQ_LIST, REQ_NEGO, NO_TYPE
} Type;

/* Format of Data packet transmitted */
typedef struct
{
	int seqNum;
	int ackNum;
	char fileName[FILENAME_LENGTH];
	char dataBuffer[DATA_BUFFER_LENGTH];
} dataPacketStore;

/* Message format used for sending and receiving datas */
typedef struct
{
	Type type;
	int  length;
	char buffer[BUFFER_LENGTH];
} Msg;

/* UdpHostB Class */
class UdpHostB
{
private:
	int hostBSocket;				/* Socket descriptor for server and client*/
	SOCKADDR_IN listenAddr;			/* Address used for listening incoming to requests */
	SOCKADDR_IN destAddr;			/* Address used for transmitting the data packet */
	Msg sendMsg, receiveMsg;		/* Variables handles the sending and receiving of Message */
	dataPacketStore sendPacketBuffer;	/*Buffer stores data to be transmitted */
	int finalSeqNum;				/* Final sequence number */
	ofstream fout;					/* Pointer to log file */

	vector<char*> databuffer;
	vector<int> windowslide;
	ifstream fileToRead;
	int sequence = 0;
	bool isReadEndOfFile;
	bool isFirstOfFile = false;

	string fileName;
	int numBytesSent;					/* Variable to store the bytes of data sent to the client */
	int numBytesRecv;					/* Variable to store the bytes of data received from the client */
	char* buffer;

public:
	UdpHostB(char *logFile = "log.txt");
	//~UdpHostB();
	char hostName[HOSTNAME_LENGTH];			/* Variable stores the Host Name */
	void start();							/* Starts the connection */
	void getOperation(char fileName[], int receivedSeqNum);				/* Sends the contents of the file (get)*/
	void putOperation();
	int sendMessage();
	int receiveMessage();
	//void putOperation(char[]);			/* Receives and uploads the received file contents */
	void sendFilesList(int receivedSeqNum);					/* Sends the list of available files */
	bool negotiateSequence(int receivedSeqNum);	/* Handles the Negotiation Process */
	int sendOldGetMessage();
	int sendNewGetMessage();
	void nextSequence();

	/*Static Variables required for maintaining the connection with Peer */
	static int transSeqNum;
	static bool connectionEstablished;
	static Type reqType;
};

#endif
