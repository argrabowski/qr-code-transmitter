# QR Code Transmitter

## Checkpoint

The client is able to send its message containing the quick response code data to the server. The server is then able to decode that message, obtaining the URL.

## Final

Basic QR-Code transmission and response implemented. The client sends the size and binary QR-Code; the server receives it, decodes it, and replies with the URL of the image, which the client receives.

Multiple clients are supported via processes that address concurrency issues.

Error checking for methods and return codes are printed and logged in `adminLog.txt`. For example, passing in an image that is not a valid QR code will result in the appropriate error code being sent to the client.

Rate limiting is supported by specifying the number of requests allowed per second.

Command line arguments for the server can be in any order according to the format below, and the server's listen port is dynamic.

Timeouts are supported using the select system call, which waits on a socket to become active and logs a timeout error after the specified time.

Maximum users are implemented, as the server only accepts up to the maximum number of clients. If a client tries to connect while the maximum number of clients occupies the server, it will wait until a spot opens up and then get accepted.

After making the executable, the server can be run as follows:

```bash
./QRServer
```
or
```bash
./QRServer --PORT=<servPort> --RATE_MSGS<numReqs> --RATE_TIME<numSecs> --MAX_USERS<maxUsers> --TIMEOUT<timeOut>
```

In the first case, the default values are `servPort = 2012`, `numReqs = 3`, `numSecs = 60`, `maxUsers = 3`, and `timeOut = 80`. In the second case, one or more of the command line arguments can specify the port number, the number of requests per number of seconds, maximum users, and timeout time.

After making the executable, the client can be run as follows:

```bash
./QRClient <ipAdress> <port> <image> <numReqs>
```

All command line arguments must be specified, as `ipAdress` and `port` denote the server to connect to. The `image` argument is the path to the image that the client wants to decode. `numReqs` is how many times the client is going to request that image to be decoded (in order to test rate limiting).
