#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <stdlib.h>
#include "lobaro1/lobaro-coap/src/coap.h"

void debugPuts(char* s){
    printf("%s", s);
}

int generateRandom(){
    int r = rand();
    return r;
}

int rtc1HzCnt(){
    return time(NULL);
}




// Response handler function
CoAP_Result_t CoAP_RespHandler_fn(CoAP_Message_t* pRespMsg, CoAP_Message_t* pReqMsg, NetEp_t* sender)
{
    if(pRespMsg == NULL) {
        printf("CoAP message transmission failed after all retries (timeout) for MessageId %d", pReqMsg->MessageID);
        return COAP_OK;
    }

    printf("Got a reply for MiD: %d", pRespMsg->MessageID);
    CoAP_PrintMsg(pRespMsg);

    return COAP_OK;
}


void sendCoapMessage(SocketHandle_t sockHandle, NetEp_t* serverEndpoint)
{
    // Send a CoAP message
    CoAP_Result_t result = CoAP_StartNewRequest(
            REQ_GET,				// (CoAP_MessageCode_t)
            "/a",	// (char*)
            sockHandle,				// (SocketHandle_t)
            serverEndpoint,		// (NetEp_t*)
            CoAP_RespHandler_fn,	// The function that will be called
            // when the message gets a response
            // or fails to be sent
            0,					// Message data buffer (uint8_t *)
            0					// Message data length (size_t)
    );
}


bool CoAP_Posix_SendDatagram(SocketHandle_t socketHandle, NetPacket_t *pckt){
    int sock = (int)socketHandle;

    struct sockaddr_in addr;
    size_t sockaddrSize;
    if(pckt->remoteEp.NetType == IPV4){
        struct sockaddr_in *remote = &addr;
        remote->sin_family = AF_INET;
        remote->sin_port = htons(pckt->remoteEp.NetPort);
        //for(uint8_t i = 0; i < 4; i++)
        remote->sin_addr.s_addr = pckt->remoteEp.NetAddr.IPv4.u32;
        sockaddrSize = sizeof(struct sockaddr_in);
    }
    else
    {
        ERROR("Unsupported NetType : %d\n", pckt->remoteEp.NetType);
        return false;
    }

    int ret = sendto(sock, pckt->pData, pckt->size, 0, &addr, sockaddrSize);

    if(ret < 0)
        printf("Error");

    return ret > 0;
}

bool CoAP_Posix_CreateSocket(SocketHandle_t *handle, NetInterfaceType_t type){
    if(type == IPV4){
        int posixSocket = socket(AF_INET, SOCK_DGRAM, 0);

        if(posixSocket < 0) {
            printf("Error");
            return false;
        }

        CoAP_Socket_t *newSocket = AllocSocket();

        if(newSocket == NULL){
            ERROR("Could not allocate memory for new socket");
            close(socket);
            return false;
        }

        newSocket->Handle = posixSocket;
        newSocket->Tx = CoAP_Posix_SendDatagram;
        newSocket->Alive = true;
        *handle = posixSocket;
        return true;
    }
    else{
        ERROR("Unsupported net type %d", type);
    }
    return false;
}

static void coapClient_work_task()
{
#define RX_BUFFER_SIZE (MAX_PAYLOAD_SIZE + 127)
    static uint8_t rxBuffer[RX_BUFFER_SIZE];
    static SocketHandle_t socketHandle = 0;

    // Create the socket (network interface)
    if(!CoAP_Posix_CreateSocket(&socketHandle, IPV4)){
        // Handle this error
    }

    while(true){
        // First, read all the pending packets from the network
        // interface and transfer them to the coap library
        int res = 0;
        do {
            // Read from network interface (using Posix socket api)
            res = recv(socketHandle, rxBuffer, RX_BUFFER_SIZE, MSG_DONTWAIT);

            if(res > 0){
                printf("New packet received on interface, bytes read = %d", res);


                // Format the packet to the proper structure
                NetPacket_t pckt;
                memset(&pckt, 0, sizeof(pckt));
                pckt.pData = rxBuffer;
                pckt.size = res;
                //pckt.remoteEp = serverEp; // Wo kommt serverEp her?

                CoAP_HandleIncomingPacket(socketHandle, &pckt);
            }
        } while (res > 0);
	
	
	// Ziel definieren
        NetEp_t ziel;
        ziel.NetType = IPV4;
        NetAddr_IPv4_t ipv4;
        ipv4.u8[0] = 127;
        ipv4.u8[1] = 0;
        ipv4.u8[2] = 0;
        ipv4.u8[3] = 1;
        NetAddr_t netaddr;
        netaddr.IPv4 = ipv4;
        ziel.NetAddr = netaddr;
        ziel.NetPort = 5683;
        // Leere ACK senden an Ziel
	CoAP_SendEmptyAck(CoAP_GetNextMid(), socketHandle, ziel);
        CoAP_doWork();

    }
}



void main(void){
    srand(time(NULL)); // seed for random number

    CoAP_API_t api = {
            .malloc = malloc,
            .free = free,
            .rtc1HzCnt = rtc1HzCnt,
            .rand = generateRandom,
            .debugPuts = debugPuts,
    };

    CoAP_Init(api);


    while(true)
    {
        coapClient_work_task();
    }


    return 0;
}


