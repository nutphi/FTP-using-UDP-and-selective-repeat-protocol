/*************************************************************************************
*								 File Name	: UdpHostA.h	   			 	         *
*	Usage : Sends request to Server for Uploading, downloading and listing of files  *
**************************************************************************************/
#ifndef UDP_HOSTA_H

#define UDP_HOSTA_H
#include <winsock.h>
#include <cstdio>
#include <iostream>
#include <cstring>
#include <string>
#include <fstream>
#include <climits>
#include <windows.h>
#include <list>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <process.h>
#include <vector>
using namespace std;

#pragma comment(lib, "Ws2_32.lib")
#define HOSTNAME_LENGTH 20
#define FILENAME_LENGTH 20
#define REQUEST_PORT 7000			/* Router's Port Num */
#define RECEIVE_PORT 5000
#define BUFFER_LENGTH 2048			/* Total Message Buffer Size */
#define DATA_BUFFER_LENGTH 2012	
#define TIMEOUT_USEC 3000000			/* Time-out value 300000*/
#define MAX_TRIES 20/* Maximum Re-Transmission for Data Packet */
#define LAST_PACKET_MAX_TRIES 7		/* Maximum Re-Transmission for Terminating Packet */

#define TRACE 1


#define WINDOWS_SIZE 3
#define SEQUENCE_SIZE 7

/* Types of Messages */
typedef enum
{
	REQ_GET = 1, REQ_PUT = 2, REQ_LIST = 3
} Type;

/* Structure of Data Packet Used for Transfer */
typedef struct
{
	int seqNum;
	int ackNum;
	char fileName[FILENAME_LENGTH];
	char dataBuffer[DATA_BUFFER_LENGTH];
} dataToTransfer;

/* Message format used for sending and receiving */
typedef struct
{
	Type type;
	int  length;				/* length of effective bytes in the buffer */
	char buffer[BUFFER_LENGTH];
} Msg;

/* UdpHostA Class */
class UdpHostA
{
private:
	vector<char*> databuffer;
	vector<int> windowslide;
	fd_set readfds;
	int hostASocket;					/* Socket descriptor */
	char hostName[HOSTNAME_LENGTH];		/* Host Name */
	Msg sendMsg, receiveMsg;				/* Message structure variables for Sending and Receiving data */
	WSADATA wsaData;					/* Variable to store socket information */
	string serverIpAdd;					/* Variable to store Server IP Address */
	string transferType;				/* Variable to store the Type of Operation */
	string fileName;					/* Variable to store name of the file for retrieval or transfer */
	int numBytesSent;					/* Variable to store the bytes of data sent to the server */
	int numBytesRecv;					/* Variable to store the bytes of data received from the server */
	dataToTransfer dataPacket;			/* Takes the Information and Data of Data Packet being transmitted */
	SOCKADDR_IN destAddr, listenAddr;	/* Variable that holds the listening and transmitting socket information */
	list <string> fileList;				/* List stores the list of available files in other peer */
	ofstream fout;						/* Pointer to Log File */
	dataToTransfer* receivedPacket = nullptr;
	int sequence = 0;
	char* buffer;
	char* buffer2;
	//for put operation
	ifstream fileToRead;
	bool isReadEndOfFile;
	bool isFirstOfFile = false;

public:
	void run();							/* Invokes the appropriate function based on selected option */
	void doOperation();
	void getOperation();				/* Retrieves the file from other Peer */
	void putOperation();				/* Transfers the file to other Peer */
	void showMenu();					/* Displays the list of available options for User */
	bool initiateNegotiation();			/* Initiates 3 way Handshake Process */
	void startClient(char *logFile = "log.txt");/* Starts the client process */
	int receiveMessage();
	int sendMessage();
	int sendOldGetMessage();
	int sendNewGetMessage();
	void nextSequence();
	unsigned long ResolveName(string name);	/* Resolve the specified host name */
	~UdpHostA();

	int transSeqNum;				/* Maintains the negotiated sequence number */
	Type selectedReqType;		/* Stores the selected request type and sends in Request */
};

#endif