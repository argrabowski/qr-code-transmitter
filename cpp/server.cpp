#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/time.h>
#include <fstream>
#include <iostream>
#include <time.h>
#include <netinet/tcp.h>
#include <getopt.h>

// constants
#define MAXPENDING 5
#define RCVBUFSIZE 4
#define MAXLINELEN 80

using namespace std;

void dieWithError(const char *errMsg);
void handleTCPClient(int clntSock, int total, uint32_t retCode, sockaddr_in qrClntAddr, char *ipAdress, int timeOut, int numSecs, int numReqs);
void INThandler(int sig);
string RandomFileName(int total);
void appendLineToFile(string line, string ipAdress);

int main(int argc, char *argv[])
{
    // socket variables
    int servSock;
    int clntSock;
    struct sockaddr_in qrServAddr;
    struct sockaddr_in qrClntAddr;
    unsigned int clntLen;
    int localAddr;
    int listenRet;

    // command line argument variables
    int servPort = 2012;
    int numReqs = 3;
    int numSecs = 60;
    int maxUsers = 3;
    int timeOut = 80;

    // concurrency variables
    int total = 0;
    int status;

    // select variables
    fd_set readfds;
    struct timeval tv;
    int selNum;
    int selRetVal;

    // return code variable
    uint32_t retCode = 0;

    // ip adress variables
    string noIp = "NO IP";
    char *ipAdress;

    char servPortChar[100];
    char numReqsChar[100];
    char numSecsChar[100];
    char maxUsersChar[100];
    char timeOutChar[100];
    char clntSockChar[100];
    char totalChar[100];

    int opt, optIndex;
    static struct option long_options[] = {
        {"PORT", optional_argument, 0, 'a'},
        {"RATE_MSGS", optional_argument, 0, 'b'},
        {"RATE_TIME", optional_argument, 0, 'c'},
        {"MAX_USERS", optional_argument, 0, 'd'},
        {"TIMEOUT", optional_argument, 0, 'e'},
    };

    while ((opt = getopt_long(argc, argv, ":a:b:c:d:e", long_options, NULL)) != -1)
    {
        switch (opt)
        {
        case 'a':
            servPort = atoi(optarg);
            break;
        case 'b':
            numReqs = atoi(optarg);
            break;
        case 'c':
            numSecs = atoi(optarg);
            break;
        case 'd':
            maxUsers = atoi(optarg);
            break;
        case 'e':
            timeOut = atoi(optarg);
            break;
        }
    }

    // admin log server startup
    printf("\n");
    printf("Starting server with the following args:\n");
    appendLineToFile("Starting server with the following args:", noIp);

    printf("servPort=%d\n", servPort);
    sprintf(servPortChar, "%d", servPort);
    string servPortStr = "servPort=";
    servPortStr += servPortChar;
    appendLineToFile(servPortStr, noIp);

    printf("numReqs=%u\n", numReqs);
    sprintf(numReqsChar, "%u", numReqs);
    string numReqsStr = "numReqs=";
    numReqsStr += numReqsChar;
    appendLineToFile(numReqsStr, noIp);

    printf("numSecs=%u\n", numSecs);
    sprintf(numSecsChar, "%u", numSecs);
    string numSecsStr = "numSecs=";
    numSecsStr += numSecsChar;
    appendLineToFile(numSecsStr, noIp);

    printf("maxUsers=%u\n", maxUsers);
    sprintf(maxUsersChar, "%u", maxUsers);
    string maxUsersStr = "maxUsers=";
    maxUsersStr += maxUsersChar;
    appendLineToFile(maxUsersStr, noIp);

    printf("timeOut=%u\n", timeOut);
    sprintf(timeOutChar, "%u", timeOut);
    string timeOutStr = "timeOut=";
    timeOutStr += timeOutChar;
    appendLineToFile(timeOutStr, noIp);

    // initialize socket
    servSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (servSock < 0)
    {
        retCode = 1;
        appendLineToFile("Error: socket() failed!", noIp);
        dieWithError("Error: socket() failed!");
    }

    // signal handler
    signal(SIGINT, INThandler);

    memset(&qrServAddr, 0, sizeof(qrServAddr));
    qrServAddr.sin_family = AF_INET;
    qrServAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    qrServAddr.sin_port = htons(servPort);

    // bind to local adress
    localAddr = bind(servSock, (struct sockaddr *)&qrServAddr, sizeof(qrServAddr));
    if (localAddr < 0)
    {
        retCode = 1;
        appendLineToFile("Error: bind() failed!", noIp);
        dieWithError("Error: bind() failed!");
    }

    // listen on port
    listenRet = listen(servSock, MAXPENDING);
    if (listenRet < 0)
    {
        retCode = 1;
        appendLineToFile("Error: listen() failed!", noIp);
        dieWithError("Error: listen() failed!");
    }

    // wait for client and accept
    for (;;)
    {
        FD_ZERO(&readfds);
        FD_SET(servSock, &readfds);
        selNum = servSock + 1;
        tv.tv_sec = 0;
        tv.tv_usec = 0;
        selRetVal = select(selNum, &readfds, NULL, NULL, &tv);
        if (selRetVal == -1)
        {
            retCode = 1;
            appendLineToFile("Error: select() failed!", noIp);
            dieWithError("Error: select() failed!");
        }
        if (selRetVal != 0 && total < maxUsers)
        {
            clntLen = sizeof(qrClntAddr);
            clntSock = accept(servSock, (struct sockaddr *)&qrClntAddr, &clntLen);
            if (clntSock < 0)
            {
                retCode = 1;
                appendLineToFile("Error: accept() failed!", noIp);
                dieWithError("Error: accept() failed!");
            }
            pid_t forkVal = fork();
            if (forkVal < 0)
            {
                retCode = 1;
                appendLineToFile("Error: fork() failed!", noIp);
                dieWithError("Error: fork() failed!");
            }
            if (forkVal == 0)
            {
                close(servSock);
                ipAdress = inet_ntoa(qrClntAddr.sin_addr);

                // log client handling
                printf("Handling client %s on socket %d!\n", ipAdress, clntSock);
                sprintf(clntSockChar, "%d", clntSock);
                string clntSockStr = "clntSock=";
                clntSockStr = "Handling client ";
                clntSockStr += ipAdress;
                clntSockStr += " on socket ";
                clntSockStr += clntSockChar;
                clntSockStr += "!";
                appendLineToFile(clntSockStr, ipAdress);

                // handle the client
                handleTCPClient(clntSock, total, retCode, qrClntAddr, ipAdress, timeOut, numSecs, numReqs);
                exit(0);
            }
            else
            {
                close(clntSock);
                total++;
                printf("numUsersConnected=%u\n", total);
                if (total == maxUsers)
                {
                    printf("Max Users: Maximum number of users (%d) reached!\n", maxUsers);
                    string maxUsersStr = "Max Users: Maximum number of users (";
                    maxUsersStr += maxUsersChar;
                    maxUsersStr += ") reached!";
                    appendLineToFile(maxUsersStr, noIp);
                }
                sprintf(totalChar, "%u", total);
                string totalStr = "numUsersConnected=";
                totalStr += totalChar;
                appendLineToFile(totalStr, noIp);
                memset(&totalChar[0], 0, sizeof(totalChar));
            }
        }
        else if (selRetVal == 0 || total >= maxUsers)
        {
            pid_t child = waitpid(-1, &status, WNOHANG);
            if (child != 0 && total > 0)
            {
                total--;
                printf("numUsersConnected=%u\n", total);
                sprintf(totalChar, "%u", total);
                string totalStr = "numUsersConnected=";
                totalStr += totalChar;
                appendLineToFile(totalStr, noIp);
                memset(&totalChar[0], 0, sizeof(totalChar));
            }
        }
    }

    return 0;
}

