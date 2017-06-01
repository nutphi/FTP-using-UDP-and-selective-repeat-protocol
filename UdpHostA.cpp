/*************************************************************************************
*								 File Name	: UdpHostA.cpp		   			 	     *
*	Usage : Sends request to Server for Uploading, downloading and listing of files  *
**************************************************************************************/
#define _CRT_SECURE_NO_WARNINGS
#include "UdpHostA.h"

/**
* Function - startClient
* Usage: Initialize WinSocket and get the host name and Peer IP Address to connnect
*
* @arg: void
*/
void UdpHostA::startClient(char *logFile)
{
	HOSTENT* hp;
	/* Initialize WinSocket */
	if (WSAStartup(0x0202, &wsaData) != 0)
	{
		WSACleanup();
		cerr << "Error in starting WSAStartup()";
		return;
	}

	/* Open the log file for logging the datas being transmitted */
	fout.open(logFile);

	/* Get Host Name */
	if (gethostname(hostName, HOSTNAME_LENGTH) != 0)
	{
		cerr << "can not get the host name,program ";
		return;
	}

	cout << "UDP Host A starting on host: " << hostName << endl;

	cout << "Type name of Receiver: " << endl;
	getline(cin, serverIpAdd);

	/* UDP Socket Creation */
	if ((hostASocket = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET)
	{
		cerr << "Create UDP Socket failed.. " << endl;
		return;
	}

	/* Get the Host Name */
	if ((hp = gethostbyname(serverIpAdd.c_str())) == NULL)
	{
		cerr << "Get server name failed.." << endl;
		return;
	}

	//************************* This binding is optional ************
	memset(&listenAddr, 0, sizeof(listenAddr));
	listenAddr.sin_family = AF_INET;
	listenAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	listenAddr.sin_port = htons(RECEIVE_PORT);

	// Binding the Socket to the Listening Port Number 
	if (bind(hostASocket, (struct sockaddr *) &listenAddr, sizeof(listenAddr)) < 0)
	{
		cerr << "Socket Binding Error,exit" << endl;
		closesocket(hostASocket);
		return;
	}
	//*************************************************

	memset(&destAddr, 0, sizeof(destAddr));
	memcpy(&destAddr.sin_addr, hp->h_addr, hp->h_length);
	destAddr.sin_family = hp->h_addrtype;
	destAddr.sin_port = htons(REQUEST_PORT);
}

/**
* Function - run
* Usage: Based on the user selected option invokes the appropriate function
*
* @arg: void
*/
void UdpHostA::run()
{
	/* Based on the Selected option invoke the appropriate function */
	if (strcmp(transferType.c_str(), "get") == 0)
	{
		cin.ignore();
		/* Before retrieving the file, display the list of files available in other peer */
		/* Initiate file retrieval */
		selectedReqType = REQ_GET;
		cout << endl << "Type name of file to be retrieved: " << endl;
		getline(cin, fileName);
		doOperation();
	}
	else if (strcmp(transferType.c_str(), "put") == 0)
	{
		cin.ignore();
		/* Before retrieving the file, display the list of files available in other peer */
		/* Initiate file retrieval */
		selectedReqType = REQ_PUT;
		cout << endl << "Type name of file to be retrieved: " << endl;
		getline(cin, fileName);

		char path[] = "file\\";
		char* file_ = new char[strlen(path) + strlen(fileName.c_str())];
		memcpy(file_, path, sizeof(path));
		strcat(file_, fileName.c_str());
		fileToRead.open(file_, ios::in | ios::binary);
		struct _stat stat_buf;
		int result = _stat(file_, &stat_buf);
		if (result == -1)
		{
			cout << fileName << " not found" << endl;
			fileToRead.close();
		}
		else {
			doOperation();
		}
	}
	else if (strcmp(transferType.c_str(), "list") == 0) {
		selectedReqType = REQ_LIST;
		doOperation();
	}
	else
	{
		cerr << "Wrong request type";
		return;
	}

}

int UdpHostA::sendNewGetMessage() {

	bool isEnd = fileToRead.eof();
	if (isReadEndOfFile) {
		return 0;
	}
	else if (!isEnd && windowslide.size() < WINDOWS_SIZE) {
		dataPacket.seqNum = sequence;
		if (isFirstOfFile) {
			isFirstOfFile = false;
			dataPacket.ackNum = ((dataToTransfer *)receiveMsg.buffer)->seqNum;
		}
		else {
			dataPacket.ackNum = -1;
		}
		memset(dataPacket.dataBuffer, '\0', DATA_BUFFER_LENGTH);
		fileToRead.read(dataPacket.dataBuffer, DATA_BUFFER_LENGTH);
		windowslide.push_back(sequence);
		buffer2 = new char[DATA_BUFFER_LENGTH];
		memcpy(buffer2, dataPacket.dataBuffer, sizeof(dataPacket.dataBuffer));
		databuffer.push_back(buffer2);
		if (sendMessage() == SOCKET_ERROR) {
			return SOCKET_ERROR;
		}
		nextSequence();
	}
	else if (isEnd) {
		buffer2 = new char[DATA_BUFFER_LENGTH];
		dataPacket.seqNum = sequence;
		dataPacket.ackNum = -1;
		windowslide.push_back(sequence);
		memset(dataPacket.dataBuffer, '\0', DATA_BUFFER_LENGTH);
		strncpy(dataPacket.dataBuffer, "END OF FILE", sizeof("END OF FILE"));
		memset(buffer2, '\0', DATA_BUFFER_LENGTH);
		memcpy(buffer2, dataPacket.dataBuffer, sizeof(dataPacket.dataBuffer));
		databuffer.push_back(buffer2);
		nextSequence();
		isReadEndOfFile = true;
		fileToRead.close();
		if (sendMessage() == SOCKET_ERROR) {
			return SOCKET_ERROR;
		}
	}
	return 0;
}

/**
* Function - ResolveName
* Usage: Returns the binary, network byte ordered address
*
* @arg: string
*/
unsigned long UdpHostA::ResolveName(string name)
{
	struct hostent *host;            /* Structure containing host information */

	if ((host = gethostbyname(name.c_str())) == NULL)
	{
		cerr << "gethostbyname() failed" << endl;
		return(1);
	}

	/* Return the binary, network byte ordered address */
	return *((unsigned long *)host->h_addr_list[0]);
}
void UdpHostA::nextSequence() {
	sequence = (sequence + 1) % SEQUENCE_SIZE;
}


int UdpHostA::sendMessage() {
	cout << "client -> server" << endl;
	cout << "seq :: " << dataPacket.seqNum << endl;
	cout << "ack :: " << dataPacket.ackNum << endl;
	cout << "type :: " << sendMsg.type << endl;

	if (TRACE)
	{
		fout << "client ----> server" << endl;
		fout << "sequence number::" << dataPacket.seqNum << endl;
		fout << "acknowledge number::" << dataPacket.ackNum << endl;
	}
	cout << ":: dataBuffer :: " << endl;
	cout << dataPacket.dataBuffer << endl;
	memset(sendMsg.buffer, '\0', DATA_BUFFER_LENGTH);
	memcpy(sendMsg.buffer, &dataPacket, sizeof(dataPacket));
	return sendto(hostASocket, (char *)&sendMsg, BUFFER_LENGTH, 0, (SOCKADDR*)&destAddr, sizeof(destAddr));
}

int UdpHostA::receiveMessage() {
	int destAddrSize = sizeof(destAddr);
	//memset(&receiveMsg, '\0', sizeof(receiveMsg));
	memset(receiveMsg.buffer, '\0', DATA_BUFFER_LENGTH);
	int result = recvfrom(hostASocket, (char *)&receiveMsg, BUFFER_LENGTH, 0, (SOCKADDR*)&destAddr, &destAddrSize);

	if (TRACE)
	{
		fout << "client <---- server" << endl;
		fout << "sequence number::" << ((dataToTransfer *)receiveMsg.buffer)->seqNum << endl;
		fout << "acknowledge number::" << ((dataToTransfer *)receiveMsg.buffer)->ackNum << endl;
	}
	cout << "client < - server" << endl;
	cout << "seq :: " << ((dataToTransfer *)receiveMsg.buffer)->seqNum << endl;
	cout << "ack :: " << ((dataToTransfer *)receiveMsg.buffer)->ackNum << endl;
	cout << "type :: " << receiveMsg.type << endl;
	//cout << "dataBuffer :: " << ((dataToTransfer *)receiveMsg.buffer)->dataBuffer << endl;
	return result;
}

void UdpHostA::putOperation() {
	//nothing
}

void UdpHostA::getOperation() {
	windowslide.clear();
	databuffer.clear();
	vector<char*>::iterator it_databuffer;
	vector<int>::iterator it_windowslide;
	int reTransCount = 0;
	//first sequence on windows
	sequence = 0;
	fileName = string("file\\") + fileName;
	ofstream myFile(fileName, ios::out | ios::binary);

	struct timeval *tp = new timeval;
	tp->tv_sec = 0;
	tp->tv_usec = TIMEOUT_USEC;
	int RetVal = 0;
	int destAddrLen = sizeof(destAddr);
	bool isFirstReceived = true;
	buffer = new char[DATA_BUFFER_LENGTH];
	/* Handle the first packets */
	receivedPacket = (dataToTransfer *)receiveMsg.buffer; // This line needs to be changed as per your implementation
														  //if the sequence number is not the first windowslide

	int length = receivedPacket->seqNum - sequence;
	for (int i = 0; i < length; i++) {

		buffer = new char[DATA_BUFFER_LENGTH];
		memset(buffer, '\0', DATA_BUFFER_LENGTH);
		transSeqNum = (sequence + i) % SEQUENCE_SIZE;
		windowslide.push_back(transSeqNum);
		databuffer.push_back(buffer);

	}
	if (receivedPacket->seqNum == 0) {
		nextSequence();
	}
	transSeqNum = receivedPacket->seqNum;
	windowslide.push_back(transSeqNum);
	buffer = new char[DATA_BUFFER_LENGTH];
	memcpy(buffer, receivedPacket->dataBuffer, DATA_BUFFER_LENGTH);
	databuffer.push_back(buffer);
	dataPacket.seqNum = -1;
	dataPacket.ackNum = receivedPacket->seqNum;
	memcpy(sendMsg.buffer, &dataPacket, sizeof(dataPacket));


	bool isReceiveEndOfFile = false;
	bool isCloseFile = false;
	if (sendMessage() == SOCKET_ERROR) {
		cout << "error" << endl;
		return;
	}
	else {
		cout << "pass" << endl;
	}
	while (1)
	{

		FD_ZERO(&readfds);
		FD_SET(hostASocket, &readfds);
		if ((RetVal = select(1, &readfds, NULL, NULL, tp)) == SOCKET_ERROR)	/* Select call waits for the specified time */
		{
			cerr << "Timer error!" << endl;
			return;
		}
		else if (RetVal>0)	/* There are incoming packets */
		{
			if (FD_ISSET(hostASocket, &readfds))	/* Incoming packet from peer host 1 */
			{
				if ((numBytesRecv = receiveMessage()) == SOCKET_ERROR)
				{
					//Socket Error
					return;
				}
				else
				{
					//Handle the received packet
					receivedPacket = (dataToTransfer *)receiveMsg.buffer; // This line needs to be changed as per your implementation
					it_windowslide = find(windowslide.begin(), windowslide.end(), receivedPacket->seqNum);
					if (strcmp(receivedPacket->dataBuffer, "No such file") == 0 || strcmp(receivedPacket->dataBuffer, " ") == 0)
					{
						//Handle no such file exists scenario					
						return;
					}
					else
					{
						/* If the received packet is a new packet, Check the sequence number and Pack and send the acknowledgement */
						if (it_windowslide != windowslide.end()) {
							//if the same sequence number, send ack back
							//because server lost syn with file message before but client already get it
							dataPacket.seqNum = -1;
							dataPacket.ackNum = receivedPacket->seqNum;
							sendMsg.type = receiveMsg.type;
							memcpy(sendMsg.buffer, &dataPacket, sizeof(dataPacket));
							if (sendMessage() == SOCKET_ERROR) {
								cout << "error" << endl;
								return;
							}
							else {
								cout << "pass" << endl;
							}
						}
						else if (receivedPacket->seqNum == sequence) {
							//if it is the first on windowslide
							// send only the first packet
							transSeqNum = receivedPacket->seqNum;
							windowslide.push_back(transSeqNum);

							buffer = new char[DATA_BUFFER_LENGTH];
							memcpy(buffer, receivedPacket->dataBuffer, DATA_BUFFER_LENGTH);
							databuffer.push_back(buffer);
							dataPacket.seqNum = -1;
							dataPacket.ackNum = receivedPacket->seqNum;
							sendMsg.type = receiveMsg.type;

							if (databuffer.size() != 0 && databuffer[0] != NULL&&strcmp(databuffer.front(), "END OF FILE") == 0) {
								isReceiveEndOfFile = true;
								strncpy(dataPacket.dataBuffer, databuffer[0], sizeof("END OF FILE"));
							}
							else if (databuffer.size() != 0 && databuffer[0] != NULL) {
								strncpy(dataPacket.dataBuffer, "ACK", sizeof("ACK"));
							}

							while (databuffer.size() != 0 && databuffer[0] != NULL && !isCloseFile) {
								if (strcmp(databuffer[0], "END OF FILE") == 0) {
									myFile.close();
									nextSequence();
									cout << "A file complete" << endl;
									isCloseFile = true;
								}
								else {
									myFile.write(databuffer[0], DATA_BUFFER_LENGTH);
									nextSequence();
								}
								databuffer.erase(databuffer.begin());
								windowslide.erase(windowslide.begin());
							}
							if (isFirstReceived) {
								isFirstReceived = false;
								sequence--;
							}
							memcpy(sendMsg.buffer, &dataPacket, sizeof(dataPacket));
							if (sendMessage() == SOCKET_ERROR) {
								cout << "error" << endl;
								return;
							}
							else {
								cout << "pass" << endl;
							}
							if (isCloseFile) {
								break;
							}
						}
						else {
							//if the sequence number is not the first windowslide
							if (receivedPacket->seqNum > sequence && (sequence + WINDOWS_SIZE - 1) < receivedPacket->seqNum) {
								//if between windows side but not new loop ex. [6][0][1]
								int length = (receivedPacket->seqNum - sequence) - databuffer.size();
								for (int i = 0; i < length; i++) {
									buffer = new char[DATA_BUFFER_LENGTH];
									memset(buffer, '\0', DATA_BUFFER_LENGTH);
									transSeqNum = (sequence + i) % SEQUENCE_SIZE;
									windowslide.push_back(transSeqNum);
									databuffer.push_back(buffer);

								}
								transSeqNum = receivedPacket->seqNum;
								windowslide.push_back(transSeqNum);
								buffer = new char[DATA_BUFFER_LENGTH];

								memcpy(buffer, receivedPacket->dataBuffer, DATA_BUFFER_LENGTH);
								databuffer.push_back(buffer);


								dataPacket.seqNum = -1;
								dataPacket.ackNum = receivedPacket->seqNum;

								memcpy(sendMsg.buffer, &dataPacket, sizeof(dataPacket));
								if (sendMessage() == SOCKET_ERROR) {
									cout << "error" << endl;
									return;
								}
								else {
									cout << "pass" << endl;
								}
							}
							else if (receivedPacket->seqNum < (sequence + WINDOWS_SIZE) % SEQUENCE_SIZE) {
								//go back to 0
								// add null value to the last of sequence
								int length = ((sequence + WINDOWS_SIZE) % SEQUENCE_SIZE) + receivedPacket->seqNum - databuffer.size();
								for (int i = 0; i < length; i++) {
									buffer = new char[DATA_BUFFER_LENGTH];
									memset(buffer, '\0', DATA_BUFFER_LENGTH);

									transSeqNum = (sequence + i) % SEQUENCE_SIZE;
									windowslide.push_back(transSeqNum);
									databuffer.push_back(buffer);


								}
								transSeqNum = receivedPacket->seqNum;
								windowslide.push_back(transSeqNum);
								buffer = new char[DATA_BUFFER_LENGTH];
								memset(buffer, '\0', DATA_BUFFER_LENGTH);
								memcpy(buffer, receivedPacket->dataBuffer, sizeof(receivedPacket->dataBuffer));
								databuffer.push_back(buffer);


								dataPacket.seqNum = -1;
								dataPacket.ackNum = receivedPacket->seqNum;
								sendMsg.type = receiveMsg.type;
								memcpy(sendMsg.buffer, &dataPacket, sizeof(dataPacket));
								if (sendMessage() == SOCKET_ERROR) {
									cout << "error" << endl;
									return;
								}
								else {
									cout << "pass" << endl;
								}
							}
							else {
								//resend out of order
								dataPacket.seqNum = -1;
								dataPacket.ackNum = receivedPacket->seqNum;
								memcpy(sendMsg.buffer, &dataPacket, sizeof(dataPacket));
								if (sendMessage() == SOCKET_ERROR) {
									cout << "error" << endl;
									return;
								}
								else {
									cout << "pass" << endl;
								}
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
			while (databuffer.size() != 0 && databuffer[0] != NULL) {
				if (strcmp(databuffer[0], "eof") == 0) {
					myFile.close();
					nextSequence();
					cout << "B file complete" << endl;
				}
				else {
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

bool UdpHostA::initiateNegotiation() {

	int RetVal;
	dataToTransfer *recvNewPacketBuffer;
	int countOfTimeOutAck = 0;
	int countOfDuplicateAck = 0;
	srand((unsigned int)time(NULL));
	rand();
	int client_no_request = rand() % 255;
	int randSeqNum = (rand() + 100) % 255;
	int currentPackSeqNum = 0;
	int reTransCount = 0;
	struct timeval *tp = new timeval;
	tp->tv_sec = 0;
	tp->tv_usec = TIMEOUT_USEC;

	/* Store the recevied sequence number for future communication */

	memset(sendMsg.buffer, '\0', sizeof(sendMsg.buffer));
	memset(dataPacket.dataBuffer, '\0', sizeof(dataPacket.dataBuffer));
	dataPacket.ackNum = -1;
	dataPacket.seqNum = -1;

	/* Acknowledge the Received packet and Generate
	and Send the Response Sequence Number */
	dataPacket.seqNum = ((randSeqNum) % 2 == 1 ? randSeqNum + 1 : randSeqNum);

	//sendPacketBuffer.ackNum = receivedSeqNum;
	currentPackSeqNum = dataPacket.seqNum;

	//strcpy(sendPacketBuffer.dataBuffer, "ACK");
	memcpy(sendMsg.buffer, &dataPacket, sizeof(dataPacket));
	sendMsg.length = sizeof(sendMsg.buffer);

	if (sendMessage() == SOCKET_ERROR)
	{
		cout << "Socket Error occured while sending data " << endl;
		if (TRACE)
		{
			fout << "Socket Error occured while sending data " << endl;
		}
		closesocket(hostASocket);
		return true;
	}

	//cout << "Sent Response Seq Num " << dataPacket.seqNum << endl;
	if (TRACE)
	{
		fout << "Sent Response Seq Num " << dataPacket.seqNum << endl;
	}

	/* Wait for Acknowledgement, Break the loop once Negotiation is successful */
	while (1)
	{
		FD_ZERO(&readfds);
		FD_SET(hostASocket, &readfds);
		RetVal = select(1, &readfds, NULL, NULL, tp);

		if (RetVal == SOCKET_ERROR)	/* Check for incoming packets */
		{

			cerr << "Timer error!" << endl;
			return true;
		}
		else if (RetVal>0)	/* There are incoming packets */
		{

			if (FD_ISSET(hostASocket, &readfds))	/* Incoming packet from peer host 1 */
			{
				if (receiveMessage() == SOCKET_ERROR)
				{
					cerr << "Get buffer error!" << endl;
					return false;
				}
				else
				{
					recvNewPacketBuffer = (dataToTransfer *)receiveMsg.buffer;

					/* If the received packet is acknowledgement of previously transmitted packet, Negotiation is successfull */
					if ((currentPackSeqNum == recvNewPacketBuffer->ackNum) && (!(strcmp(recvNewPacketBuffer->dataBuffer, "ACK"))))
					{
						//selectedReqType = receiveMsg.type;
						currentPackSeqNum = currentPackSeqNum % 7;
						//transSeqNum = recvNewPacketBuffer->seqNum;
						cout << "Ack received for Handshake " << endl;
						if (TRACE)
						{
							fout << "Ack received for Handshake " << endl;
						}

						//to send last packet for ack to server 3 way handshake with get/put/list operation

						transSeqNum = currentPackSeqNum;
						dataPacket.seqNum = currentPackSeqNum;
						dataPacket.ackNum = recvNewPacketBuffer->seqNum;
						sendMsg.length = sizeof(dataPacket);
						sendMsg.type = selectedReqType;
						if (sendMsg.type == REQ_GET || sendMsg.type == REQ_PUT) {
							memcpy(dataPacket.fileName, fileName.c_str(), strlen(fileName.c_str()));
							if (sendMsg.type == REQ_PUT) {
								//the last one will send to server after this function
								isFirstOfFile = true;
								return true;
							}
						}

						memcpy(dataPacket.dataBuffer, "ACK", sizeof("ACK"));

						memcpy(sendMsg.buffer, &dataPacket, sizeof(dataPacket));
						if (sendMessage() == SOCKET_ERROR)
						{
							cout << "Socket Error occured while sending data " << endl;
							if (TRACE)
							{
								fout << "Socket Error occured while sending data " << endl;
							}
							closesocket(hostASocket);

							return false;
						}
						return true;
					}
					else if (transSeqNum == recvNewPacketBuffer->seqNum)
					{
						/* Else if it is the same old packet, discard the packet and retransmit the acknowledgement */

						reTransCount = 0;
						dataToTransfer *checkBuffer = (dataToTransfer *)receiveMsg.buffer;
						cout << "Discarded duplicate Packet " << recvNewPacketBuffer->seqNum << endl;
						cout << "Re-Transmitting Packet " << checkBuffer->seqNum << endl;
						if (TRACE)
						{
							fout << "Discarded duplicate Packet " << recvNewPacketBuffer->seqNum << endl;
							fout << "Re-Transmitting Packet " << checkBuffer->seqNum << endl;
						}
						if (sendMessage() == SOCKET_ERROR)
						{
							cout << "Socket Error occured while sending data " << endl;
							if (TRACE)
							{
								fout << "Socket Error occured while sending data " << endl;
							}
							closesocket(hostASocket);
							return false;
						}
					}
				}
			}
		}
		else if (RetVal == 0)
		{
			dataToTransfer *checkBuffer = (dataToTransfer *)sendMsg.buffer;
			/* Check for Maximum Re-Transmission and once it reaches the specified count, Re-start the negotiation process */
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
			cout << "Re-Transmitting Packet Seq Num " << checkBuffer->seqNum << endl;
			if (TRACE)
			{
				fout << "Re-Transmitting Packet Seq Num " << checkBuffer->seqNum << endl;
			}

			/* Retransmit the unacknowledged packet */

			if (sendMessage() == SOCKET_ERROR)
			{
				cout << "Socket Error occured while sending data " << endl;
				if (TRACE)
				{
					fout << "Socket Error occured while sending data " << endl;
				}
				closesocket(hostASocket);
				return false;
			}
		}
	}
	return false;
}

/**
* Function - doOperation
* Usage: Establish connection and retrieve file from server
*
* @arg: void
*/
void UdpHostA::doOperation()
{
	windowslide.clear();
	databuffer.clear();
	vector<char*>::iterator it_databuffer;
	vector<int>::iterator it_windowslide;

	int RetVal;
	bool connectionStatus = false;
	int waitCount = 0;
	int ReceiveBytes = 0;
	int EffectiveBytes = 0;
	int PacketCount = 0;
	//for put operation
	isReadEndOfFile = false;

	/* File Descriptor to be used with Socket */
	fd_set readfds;



	/* Initialize and Set the Timer Value */
	struct timeval *tp = new timeval;
	tp->tv_sec = 0;
	tp->tv_usec = TIMEOUT_USEC; // Macro Value 300000

	/* Initiate Negotiation and Start the transmission or reception
	only if negotiation is successful */
	do
	{
		/* Ensure that 3 Way handshake is successful before initiating the tranmission */
		connectionStatus = initiateNegotiation();
	} while (connectionStatus == false);
	cout << "finish 3 way handshake" << endl;
	/* Handle the incoming packets */

	if (sendMsg.type == REQ_PUT) {
		cout << "Have file" << endl;
		sequence = 0;
		while (windowslide.size()<WINDOWS_SIZE&&!isReadEndOfFile)
		{
			if (sendNewGetMessage() == SOCKET_ERROR) {
				//if there is socket error
				cout << "server error" << endl;
				return;
			}
		}
	}
	//for one packet back
	//inside there are get and put file operation functions 
	while (1)
	{
		FD_ZERO(&readfds);
		FD_SET(hostASocket, &readfds);
		if ((RetVal = select(1, &readfds, NULL, NULL, tp)) == SOCKET_ERROR)	/* Select call waits for the specified time */
		{
			cerr << "Timer error!" << endl;
			return;
		}
		else if (RetVal>0)	/* There are incoming packets */
		{

			if (FD_ISSET(hostASocket, &readfds))	/* Incoming packet from peer host 1 */
			{
				if ((numBytesRecv = receiveMessage()) == SOCKET_ERROR)
				{
					//Socket Error
					return;
				}
				else
				{
					//Handle the received packet
					receivedPacket = (dataToTransfer *)receiveMsg.buffer; // This line needs to be changed as per your implementation
																		  //in case get file
					if (strcmp(receivedPacket->dataBuffer, "No such file") == 0)
					{
						//Handle no such file exists scenario
						cout << "no this file" << endl;
						dataPacket.ackNum = receivedPacket->seqNum;
						dataPacket.seqNum = -1;
						memcpy(dataPacket.fileName, receivedPacket->fileName, sizeof(receivedPacket->fileName));
						memcpy(dataPacket.dataBuffer, receivedPacket->dataBuffer, sizeof(receivedPacket->dataBuffer));

						memcpy(sendMsg.buffer, &dataPacket, sizeof(dataPacket));

						if (sendMessage() == SOCKET_ERROR)
						{
							cout << "Socket Error occured while sending data " << endl;
							if (TRACE)
							{
								fout << "Socket Error occured while sending data " << endl;
							}
							closesocket(hostASocket);
							return;
						}
						else
						{
							cout << "get ::" << string(dataPacket.dataBuffer) << endl;
							return;
						}
						return;
					}
					else
					{
						if (sendMsg.type == REQ_GET) {
							cout << "has a file" << endl;
							getOperation();
							return;
						}
						//test put
						else if (sendMsg.type == REQ_PUT) {
							if ((strcmp(receivedPacket->dataBuffer, "END OF FILE") == 0)) {
								cout << "finish put operation" << endl;
								return;
							}
							else {
								receivedPacket = (dataToTransfer *)receiveMsg.buffer;
								it_windowslide = find(windowslide.begin(), windowslide.end(), receivedPacket->ackNum);
								if (it_windowslide != windowslide.end()) {
									//if an ack from client is the same as seq on windowslide
									if (windowslide[0] == receivedPacket->ackNum) {
										if (receivedPacket->dataBuffer == NULL) {
											//it means the last packet end of file
											break;
										}
										//if the first windowslide
										do {

											windowslide.erase(windowslide.begin());
											databuffer.erase(databuffer.begin());
											if (!isReadEndOfFile) {
												// if the server did not send eof
												while (databuffer.size() > 0 && databuffer[0] == NULL) {
													databuffer.erase(databuffer.begin());
													windowslide.erase(windowslide.begin());
													if (sendNewGetMessage() == SOCKET_ERROR) {
														//if there is socket error
														return;
													}
												}
												if (sendNewGetMessage() == SOCKET_ERROR) {
													//if there is socket error
													return;
												}
											}
										} while (databuffer.size() != 0 && databuffer[0] == NULL);
										//buffer
										cout << "" << endl;
									}
									else {
										//if not the first windowslide
										//set '/0'
										for (int i = 0; i < windowslide.size(); i++) {
											if (receivedPacket->ackNum == windowslide[i]) {
												databuffer[i] = '\0';
											}
										}
									}
								}
							}
						}
						else if (sendMsg.type == REQ_LIST) {
							cout << "list ::" << string(receivedPacket->dataBuffer) << endl;

							dataPacket.ackNum = receivedPacket->seqNum;
							dataPacket.seqNum = -1;
							memcpy(sendMsg.buffer, &dataPacket, sizeof(dataPacket));
							if (sendMessage() == SOCKET_ERROR)
							{
								cout << "Socket Error occured while sending data " << endl;
								if (TRACE)
								{
									fout << "Socket Error occured while sending data " << endl;
								}
								closesocket(hostASocket);
								return;
							}
							else
							{
								cout << "list ::" << string(dataPacket.dataBuffer) << endl;
								if (TRACE)
								{
									fout << "---send list----" << endl;
								}
								return;
							}

						}
						/* If the received packet is a new packet, Check the sequence number and Pack and send the acknowledgement */

						/* Multiple Scenarios possible, Check for duplicate packet too and handle the duplicate packet scenario */
					}
				}
			}
		}
		else if (RetVal == 0)
		{
			// This line needs to be changed as per your implementation
			if (sendMsg.type == REQ_LIST || (sendMsg.type == REQ_GET&&strcmp(dataPacket.dataBuffer, "no such file"))) {


				//if ((receivedPacket->seqNum == transSeqNum) && (!(strcmp(dataPacket.dataBuffer, "ACK")))) {
				if (sendMsg.type == REQ_GET) {
					cout << "FileName = " << dataPacket.fileName << endl;
				}
				cout << "sendMsgType" << sendMsg.type << endl;
				memcpy(sendMsg.buffer, &dataPacket, sizeof(dataPacket));
				if (sendMessage() == SOCKET_ERROR)
				{
					cout << "Socket Error occured while sending data " << endl;
					if (TRACE)
					{
						fout << "Socket Error occured while sending data " << endl;
					}
					closesocket(hostASocket);
				}
				cout << "end" << endl;

				//return;
				//}
			}
			else if (sendMsg.type == REQ_PUT) {
				if (databuffer.size() == 0) {
					return;
				}
				for (int i = 0; i < databuffer.size(); i++) {
					if (databuffer[i] != NULL) {

						dataPacket.seqNum = windowslide[i];
						
						dataPacket.ackNum = -1;
						if (((dataToTransfer *)receiveMsg.buffer)->seqNum != -1 && i == 0)
							dataPacket.ackNum = ((dataToTransfer *)receiveMsg.buffer)->seqNum;
						memcpy(dataPacket.dataBuffer, databuffer[i], DATA_BUFFER_LENGTH);
						if (sendMessage() == SOCKET_ERROR) {
							cerr << "error" << endl;
							return;
						}
					}
				}
			}
			/* If no packet arrives for specified time limit, Reinitiate the process - For Get operation */


			/* If Put Operation - Implement the retransmission of data packet */

		}
	}
}

/**
* Function - showMenu
* Usage: Display the Menu with options for the User to select based on the operation
*
* @arg: void
*/
void UdpHostA::showMenu()
{
	int optionVal;
	string listFile = "";
	cout << "1 : GET " << endl;
	cout << "2 : PUT " << endl;
	cout << "3 : LIST " << endl;
	cout << "6 : EXIT " << endl;
	cout << "Please select the operation that you want to perform : ";
	/* Check if invalid value is provided and reset if cin error flag is set */
	if (!(cin >> optionVal))
	{
		cout << endl << "Input Types does not match " << endl;
		cin.clear();
		cin.ignore(250, '\n');
	}
	/* Based on the option selected by User, set the transfer type and invoke the appropriate function */
	switch (optionVal)
	{
	case 1:
		transferType = "get";
		run();
		break;
	case 2:

		transferType = "put";
		run();
		break;
	case 3:
		cout << "1 : CLIENT LIST SIDE" << endl;
		cout << "2 : SERVER LIST SIDE" << endl;
		cout << "Please select the side that you want to perform : ";
		if (!(cin >> optionVal))
		{
			cout << endl << "Input Types does not match " << endl;
			cin.clear();
			cin.ignore(250, '\n');
		}

		switch (optionVal)
		{
		case 1:
			HANDLE find;
			WIN32_FIND_DATA data;

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
			cout << listFile.c_str() << endl;
			break;
		case 2:
			transferType = "list";
			run();
			break;
		default:
			cout << "Please select from one of the above options " << endl;
			break;
		}
		break;
	case 6:
		cout << "Terminating... " << endl;
		exit(1);
		break;

	default:
		cout << "Please select from one of the above options " << endl;
		break;
	}
	cout << endl;
}

/**
* Destructor - ~UdpHostA
* Usage: DeAllocate the allocated memory
*
* @arg: void
*/
UdpHostA::~UdpHostA()
{
	closesocket(hostASocket);
	/* When done uninstall winsock.dll (WSACleanup()) and return; */
	WSACleanup();

}

/**
* Function - main
* Usage: Initiates the Client
*
* @arg: int, char*
*/
int main(int argc, char *argv[])
{
	UdpHostA *tc = new UdpHostA();

	tc->startClient();
	while (1)
	{
		tc->showMenu();
	}

	return 0;
}
