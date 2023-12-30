all: QRServer QRClient

QRServer: cpp/server.cpp
	g++ -o QRServer cpp/server.cpp

QRClient: cpp/client.cpp
	g++ -o QRClient cpp/client.cpp

.PHONY: clean

clean:
	rm -f QRServer QRClient *.o
