#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <string>
#include <fstream>
#include <sys/select.h>

// constants
#define RCVBUFSIZE 4
#define MAX_DIGITS 10

using namespace std;

void dieWithError(const char *errMsg);

int main(int argc, char *argv[])
{
    // socket and file variables
    int clntSock;
    struct sockaddr_in qrServAddr;
    char qrBuffer[RCVBUFSIZE];
    uint32_t bytesRcvd = 0;
    uint32_t fileSize;
    char fileBuffer[1024];
    string fileData;
    unsigned int fileDataLen;
    int connectRet;
    ssize_t sendRet;
    size_t readRet;

    // recieved from server variables
    char *retCodeBuffer;
    char *urlLenBuffer;
    char *urlBuffer;
    uint32_t retCode;
    uint32_t urlLen;
    char *url;

    // command line argument variables
    char *servIP;
    int servPort;
    char *filePath;
    int numRequs;

    // select variables
    fd_set recvfds;
    struct timeval tv;
    int selNum;
    int selRetVal;

    setbuf(stdout, NULL);

    // check command line args
    if (argc != 5)
    {
        printf("Error: 4 arguments expected but %d found!\n", argc);
        exit(1);
    }

    // set command line args
    servIP = argv[1];
    servPort = atoi(argv[2]);
    filePath = argv[3];
    numRequs = atoi(argv[4]);

    printf("\n");
    printf("Starting client with the following args:\n");
    printf("servIP=%s\n", servIP);
    printf("servPort=%d\n", servPort);
    printf("filePath=%s\n", filePath);

    // initialize socket
    clntSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clntSock < 0)
    {
        dieWithError("Error: socket() failed!");
    }

    memset(&qrServAddr, 0, sizeof(qrServAddr));
    qrServAddr.sin_family = AF_INET;
    qrServAddr.sin_addr.s_addr = inet_addr(servIP);
    qrServAddr.sin_port = htons(servPort);

    // connect to server
    connectRet = connect(clntSock, (struct sockaddr *)&qrServAddr, sizeof(qrServAddr));
    if (connectRet < 0)
    {
        dieWithError("Error: connect() failed!");
    }

    printf("\n");
    printf("Client sends the following data:\n");

    for (int loop1 = 0; loop1 < numRequs; loop1++)
    {
        bytesRcvd = 0;
        printf("running rate limit test loop %i\n", loop1);

        // send file size to server
        FILE *fd = fopen(filePath, "r");
        int fileDes = fileno(fd);
        off_t end = lseek(fileDes, 0, SEEK_END);
        off_t start = lseek(fileDes, 0, SEEK_SET);
        fileSize = end;
        sendRet = send(clntSock, &fileSize, RCVBUFSIZE, 0);
        if (sendRet != RCVBUFSIZE)
        {
            dieWithError("Error: send() failed!");
        }
        printf("fileSize=%u\n", fileSize);

        // send file data to server
        while (true)
        {
            readRet = read(fileDes, fileBuffer, 1023);
            if (readRet <= 0)
            {
                break;
            }
            sendRet = send(clntSock, fileBuffer, readRet, 0);
        }
        fclose(fd);
        // printf("fileBuffer(part)=%s\n", fileBuffer);

        printf("\n");
        printf("Client receives the following data:\n");

        // timeout logic
        FD_ZERO(&recvfds);
        FD_SET(clntSock, &recvfds);
        selNum = clntSock + 1;
        tv.tv_sec = 80;
        tv.tv_usec = 0;
        selRetVal = select(selNum, &recvfds, NULL, NULL, &tv);

        // recieve return code from server
        while (bytesRcvd < RCVBUFSIZE)
        {
            if (selRetVal == -1)
            {
                dieWithError("Error: select() failed!");
            }
            else if (selRetVal != 0) // do receive
            {
                retCodeBuffer = (char *)&retCode;
                retCodeBuffer += bytesRcvd;
                bytesRcvd += recv(clntSock, retCodeBuffer, RCVBUFSIZE - bytesRcvd, 0);
                if (bytesRcvd <= 0)
                {
                    dieWithError("Error: recv() failed!");
                }
            }
            else if (selRetVal == 0)
            {
                dieWithError("Time Out: The client timed out on recv()!");
            }
        }
        printf("retCode=%u\n", retCode);
        if (retCode == 3)
        {
            printf("retCode %u indicates server rate limit exceeded!\n", retCode);
        }

        if ((retCode != 1) && (retCode != 2) && (retCode != 3))
        {
            // timeout logic
            FD_ZERO(&recvfds);
            FD_SET(clntSock, &recvfds);
            selNum = clntSock + 1;
            tv.tv_sec = 80;
            tv.tv_usec = 0;
            selRetVal = select(selNum, &recvfds, NULL, NULL, &tv);

            // recieve url length from server
            bytesRcvd = 0;
            while (bytesRcvd < RCVBUFSIZE)
            {
                if (selRetVal == -1)
                {
                    dieWithError("Error: select() failed!");
                }
                else if (selRetVal != 0) // do receive
                {
                    urlLenBuffer = (char *)&urlLen;
                    urlLenBuffer += bytesRcvd;
                    bytesRcvd += recv(clntSock, urlLenBuffer, RCVBUFSIZE - bytesRcvd, 0);
                    if (bytesRcvd <= 0)
                    {
                        dieWithError("Error: recv() failed!");
                    }
                }
                else if (selRetVal == 0)
                {
                    dieWithError("Time Out: The client timed out on recv()!");
                }
            }
            printf("urlLen=%u\n", urlLen);

            // timeout logic
            FD_ZERO(&recvfds);
            FD_SET(clntSock, &recvfds);
            selNum = clntSock + 1;
            tv.tv_sec = 80;
            tv.tv_usec = 0;
            selRetVal = select(selNum, &recvfds, NULL, NULL, &tv);

            // recieve url from server
            bytesRcvd = 0;
            url = (char *)malloc(fileSize);
            while (bytesRcvd < urlLen)
            {
                if (selRetVal == -1)
                {
                    dieWithError("Error: select() failed!");
                }
                else if (selRetVal != 0) // do receive
                {
                    urlBuffer = url;
                    urlBuffer += bytesRcvd;
                    bytesRcvd += recv(clntSock, urlBuffer, urlLen - bytesRcvd, 0);
                    if (bytesRcvd <= 0)
                    {
                        dieWithError("Error: recv() failed!");
                    }
                }
                else if (selRetVal == 0)
                {
                    dieWithError("Time Out: The client timed out on recv()!");
                }
            }
            printf("url=%s\n", url);
        }
        sleep(2);
    }

    close(clntSock);
    exit(1);

    return 0;
}

// error logging function
void dieWithError(const char *errMsg)
{
    printf("\n");
    printf("%s\n", errMsg);
    exit(1);
}
