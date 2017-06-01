/*************************************************************************************
*								 File Name	: UdpHostB.cpp		   			 	     *
*	Usage : Handles Client request for Uploading, downloading and listing of files   *
**************************************************************************************/
#define _CRT_SECURE_NO_WARNINGS
#include <winsock.h>
#include <iostream>
#include <windows.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <process.h>
#include <vector>
#include "UdpHostB.h"

using namespace std;

/* Initialize Static Variables necessary for establishing connection */
int UdpHostB::transSeqNum = -1;
bool UdpHostB::connectionEstablished = false;
Type UdpHostB::reqType = NO_TYPE;

int getplace(int head, int seqNum)
{
	if (seqNum >= head)
	{
		return seqNum - head ;
	}
	else
	{
		return SEQUENCE_SIZE - head + seqNum;
	}
}
bool within(int head, int seqNum)
{
	int tail = (head + WINDOWS_SIZE - 1) % SEQUENCE_SIZE;
	if (tail >= head)
	{
		if (seqNum >= head&&seqNum <= tail)
			return true;
	}
	else
	{
		if (seqNum>=head || seqNum<=tail)
			return true;
	}
	return false;
}

/**
* Constructor - UdpHostB
* Usage: Initializes the log file, WinSocket and get the host name and Peer IP Address to connnect
*
* @arg: char*	Name of the Log File
*/
UdpHostB::UdpHostB(char *logFile)
{
	/* Open the log file */
	fout.open(logFile);

	WSADATA wsadata;
	//HOSTENT* hp;
	/* Initialize WinSocket */
	if (WSAStartup(0x0202, &wsadata) != 0)
	{
		WSACleanup();
		cerr << "Error in starting WSAStartup()";
		return;
	}

	/* Get Host Name */
	if (gethostname(hostName, HOSTNAME_LENGTH) != 0)
	{
		cerr << "can not get the host name,program ";
		return;
	}

	cout << "ftpd_udp starting at host: " << hostName << endl;

	/* UDP Socket Creation */
	if ((hostBSocket = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET)
	{
		cerr << "Create UDP Socket failed.. " << endl;
		return;
	}


	memset(&listenAddr, 0, sizeof(listenAddr));
	listenAddr.sin_family = AF_INET;
	listenAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	listenAddr.sin_port = htons(REQUEST_PORT);

	// Binding the Socket to the Listening Port Number
	if (bind(hostBSocket, (struct sockaddr *) &listenAddr, sizeof(listenAddr)) < 0)
	{
		cerr << "Socket Binding Error,exit" << endl;
		closesocket(hostBSocket);
		return;
	}
	//***************************************************/

	/*
	memset(&destAddr, 0, sizeof(destAddr));
	memcpy(&destAddr.sin_addr, hp->h_addr, hp->h_length);
	destAddr.sin_family = hp->h_addrtype;
	destAddr.sin_port = htons(RESPONSE_PORT);
	*/
}

/**
* Function - startClient
* Usage:
*
* @arg: void
*/
void UdpHostB::start()
{
	memset(&receiveMsg, '\0', BUFFER_LENGTH);
	memset(&sendMsg, '\0', BUFFER_LENGTH);
	memset(&sendPacketBuffer, '\0', DATA_BUFFER_LENGTH);
	cout << "Waiting to be contacted for transferring files... " << endl;
	int destAddrSize = sizeof(destAddr);
	if ((recvfrom(hostBSocket, (char *)&receiveMsg, sizeof(receiveMsg), 0, (SOCKADDR*)&destAddr, &destAddrSize)) == SOCKET_ERROR)
	{
		cerr << "Get buffer error!" << endl;
		return;
	}
	else {
		int seqNum = ((dataPacketStore*)&receiveMsg.buffer)->seqNum;
		cout << "server <- client" << endl;
		cout << "seq = " << ((dataPacketStore*)&receiveMsg.buffer)->seqNum << endl;
		cout << "ack = " << ((dataPacketStore*)&receiveMsg.buffer)->ackNum << endl;
		cout << "buffer = " << ((dataPacketStore*)&receiveMsg.buffer)->dataBuffer << "\n" << endl;
		if (negotiateSequence(seqNum)) {
			cout << "type ::" << reqType << "\n" << endl;
			if (reqType == REQ_LIST) {
				return;
			}
		}
	}
	return;
}


void UdpHostB::nextSequence() {
	sequence = (sequence + 1) % SEQUENCE_SIZE;
}

int UdpHostB::sendMessage() {
	cout << "server - > client" << endl;
	cout << "seq :: " << sendPacketBuffer.seqNum << endl;
	cout << "ack :: " << sendPacketBuffer.ackNum << endl;
	cout << "type :: " << sendMsg.type << endl;
	//cout << "dataBuffer :: " << sendPacketBuffer.dataBuffer << endl;
	memcpy(sendMsg.buffer, &sendPacketBuffer, sizeof(sendPacketBuffer));

	if (TRACE)
	{
		fout << "server ----> client " << endl;
		fout << "sequence number::" << sendPacketBuffer.seqNum << endl;
		fout << "acknowledge number::" << sendPacketBuffer.ackNum << endl;
	}

	return sendto(hostBSocket, (char *)&sendMsg, BUFFER_LENGTH, 0, (SOCKADDR*)&destAddr, sizeof(destAddr));
}

int UdpHostB::receiveMessage() {
	int destAddrSize = sizeof(destAddr);
	//memset(&receiveMsg, '\0', BUFFER_LENGTH);
	dataPacketStore* receiveNewMessage = (dataPacketStore *)receiveMsg.buffer;
	memset(receiveNewMessage->dataBuffer, '\0', DATA_BUFFER_LENGTH);
	int result = recvfrom(hostBSocket, (char *)&receiveMsg, BUFFER_LENGTH, 0, (SOCKADDR*)&destAddr, &destAddrSize);

	cout << "server < - client" << endl;
	cout << "seq :: " << ((dataPacketStore *)receiveMsg.buffer)->seqNum << endl;
	cout << "ack :: " << ((dataPacketStore *)receiveMsg.buffer)->ackNum << endl;
	cout << "type :: " << receiveMsg.type << endl;
	//cout << ":: dataBuffer :: " << endl;
	cout << receiveNewMessage->dataBuffer << endl;
	if (TRACE)
	{
		fout << "server <--acknowledge number::" << ((dataPacketStore *)receiveMsg.buffer)->ackNum << " -- client " << endl;
		fout << "server <--acknowledge number::" << ((dataPacketStore *)receiveMsg.buffer)->ackNum << " -- client " << endl;
	}
	return result;
}

int UdpHostB::sendOldGetMessage() {
	vector<char*>::iterator it_databuffer;
	vector<int>::iterator it_windowslide;
	it_windowslide = find(windowslide.begin(), windowslide.end(), sequence);
	for (int i = 0; i < windowslide.size(); i++) {
		if (it_windowslide[0] == windowslide[i]) {
			memset(&sendPacketBuffer, '\0', DATA_BUFFER_LENGTH);
			sendPacketBuffer.seqNum = windowslide[i];
			memcpy(sendPacketBuffer.dataBuffer, databuffer.at(i), DATA_BUFFER_LENGTH);
			if (sendMessage() == SOCKET_ERROR) {
				return SOCKET_ERROR;
			}
			else {
				return 1;
			}
		}
	}
	return 0;
}

int UdpHostB::sendNewGetMessage() {
	bool isEnd = fileToRead.eof();
	if (isReadEndOfFile) {
		return 0;
	}
	else if (!isEnd && windowslide.size() < WINDOWS_SIZE) {
		sendPacketBuffer.seqNum = sequence;
		if (isFirstOfFile) {
			isFirstOfFile = false;
			sendPacketBuffer.ackNum = ((dataPacketStore *)receiveMsg.buffer)->seqNum;
		}
		else {
			sendPacketBuffer.ackNum = -1;
		}
		memset(sendPacketBuffer.dataBuffer, '\0', DATA_BUFFER_LENGTH);
		fileToRead.read(sendPacketBuffer.dataBuffer, DATA_BUFFER_LENGTH);
		windowslide.push_back(sequence);
		buffer = new char[DATA_BUFFER_LENGTH];
		memcpy(buffer, sendPacketBuffer.dataBuffer, sizeof(sendPacketBuffer.dataBuffer));
		databuffer.push_back(buffer);
		if (sendMessage() == SOCKET_ERROR) {
			return SOCKET_ERROR;
		}
		nextSequence();
	}
	else if (isEnd) {

		sendPacketBuffer.seqNum = sequence;
		sendPacketBuffer.ackNum = -1;
		windowslide.push_back(sequence);
		memset(sendPacketBuffer.dataBuffer, '\0', DATA_BUFFER_LENGTH);
		memcpy(sendPacketBuffer.dataBuffer, "END OF FILE", sizeof("END OF FILE"));
		memset(buffer, '\0', DATA_BUFFER_LENGTH);
		memcpy(buffer, sendPacketBuffer.dataBuffer, sizeof(sendPacketBuffer.dataBuffer));
		databuffer.push_back(buffer);
		nextSequence();
		isReadEndOfFile = true;
		fileToRead.close();
		if (sendMessage() == SOCKET_ERROR) {
			return SOCKET_ERROR;
		}
	}
	return 0;
}

void UdpHostB::putOperation() {
	windowslide.clear();
	databuffer.clear();
	vector<char*>::iterator it_databuffer;
	vector<int>::iterator it_windowslide;
	int reTransCount = 0;
	//first sequence on windows
	sequence = 0;
	fileName = string("file\\") + fileName;
	ofstream myFile(fileName, ios::out | ios::binary);
	fd_set readfds;
	struct timeval *tp = new timeval;
	tp->tv_sec = 0;
	tp->tv_usec = TIMEOUT_USEC;
	int RetVal = 0;
	int destAddrLen = sizeof(destAddr);
	bool isFirstReceived = true;
	buffer = new char[DATA_BUFFER_LENGTH];
	/* Handle the first packets */
	dataPacketStore *recvNewPacketBuffer;
	recvNewPacketBuffer = (dataPacketStore *)receiveMsg.buffer; // This line needs to be changed as per your implementation
																//if the sequence number is not the first windowslide

	int length = recvNewPacketBuffer->seqNum - sequence;
	for (int i = 0; i <= length; i++)
	{
		//buffer = new char[DATA_BUFFER_LENGTH];
		//memset(buffer, '\0', DATA_BUFFER_LENGTH);
		transSeqNum = -1;
		windowslide.push_back(transSeqNum);
		databuffer.push_back(NULL);
	}


	transSeqNum = recvNewPacketBuffer->seqNum;
	windowslide[length]=transSeqNum;
	buffer = new char[DATA_BUFFER_LENGTH];
	memcpy(buffer, recvNewPacketBuffer->dataBuffer, DATA_BUFFER_LENGTH);
	databuffer[length]=buffer;
	sendPacketBuffer.seqNum = -1;
	sendPacketBuffer.ackNum = recvNewPacketBuffer->seqNum;
	sendMsg.type = receiveMsg.type;
	memcpy(sendMsg.buffer, &sendPacketBuffer, sizeof(sendPacketBuffer));
	while (databuffer.size() != 0 && databuffer[0] != NULL )
	{
		//int w = find(windowslide.begin(), windowslide.end(), sequence) - windowslide.begin();
		if (strcmp(databuffer[0], "END OF FILE") == 0)
		{
			myFile.close();
			nextSequence();
			cout << "A file complete" << endl;
			if (TRACE)
			{
				fout << "a file complete" << endl;
			}
		}
		else
		{
			myFile.write(databuffer[0], DATA_BUFFER_LENGTH);
			nextSequence();
		}
		databuffer.erase(databuffer.begin());
		windowslide.erase(windowslide.begin());
	}


	bool isReceiveEndOfFile = false;
	bool isCloseFile = false;
	if (sendMessage() == SOCKET_ERROR)
	{
		cout << "error" << endl;
		return;
	}
	else
	{
		cout << "pass" << endl;
	}

	while (1)
	{
		FD_ZERO(&readfds);
		FD_SET(hostBSocket, &readfds);
		if ((RetVal = select(1, &readfds, NULL, NULL, tp)) == SOCKET_ERROR)	/* Select call waits for the specified time */
		{
			cerr << "Timer error!" << endl;
			return;
		}
		else if (RetVal > 0)	/* There are incoming packets */
		{
			if (FD_ISSET(hostBSocket, &readfds))	/* Incoming packet from peer host 1 */
			{
				if ((numBytesRecv = receiveMessage()) == SOCKET_ERROR)
				{
					//Socket Error
					return;
				}
				else
				{
					//Handle the received packet
					recvNewPacketBuffer = (dataPacketStore *)receiveMsg.buffer; // This line needs to be changed as per your implementation
					it_windowslide = find(windowslide.begin(), windowslide.end(), recvNewPacketBuffer->seqNum);
					if (strcmp(recvNewPacketBuffer->dataBuffer, "No such file") == 0 || strcmp(recvNewPacketBuffer->dataBuffer, " ") == 0)
					{
						//Handle no such file exists scenario					
						return;
					}
					else
					{
						/* If the received packet is a new packet, Check the sequence number and Pack and send the acknowledgement */
						if (it_windowslide != windowslide.end())
						{
							//if the same sequence number, send ack back
							//because server lost syn with file message before but client already get it
							sendPacketBuffer.seqNum = -1;
							sendPacketBuffer.ackNum = recvNewPacketBuffer->seqNum;
							sendMsg.type = receiveMsg.type;
							memcpy(sendMsg.buffer, &sendPacketBuffer, sizeof(sendPacketBuffer));
							if (sendMessage() == SOCKET_ERROR)
							{
								cout << "error" << endl;
								return;
							}
							else
							{
								cout << "pass" << endl;
							}
						}
						else if (recvNewPacketBuffer->seqNum == sequence)
						{
							//if it is the first on windowslide
							// send only the first packet
							int length = getplace(sequence, recvNewPacketBuffer->seqNum);
							if (length + 1>windowslide.size())
								for (int i = windowslide.size(); i <= length; i++) {
								buffer = new char[DATA_BUFFER_LENGTH];
								memset(buffer, '\0', DATA_BUFFER_LENGTH);

								transSeqNum = (sequence + i) % SEQUENCE_SIZE;
								windowslide.push_back(transSeqNum);
								databuffer.push_back(buffer);

								}

							transSeqNum = recvNewPacketBuffer->seqNum;
							windowslide[length] = transSeqNum;
							buffer = new char[DATA_BUFFER_LENGTH];
							memset(buffer, '\0', DATA_BUFFER_LENGTH);
							memcpy(buffer, recvNewPacketBuffer->dataBuffer, sizeof(recvNewPacketBuffer->dataBuffer));
							databuffer[length] = buffer;
							
							sendPacketBuffer.seqNum = -1;
							sendPacketBuffer.ackNum = recvNewPacketBuffer->seqNum;
							memcpy(sendMsg.buffer, &sendPacketBuffer, sizeof(sendPacketBuffer));
							//int w = find(windowslide.begin(), windowslide.end(), sequence) - windowslide.begin();
							if (databuffer.size() != 0 && databuffer[0] != NULL&&strcmp(databuffer[0], "END OF FILE") == 0)
							{
								isReceiveEndOfFile = true;
								strncpy(sendPacketBuffer.dataBuffer, databuffer[0], sizeof("END OF FILE"));
							}
							else if (databuffer.size() != 0 && databuffer[0] != NULL)
							{
								strncpy(sendPacketBuffer.dataBuffer, "ACK", sizeof("ACK"));
							}
							while (databuffer.size() != 0 && databuffer[0]!=NULL && !isCloseFile)
							{
								//int w = find(windowslide.begin(), windowslide.end(), sequence) - windowslide.begin();
								if (strcmp(databuffer[0], "END OF FILE") == 0)
								{
									myFile.close();
									nextSequence();
									cout << "A file complete" << endl;
									if (TRACE)
									{
										fout << "a file complete" << endl;
									}
									isCloseFile = true;
								}
								else
								{
									myFile.write(databuffer[0], DATA_BUFFER_LENGTH);
									fout << windowslide[0] << endl;
									nextSequence();
								}
								databuffer.erase(databuffer.begin());
								windowslide.erase(windowslide.begin());
							}
							/*if (isFirstReceived)
							{
								isFirstReceived = false;
								sequence--;
							}*/
							memcpy(sendMsg.buffer, &sendPacketBuffer, sizeof(sendPacketBuffer));
							if (sendMessage() == SOCKET_ERROR)
							{
								cout << "error" << endl;
								return;
							}
						}
						else if (within(sequence, recvNewPacketBuffer->seqNum))
						{
							int length = getplace(sequence, recvNewPacketBuffer->seqNum);
							if (length + 1>windowslide.size())
							for (int i = windowslide.size(); i <= length; i++) {
								//buffer = new char[DATA_BUFFER_LENGTH];
								//memset(buffer, '\0', DATA_BUFFER_LENGTH);

								transSeqNum = -1;
								windowslide.push_back(transSeqNum);
								databuffer.push_back(NULL);

							}

							transSeqNum = recvNewPacketBuffer->seqNum;
							windowslide[length]=transSeqNum;
							buffer = new char[DATA_BUFFER_LENGTH];
							memset(buffer, '\0', DATA_BUFFER_LENGTH);
							memcpy(buffer, recvNewPacketBuffer->dataBuffer, sizeof(recvNewPacketBuffer->dataBuffer));
							databuffer[length]=buffer;

							sendPacketBuffer.seqNum = -1;
							sendPacketBuffer.ackNum = recvNewPacketBuffer->seqNum;
							memcpy(sendMsg.buffer, &sendPacketBuffer, sizeof(sendPacketBuffer));
							if (sendMessage() == SOCKET_ERROR)
							{
								cout << "error" << endl;
								return;
							}

						}
						else {
							//resend out of order
							sendPacketBuffer.seqNum = -1;
							sendPacketBuffer.ackNum = recvNewPacketBuffer->seqNum;
							memcpy(sendMsg.buffer, &sendPacketBuffer, sizeof(sendPacketBuffer));
							if (sendMessage() == SOCKET_ERROR)
							{
								cout << "error" << endl;
								return;
							}
						}
						/* Multiple Scenarios possible, Check for duplicate packet too and handle the duplicate packet scenario */
					}
				}
			}
		}
		else if (RetVal == 0)
		{
			//do nothing just wait for a packet
			while (databuffer.size() != 0 && databuffer[0] != NULL)
			{
				if (strcmp(databuffer[0], "END OF FILE") == 0)
				{
					myFile.close();
					nextSequence();
					cout << "file complete" << endl;
					if (TRACE)
					{
						fout << "a file complete" << endl;
					}
				}
				else
				{
					myFile.write(databuffer[0], DATA_BUFFER_LENGTH);
					nextSequence();
				}
				databuffer.erase(databuffer.begin());
				windowslide.erase(windowslide.begin());
			}
			/* If no packet arrives for specified time limit, Reinitiate the process - For Get operation */
			reTransCount++;
			cout << "Waiting for Re-Transmitting Packet :" << reTransCount << endl;
			if (reTransCount == MAX_TRIES)
			{
				cout << "Max Re-Transmission reached, Waiting for Re-Negotiation " << endl;
				if (TRACE)
				{
					fout << "Max Re-Transmission reached, Waiting for Re-Negotiation " << endl;
				}
				return;
			}
			/* If Put Operation - Implement the retransmission of data packet */
		}
	}
}
void UdpHostB::getOperation(char fileName[], int receivedSeqNum) {

	databuffer.clear();
	windowslide.clear();
	vector<char*>::iterator it_databuffer;
	vector<int>::iterator it_windowslide;
	sequence = 0;

	struct timeval *tp = new timeval;
	tp->tv_sec = 0;
	tp->tv_usec = TIMEOUT_USEC;
	int RetVal = 0;
	int reTransCount = 0;
	int destAddrSize = sizeof(destAddr);
	dataPacketStore *recvNewPacketBuffer;
	recvNewPacketBuffer = (dataPacketStore *)receiveMsg.buffer;
	fd_set readfds;

	isReadEndOfFile = false;
	int last_packet = 0;
	char path[] = "file\\";
	char* file_ = new char[strlen(path) + strlen(fileName)];
	memcpy(file_, path, sizeof(path));
	strcat(file_, fileName);
	fileToRead.open(file_, ios::in | ios::binary);
	struct _stat stat_buf;
	int result = _stat(file_, &stat_buf);

	if (result == -1)
	{
		memset(sendMsg.buffer, '\0', BUFFER_LENGTH);
		sendPacketBuffer.seqNum = (receivedSeqNum + 1) % SEQUENCE_SIZE;
		sendPacketBuffer.ackNum = receivedSeqNum;
		memcpy(sendPacketBuffer.dataBuffer, "No such file", sizeof("No such file"));
		memcpy(sendMsg.buffer, &sendPacketBuffer, sizeof(sendPacketBuffer));
		sendMsg.length = sizeof(sendMsg.buffer);
		sendMsg.type = REQ_GET;
		/* Send the contents of file recursively */
		if (sendMessage() == SOCKET_ERROR)
		{
			cout << "Socket Error occured while sending data " << endl;
			/* Close the connection and unlock the mutex if there is a Socket Error */
			closesocket(hostBSocket);
			return;
		}
		else
		{
			/* Reset the buffer */
			memset(sendMsg.buffer, '\0', sizeof(sendMsg.buffer));
		}
	}
	else {
		cout << "Have file" << endl;
		while (windowslide.size()<WINDOWS_SIZE&&!isReadEndOfFile)
		{
			if (sendNewGetMessage() == SOCKET_ERROR) {
				//if there is socket error
				cout << "server error" << endl;
				return;
			}
		}
	}

	while (1) {
		FD_ZERO(&readfds);
		FD_SET(hostBSocket, &readfds);
		RetVal = select(1, &readfds, NULL, NULL, tp);
		if (RetVal == SOCKET_ERROR)	// Check for incoming packets
		{
			cerr << "Timer error!" << endl;
			return;
		}
		else if (RetVal>0) {
			if (receiveMessage() == SOCKET_ERROR)
			{
				cerr << "Get buffer error!" << endl;
				return;
			}
			else if ((strcmp(recvNewPacketBuffer->dataBuffer, "No such file") == 0)) {
				return;
			}
			else if ((strcmp(recvNewPacketBuffer->dataBuffer, "END OF FILE") == 0)) {
				cout << "finish get operation ya huuu" << endl;
				return;
			}
			else {
				recvNewPacketBuffer = (dataPacketStore *)receiveMsg.buffer;
				it_windowslide = find(windowslide.begin(), windowslide.end(), recvNewPacketBuffer->ackNum);
				if (it_windowslide != windowslide.end()) {
					//if an ack from client is the same as seq on windowslide
					if (windowslide[0] == recvNewPacketBuffer->ackNum) {
						if (recvNewPacketBuffer->dataBuffer == NULL) {
							//it means the last packet end of file
							break;
						}
						//if the first windowslide
						do {

							windowslide.erase(windowslide.begin());
							databuffer.erase(databuffer.begin());
							if (!isReadEndOfFile) {
								// if the server did not send eof
								if (sendNewGetMessage() == SOCKET_ERROR) {
									//if there is socket error
									return;
								}
							}
						} while (databuffer.size() != 0 && databuffer[0] == NULL);
					}
					else {
						//if not the first windowslide
						//set '/0'
						for (int i = 0; i < windowslide.size(); i++) {
							if (recvNewPacketBuffer->ackNum == windowslide[i]) {
								databuffer[i] = '\0';
							}
						}
					}
				}
			}
		}
		else if (RetVal == 0)
		{
			cout << "retval : " << RetVal << endl;
			//dataPacketStore checkBuffer3 = nullptr;
			// Check for Maximum Re-Transmission and once it reaches the specified count, Re-start the negotiation process
			reTransCount++;
			for (int i = 0; i < databuffer.size(); i++) {
				if (databuffer[i] != NULL) {

					sendPacketBuffer.seqNum = windowslide[i];
					sendPacketBuffer.ackNum = -1;
					memcpy(sendPacketBuffer.dataBuffer, databuffer[i], DATA_BUFFER_LENGTH);
					if (sendMessage() == SOCKET_ERROR) {
						cerr << "error" << endl;
						return;
					}
				}
			}

			if (reTransCount == MAX_TRIES)
			{
				fileToRead.close();
				cout << "Max Re-Transmission reached, Waiting for Re-Negotiation " << endl;
				if (TRACE)
				{
					fout << "Max Re-Transmission reached, Waiting for Re-Negotiation " << endl;
				}
				return;
			}
			cout << "Re-Transmitting Packet Seq Num " << sendPacketBuffer.seqNum << endl;
			//cout << "Re-Transmitting Packet Seq Num " << checkBuffer3->seqNum << endl;
			if (TRACE)
			{
				fout << "Re-Transmitting Packet Seq Num " << sendPacketBuffer.seqNum << endl;
				//fout << "Re-Transmitting Packet Seq Num " << checkBuffer3->seqNum << endl;
			}
		}
	}
}

void UdpHostB::sendFilesList(int receivedSeqNum) {
	HANDLE find;
	WIN32_FIND_DATA data;
	string listFile;
	find = FindFirstFile("file\\*.*", &data);
	if (find != INVALID_HANDLE_VALUE) {
		do {

			if (strcmp(".", data.cFileName) == 0) {}
			else if (strcmp("..", data.cFileName) == 0) {}
			else {
				listFile = listFile + string(data.cFileName) + string("\n");
			}
		} while (FindNextFile(find, &data));
		FindClose(find);
	}
	sendMsg.type = receiveMsg.type;
	memcpy(sendPacketBuffer.dataBuffer, listFile.c_str(), strlen(listFile.c_str()));
	memcpy(sendMsg.buffer, &sendPacketBuffer, sizeof(sendPacketBuffer));

	if (sendMessage() == SOCKET_ERROR)
	{
		cout << "Socket Error occured while sending data " << endl;
		if (TRACE)
		{
			fout << "Socket Error occured while sending data " << endl;
		}
		closesocket(hostBSocket);
	}
	/*
	cout << "server -list-> client" << endl;
	cout << "seq = " << sendPacketBuffer.seqNum << endl;
	cout << "ack = " << sendPacketBuffer.ackNum << endl;
	cout << "buffer = " << sendPacketBuffer.dataBuffer << "\n" << endl;
	*/
	struct timeval *tp = new timeval;
	tp->tv_sec = 0;
	tp->tv_usec = TIMEOUT_USEC;
	int RetVal = 0;
	int reTransCount = 0;
	int destAddrSize = sizeof(destAddr);
	dataPacketStore *recvNewPacketBuffer;
	recvNewPacketBuffer = (dataPacketStore *)receiveMsg.buffer;
	fd_set readfds;

	while (1) {
		FD_ZERO(&readfds);
		FD_SET(hostBSocket, &readfds);
		if ((RetVal = select(1, &readfds, NULL, NULL, tp)) == SOCKET_ERROR)	// Check for incoming packets
		{
			cerr << "Timer error!" << endl;
			return;
		}
		else if (RetVal>0) {
			if (receiveMessage() == SOCKET_ERROR)
			{
				cerr << "Get buffer error!" << endl;
				cout << "b" << endl;
				return;
			}
			else if (receivedSeqNum == recvNewPacketBuffer->seqNum)
			{
				/*
				cout << "server --repeat--list--> client" << endl;
				cout << "seq = " << sendPacketBuffer.seqNum << endl;
				cout << "ack = " << sendPacketBuffer.ackNum << endl;
				cout << "buffer = " << sendPacketBuffer.dataBuffer << "\n" << endl;
				*/
				if (sendMessage() == SOCKET_ERROR)
				{
					cout << "Socket Error occured while sending data " << endl;
					if (TRACE)
					{
						fout << "Socket Error occured while sending data " << endl;
					}
					closesocket(hostBSocket);
				}
			}
			else {
				/*
				cout << "server<-client Last LIST" << endl;
				cout << "seqNum = " << ((dataPacketStore *)receiveMsg.buffer)->seqNum << endl;
				cout << "ackNum = " << ((dataPacketStore *)receiveMsg.buffer)->ackNum << endl;
				cout << "buffer = " << ((dataPacketStore *)receiveMsg.buffer)->dataBuffer << endl;
				*/
				return;
			}

		}
		else if (RetVal == 0)
		{
			cout << "retval : " << RetVal << endl;
			dataPacketStore *checkBuffer2 = (dataPacketStore *)sendMsg.buffer;
			// Check for Maximum Re-Transmission and once it reaches the specified count, Re-start the negotiation process

			reTransCount++;
			if (reTransCount == MAX_TRIES)
			{
				cout << "Max Re-Transmission reached, Waiting for Re-Negotiation " << endl;
				if (TRACE)
				{
					fout << "Max Re-Transmission reached, Waiting for Re-Negotiation " << endl;
				}
				return;
			}
			cout << "Re-Transmitting Packet Seq Num " << checkBuffer2->seqNum << endl;
			if (TRACE)
			{
				fout << "Re-Transmitting Packet Seq Num " << checkBuffer2->seqNum << endl;
			}
		}
	}
}

/**
* Function - negotiateSequence
* Usage: Negotiates with the Host that Initiated HandShake process
*
* @arg: int Sequence Number received from Host
*/
bool UdpHostB::negotiateSequence(int receivedSeqNum)
{
	int recvlen, RetVal;
	fd_set readfds;
	dataPacketStore *recvNewPacketBuffer;
	int countOfTimeOutAck = 0;
	int countOfDuplicateAck = 0;
	srand((unsigned int)time(NULL));
	rand();
	int randSeqNum = (rand() + 100) % 255;
	int currentPackSeqNum = 0;
	int reTransCount = 0;
	struct timeval *tp = new timeval;
	tp->tv_sec = 0;
	tp->tv_usec = TIMEOUT_USEC;
	cout << "random seq number" << randSeqNum << endl;
	/* Store the recevied sequence number for future communication */
	transSeqNum = receivedSeqNum & 0x01;
	cout << "transSeqNum" << transSeqNum << endl;
	memset(sendMsg.buffer, '\0', sizeof(sendMsg.buffer));
	memset(sendPacketBuffer.dataBuffer, '\0', sizeof(sendPacketBuffer.dataBuffer));
	sendPacketBuffer.ackNum = -1;
	sendPacketBuffer.seqNum = -1;

	/* Acknowledge the Received packet and Generate
	and Send the Response Sequence Number */
	if ((receivedSeqNum & 0x01) == 1)
	{
		sendPacketBuffer.seqNum = ((randSeqNum) % 2 == 1 ? randSeqNum + 1 : randSeqNum);
	}
	else
	{
		sendPacketBuffer.seqNum = ((randSeqNum) % 2 == 1 ? randSeqNum : randSeqNum + 1);
	}
	sendPacketBuffer.ackNum = receivedSeqNum;
	currentPackSeqNum = sendPacketBuffer.seqNum;
	memcpy(sendPacketBuffer.dataBuffer, "ACK", sizeof("ACK"));
	memcpy(sendMsg.buffer, &sendPacketBuffer, sizeof(sendPacketBuffer));
	sendMsg.length = sizeof(sendMsg.buffer);

	if (sendMessage() == SOCKET_ERROR)
	{
		cout << "Socket Error occured while sending data " << endl;
		if (TRACE)
		{
			fout << "Socket Error occured while sending data " << endl;
		}
		closesocket(hostBSocket);
		return false;
	}

	cout << "Sent Response Seq Num " << sendPacketBuffer.seqNum << endl;
	if (TRACE)
	{
		fout << "Sent Response Seq Num " << sendPacketBuffer.seqNum << endl;
	}

	// Wait for Acknowledgement, Break the loop once Negotiation is successful 
	while (1)
	{
		FD_ZERO(&readfds);
		FD_SET(hostBSocket, &readfds);
		if ((RetVal = select(1, &readfds, NULL, NULL, NULL)) == SOCKET_ERROR)	// Check for incoming packets
		{
			cerr << "Timer error!" << endl;
			return false;
		}
		else if (RetVal>0)	// There are incoming packets 
		{

			if (FD_ISSET(hostBSocket, &readfds))	// Incoming packet from peer host 1 
			{
				recvlen = receiveMessage();
				recvNewPacketBuffer = (dataPacketStore *)receiveMsg.buffer;

				if (recvlen == SOCKET_ERROR)
				{
					cerr << "Get buffer error!" << endl;
					return false;
				}
				else
				{
					// If the received packet is acknowledgement of previously transmitted packet, Negotiation is successfull 
					if ((currentPackSeqNum == recvNewPacketBuffer->ackNum) && (!(strcmp(recvNewPacketBuffer->dataBuffer, "ACK"))))
					{
						reqType = receiveMsg.type;
						cout << "reqType ---:: " << reqType << endl;
						finalSeqNum = recvNewPacketBuffer->seqNum;
						cout << "Ack received for Handshake " << endl;
						if (TRACE)
						{
							fout << "Ack received for Handshake " << endl;
						}
						if (receiveMsg.type == REQ_LIST) {
							//resend list packet
							if (TRACE)
							{
								fout << "LIST OPERATION" << endl;
							}
							sendPacketBuffer.ackNum = recvNewPacketBuffer->seqNum;
							sendPacketBuffer.seqNum = (recvNewPacketBuffer->seqNum + 1) % 7;
							sendFilesList(recvNewPacketBuffer->seqNum);
							return true;
						}
						else if (receiveMsg.type == REQ_GET) {
							if (TRACE)
							{
								fout << "GET OPERATION" << endl;
							}
							cout << "fileName :: " << recvNewPacketBuffer->fileName << endl;
							cout << "seqNum :: " << recvNewPacketBuffer->seqNum << endl;
							isFirstOfFile = true;
							getOperation(recvNewPacketBuffer->fileName, recvNewPacketBuffer->seqNum);
							return true;
						}
					}
					else if ((currentPackSeqNum == recvNewPacketBuffer->ackNum) && receiveMsg.type == REQ_PUT) {
						if (TRACE)
						{
							fout << "PUT OPERATION" << endl;
						}
						reqType = receiveMsg.type;
						fileName = recvNewPacketBuffer->fileName;
						cout << "fileName :: " << recvNewPacketBuffer->fileName << endl;
						cout << "seqNum :: " << recvNewPacketBuffer->seqNum << endl;
						cout << "ackNum :: " << recvNewPacketBuffer->ackNum << endl;
						putOperation();
						return true;
					}
					else if (receivedSeqNum == recvNewPacketBuffer->seqNum)
					{
						// Else if it is the same old packet, discard the packet and retransmit the acknowledgement
						reTransCount = 0;
						dataPacketStore *checkBuffer = (dataPacketStore *)receiveMsg.buffer;
						cout << "Discarded duplicate Packet " << recvNewPacketBuffer->seqNum << endl;
						cout << "Re-Transmitting Packet " << checkBuffer->seqNum << endl;
						if (TRACE)
						{
							fout << "Discarded duplicate Packet " << recvNewPacketBuffer->seqNum << endl;
							fout << "Re-Transmitting Packet " << checkBuffer->seqNum << endl;
						}
						//resend three way handshake
						cout << "resent" << endl;

						if (sendMessage() == SOCKET_ERROR)
						{
							cout << "Socket Error occured while sending data " << endl;
							if (TRACE)
							{
								fout << "Socket Error occured while sending data " << endl;
							}
							closesocket(hostBSocket);
							return false;
						}
					}
				}
			}
		}
		else if (RetVal == 0)
		{
			cout << "retval : " << RetVal << endl;
			dataPacketStore *checkBuffer1 = (dataPacketStore *)sendMsg.buffer;
			// Check for Maximum Re-Transmission and once it reaches the specified count, Re-start the negotiation process
			reTransCount++;
			if (reTransCount == MAX_TRIES)
			{
				cout << "Max Re-Transmission reached, Waiting for Re-Negotiation " << endl;
				if (TRACE)
				{
					fout << "Max Re-Transmission reached, Waiting for Re-Negotiation " << endl;
				}
				return false;
			}
			cout << "Re-Transmitting Packet Seq Num " << checkBuffer1->seqNum << endl;
			if (TRACE)
			{
				fout << "Re-Transmitting Packet Seq Num " << checkBuffer1->seqNum << endl;
			}

			// Retransmit the unacknowledged packet
			if (sendMessage() == SOCKET_ERROR)
			{
				cout << "Socket Error occured while sending data " << endl;
				if (TRACE)
				{
					fout << "Socket Error occured while sending data " << endl;
				}
				closesocket(hostBSocket);
				return false;
			}
		}
	}
}


void main() {
	UdpHostB *tc = new UdpHostB("log.txt");

	while (1) {
		tc->start();
	}
}