void handleTCPClient(int clntSock, int total, uint32_t retCode, sockaddr_in qrClntAddr, char *ipAdress, int timeOut, int numSecs, int numReqs)
{
    struct timeval rateStart;
    int attempts;

    for (;;)
    {

        // file and decoding variables
        uint32_t fileSize;
        uint32_t bytesRcvd = 0;
        char *fileSizeBuffer;
        char *fileBuffer;
        char *url;
        uint32_t urlLen;
        ssize_t sendRet;

        // select variables
        fd_set recvfds;
        struct timeval tv;
        int selNum;
        int selRetVal;

        printf("\n");
        printf("Server receives the following data:\n");
        appendLineToFile("Server receives the following data:", ipAdress);

        // timeout logic
        FD_ZERO(&recvfds);
        FD_SET(clntSock, &recvfds);
        selNum = clntSock + 1;
        tv.tv_sec = timeOut;
        tv.tv_usec = 0;
        selRetVal = select(selNum, &recvfds, NULL, NULL, &tv);

        // recieve file size from client
        while (bytesRcvd < RCVBUFSIZE)
        {
            if (selRetVal == -1)
            {
                retCode = 1;
                appendLineToFile("Error: select() failed!", ipAdress);
                dieWithError("Error: select() failed!");
            }
            else if (selRetVal != 0) // do receive
            {
                fileSizeBuffer = (char *)&fileSize;
                fileSizeBuffer += bytesRcvd;
                bytesRcvd += recv(clntSock, fileSizeBuffer, RCVBUFSIZE - bytesRcvd, 0);
                if (bytesRcvd <= 0)
                {
                    retCode = 1;
                    appendLineToFile("Error: recv() failed!", ipAdress);
                    dieWithError("Error: recv() failed!");
                }
            }
            else if (selRetVal == 0)
            {
                retCode = 2;
                appendLineToFile("Time Out: The server timed out on recv()!", ipAdress);
                dieWithError("Time Out: The server timed out on recv()!");
            }
        }

        struct timeval currentTime
        {
        };
        gettimeofday(&currentTime, nullptr);
        time_t currentMicroseconds = (currentTime.tv_sec * 1000) + (currentTime.tv_usec / 1000);
        time_t rateEndMicroseconds = (rateStart.tv_sec * 1000) + (rateStart.tv_usec / 1000) + (numSecs * 1000);
        if (currentMicroseconds < rateEndMicroseconds)
        {
            if (attempts < numReqs)
            {
                attempts++;
            }
            else
            {
                retCode = 3;
                int sr = send(clntSock, &retCode, sizeof(retCode), 0);
                setsockopt(clntSock, IPPROTO_TCP, TCP_NODELAY, (void *)1, sizeof(int));
                setsockopt(clntSock, IPPROTO_TCP, TCP_NODELAY, 0, sizeof(int));

                if (sr != sizeof(retCode))
                {
                    retCode = 1;
                    dieWithError("Error: send() failed!");
                    appendLineToFile("Error: send() failed!", ipAdress);
                }

                appendLineToFile("Error: Rate limit exceeded!", ipAdress);
                printf("Error: Rate limit exceeded!\n");
                sleep(1);
                continue;
            }
        }
        else
        {
            rateStart = currentTime;
            attempts = 0;
        }

        // log fileSize
        printf("fileSize=%u\n", fileSize);
        char fileSizeChar[50];
        sprintf(fileSizeChar, "%d", fileSize);
        string fileSizeStr = "fileSize=";
        fileSizeStr += fileSizeChar;
        appendLineToFile(fileSizeStr, ipAdress);

        // check buffer overflow
        if (fileSize > (1024 * 1024 * 1024))
        {
            retCode = 1;
            appendLineToFile("Error: Too many bytes recieved!", ipAdress);
            dieWithError("Error: Too many bytes recieved!");
        }

        // timeout logic
        FD_ZERO(&recvfds);
        FD_SET(clntSock, &recvfds);
        selNum = clntSock + 1;
        tv.tv_sec = 0;
        tv.tv_usec = 0;
        selRetVal = select(selNum, &recvfds, NULL, NULL, &tv);

        // recieve file data from client
        bytesRcvd = 0;
        char *buffer = (char *)malloc(fileSize);
        while (bytesRcvd < fileSize)
        {
            if (selRetVal == -1)
            {
                retCode = 1;
                appendLineToFile("Error: select() failed!", ipAdress);
                dieWithError("Error: select() failed!");
            }
            else if (selRetVal != 0) // do receive
            {
                fileBuffer = buffer;
                fileBuffer += bytesRcvd;
                bytesRcvd += recv(clntSock, fileBuffer, fileSize - bytesRcvd, 0);
                if (bytesRcvd <= 0)
                {
                    retCode = 1;
                    appendLineToFile("Error: recv() failed!", ipAdress);
                    dieWithError("Error: recv() failed!");
                }
            }
            else if (selRetVal == 0)
            {
                retCode = 2;
                appendLineToFile("Time Out: The server timed out on recv()!", ipAdress);
                dieWithError("Time Out: The server timed out on recv()!");
            }
        }

        // log beginning of fileBuffer
        // printf("fileBuffer(part)=%s\n", fileBuffer);
        string fileBufferStr = "fileBuffer(beginning)=";
        fileBufferStr += fileBuffer;
        // appendLineToFile(fileBufferStr, ipAdress);

        // printf("\n");
        printf("Server sends the following data:\n");
        appendLineToFile("Server sends the following data:", ipAdress);

        // log retCode
        printf("retCode=%u\n", retCode);
        char retCodeChar[50];
        sprintf(retCodeChar, "%u", retCode);
        string retCodeStr = "retCode=";
        retCodeStr += retCodeChar;
        appendLineToFile(retCodeStr, ipAdress);

        if (retCode != 1)
        {
            // write to new file
            string fileName = RandomFileName(total);
            printf("fileName=%s\n", fileName.c_str());
            printf("\n");
            FILE *fd = fopen(fileName.c_str(), "w");
            if (!fd)
            {
                retCode = 1;
                dieWithError("Error: fopen() failed!");
                appendLineToFile("Error: fopen() failed!", ipAdress);
            }
            int fileDes = fileno(fd);
            write(fileDes, buffer, fileSize);
            fclose(fd);
            free(buffer);

            // decode image file
            string command = "java -cp jar/javase.jar:jar/core.jar com.google.zxing.client.j2se.CommandLineRunner ";
            command += fileName;
            FILE *output = popen(command.c_str(), "r");
            if (!output)
            {
                retCode = 1;
                dieWithError("Error: popen() failed!");
                appendLineToFile("Error: popen() failed!", ipAdress);
            }
            char outBuffer[MAXLINELEN];
            for (int i = 0; i < 4; i++)
            {
                fgets(outBuffer, sizeof(outBuffer), output);
            }
            url = outBuffer;
            urlLen = strlen(url);
            pclose(output);

            // log urlLen
            printf("urlLen=%u\n", urlLen);
            char urlLenChar[50];
            sprintf(urlLenChar, "%u", urlLen);
            string urlLenStr = "urlLen=";
            urlLenStr += urlLenChar;
            appendLineToFile(urlLenStr, ipAdress);

            // log url
            printf("url=%s\n", url);
            string urlStr = "url=";
            urlStr += url;
            if (urlStr.find("No barcode found") != string::npos)
            {
                retCode = 1;
                printf("Error: no barcode found!\n");
                appendLineToFile("Error: no barcode found!", ipAdress);
            }
            else
            {
                appendLineToFile(urlStr, ipAdress);
            }
        }

        // send return code to client
        sendRet = send(clntSock, &retCode, RCVBUFSIZE, 0);
        if (sendRet != RCVBUFSIZE)
        {
            retCode = 1;
            dieWithError("Error: send() failed!");
            appendLineToFile("Error: send() failed!", ipAdress);
        }

        if (retCode != 1)
        {
            // send url length to client
            sendRet = send(clntSock, &urlLen, RCVBUFSIZE, 0);
            if (sendRet != RCVBUFSIZE)
            {
                retCode = 1;
                dieWithError("Error: send() failed!");
                appendLineToFile("Error: send() failed!", ipAdress);
            }

            // send url to client
            sendRet = send(clntSock, url, urlLen, 0);
            if (sendRet != urlLen)
            {
                retCode = 1;
                dieWithError("Error: send() failed!");
                appendLineToFile("Error: send() failed!", ipAdress);
            }
        }
        bytesRcvd = 0;
    }

    exit(1);
}

// error logging function
void dieWithError(const char *errMsg)
{
    printf("%s\n", errMsg);
    printf("\n");
    exit(1);
}

void INThandler(int sig)
{
    char c;
    signal(sig, SIG_IGN);
    close(4);
    exit(0);
}

string RandomFileName(int total)
{
    string str = "012345679";
    string newStr = "img/";
    newStr += to_string(total);
    int pos;
    srand(time(NULL));
    while (newStr.size() != 10)
    {
        pos = (rand() % (str.size() - 1));
        newStr += str.substr(pos, 1);
    }
    newStr += ".png";
    return newStr;
}

void appendLineToFile(string line, string ipAdress)
{
    // time variables
    time_t rawTime;
    struct tm *timeInfo;
    char timeBuffer[80];

    time(&rawTime);
    timeInfo = localtime(&rawTime);
    strftime(timeBuffer, 80, "%F %T", timeInfo);
    string timeStr = timeBuffer;

    ofstream file;
    file.open("adminLog.txt", ios::out | ios::app);
    if (file.fail())
    {
        appendLineToFile("Error: open() failed!", ipAdress);
        dieWithError("Error: open() failed!");
    }
    file << timeStr << " "
         << "[" << ipAdress << "]"
         << " " << line << endl;
}
