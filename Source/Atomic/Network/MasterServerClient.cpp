//
// Created by Keith Johnston on 4/16/16.
//

#include "MasterServerClient.h"
#include "../Precompiled.h"
#include <vector>
#include <inttypes.h>
#include "../IO/Log.h"
#include "../Network/NetworkPriority.h"
#include "../Network/Network.h"
#include "../Core/Profiler.h"
#include "../Network/NetworkEvents.h"

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>

namespace Atomic
{

MasterServerClient::MasterServerClient(Context *context) :

        readingMasterMessageLength(true),
        Object(context),
        udpSecondsTillRetry_(0.5f),
        connectToMasterState_(MASTER_NOT_CONNECTED),
        clientConnectToGameServerState_(GAME_NOT_CONNECTED),
        timeBetweenClientPunchThroughAttempts_(1.0),
        timeTillNextPunchThroughAttempt_(0.0),
        timeBetweenClientConnectAttempts_(1.0),
        timeTillNextClientConnectAttempt_(1.0),
        masterTCPConnection_(NULL),
        clientToServerSocket_(NULL),
        clientPendingScene_(NULL)
{

}

MasterServerClient::~MasterServerClient()
{
    if (masterTCPConnection_)
    {
        masterTCPConnection_->Disconnect();
    }
}

void MasterServerClient::ConnectToMaster(const String &address, unsigned short port) {
    PROFILE(ConnectToMaster);

    // TODO - support dns names too
    sscanf(address.CString(), "%hu.%hu.%hu.%hu",
           (unsigned short *) &masterEndPoint_.ip[0],
           (unsigned short *) &masterEndPoint_.ip[1],
           (unsigned short *) &masterEndPoint_.ip[2],
           (unsigned short *) &masterEndPoint_.ip[3]);

    masterEndPoint_.port = port;

    Atomic::Network* network = GetSubsystem<Network>();
    kNet::Network* kNetNetwork = network->GetKnetNetwork();

    kNet::NetworkServer* server = kNetNetwork->GetServer().ptr();

    if (server)
    {
        std::vector < kNet::Socket * > listenSockets = kNetNetwork->GetServer()->ListenSockets();

        int numSockets = listenSockets.size();

        kNet::Socket *listenSocket = listenSockets[0];

        String foo = String(listenSocket->LocalEndPoint().ToString().c_str());
        LOGINFO(foo);

        // Create a UDP and a TCP connection to the master server
        // UDP connection re-uses the same udp port we are listening on for the sever
        masterUDPConnection_ = new kNet::Socket(listenSocket->GetSocketHandle(),
                                                listenSocket->LocalEndPoint(),
                                                listenSocket->LocalAddress(), masterEndPoint_, "",
                                                kNet::SocketOverUDP, kNet::ServerClientSocket, 1400);
    }
    else
    {
        masterUDPConnection_ = kNetNetwork->CreateUnconnectedUDPSocket(address.CString(),port);
    }

    // We first make a TCP connection
    connectToMasterState_ = MASTER_CONNECTING_TCP;

    masterTCPConnection_ = kNetNetwork->ConnectSocket(address.CString(), port, kNet::SocketOverTCP);

    SendMessageToMasterServer("{ \"cmd\": \"connectRequest\" }");
}

void MasterServerClient::RequestServerListFromMaster()
{
    SendMessageToMasterServer("{ \"cmd\": \"getServerList\" }");
}

void MasterServerClient::ConnectToServerViaMaster(const String &serverId,
                                                  const String &internalAddress, unsigned short internalPort,
                                                  const String &externalAddress, unsigned short externalPort,
                                                  Scene* scene)
{
    clientConnectToGameServerState_ = GAME_CONNECTING_INTERNAL_IP;

    /*
    String msg = String("{") +
                 String("\"cmd\":") + String("\"requestIntroduction\",") +
                 String("\"id\":\"") + masterServerConnectionId_ + String("\", ") +
                 String("\"serverId\":\"") + serverId + String("\"") +
                 String("}");

    SendMessageToMasterServer(msg);
    */

    kNet::EndPoint serverEndPoint;
    sscanf(internalAddress.CString(), "%hu.%hu.%hu.%hu",
           (unsigned short *) &serverEndPoint.ip[0],
           (unsigned short *) &serverEndPoint.ip[1],
           (unsigned short *) &serverEndPoint.ip[2],
           (unsigned short *) &serverEndPoint.ip[3]);

    serverEndPoint.port = internalPort;

    clientPendingScene_ = scene;
    clientToServerSocket_ = new kNet::Socket(masterUDPConnection_->GetSocketHandle(),
                                       masterUDPConnection_->LocalEndPoint(),
                                       masterUDPConnection_->LocalAddress(), serverEndPoint, "",
                                       kNet::SocketOverUDP, kNet::ClientConnectionLessSocket, 1400);

    Atomic::Network* network = GetSubsystem<Network>();
    network->ConnectWithExistingSocket(clientToServerSocket_, clientPendingScene_);
}

void MasterServerClient::RegisterServerWithMaster(const String &name)
{
    // Get the internal IP and port
    kNet::EndPoint localEndPoint = masterTCPConnection_->LocalEndPoint();

    unsigned char* ip = localEndPoint.ip;

    char str[256];
    sprintf(str, "%d.%d.%d.%d", (unsigned int)ip[0], (unsigned int)ip[1], (unsigned int)ip[2], (unsigned int)ip[3]);

    Atomic::Network* network = GetSubsystem<Network>();
    unsigned int localPort = network->GetServerPort();

    String msg = String("{") +
                 String("\"cmd\":") + String("\"registerServer\",") +
                 String("\"internalIP\": \"") + String(str) + String("\", ") +
                 String("\"internalPort\": ") + String(localPort) + String(", ") +
                 String("\"id\":\"") + masterServerConnectionId_ + String("\", ") +
                 String("\"serverName\":\"") + name + String("\"") +
                 String("}");

    SendMessageToMasterServer(msg);
}

void MasterServerClient::SendMessageToMasterServer(const String& msg)
{
    if (masterTCPConnection_)
    {
        String netString = String(msg.Length()) + ':' + msg;
        masterTCPConnection_->Send(netString.CString(), netString.Length());
    }
    else
    {
        LOGERROR("No master server connection. Cannot send message");
    }
}

void MasterServerClient::Update(float dt) {
    if (masterTCPConnection_==NULL)
    {
        return;
    }

    kNet::OverlappedTransferBuffer *buf = masterTCPConnection_->BeginReceive();

    if (buf && buf->bytesContains > 0) {

        int n = 0;
        int totalBytes = buf->bytesContains;

        while (n < totalBytes) {
            char c = buf->buffer.buf[n++];

            // Are we still reading in the length?
            if (readingMasterMessageLength) {
                if (c == ':') {
                    sscanf(masterMessageLengthStr.CString(), "%" SCNd32, &bytesRemainingInMasterServerMessage_);
                    readingMasterMessageLength = false;

                    LOGINFO("Message is "+String(bytesRemainingInMasterServerMessage_)+" long");
                }
                else {
                    masterMessageLengthStr += c;
                }
            }
            else {
                // Are we reading in the string?

                masterMessageStr += c;
                bytesRemainingInMasterServerMessage_--;

                // Did we hit the end of the string?
                if (bytesRemainingInMasterServerMessage_ == 0) {
                    HandleMasterServerMessage(masterMessageStr);
                    readingMasterMessageLength = true;
                    masterMessageLengthStr = "";
                    masterMessageStr = "";
                }
            }
        }

        masterTCPConnection_->EndReceive(buf);
    }

    if (connectToMasterState_ == MASTER_CONNECTING_UDP)
    {
        ConnectUDP(dt);
    }

    // Do we have clients requesting a punchthrough?
    if (clientIdToPunchThroughSocketMap_.Size()>0)
    {
        if (timeTillNextPunchThroughAttempt_ <= 0)
        {
            for (HashMap<String, kNet::Socket*>::Iterator i = clientIdToPunchThroughSocketMap_.Begin(); i != clientIdToPunchThroughSocketMap_.End();)
            {
                Atomic::Network* network = GetSubsystem<Network>();
                LOGINFO("Sending packet to client");
                kNet::Socket* s = i->second_;

                if (network->IsEndPointConnected(s->RemoteEndPoint()))
                {
                    i = clientIdToPunchThroughSocketMap_.Erase(i);
                }
                else
                {
                    s->Send("K",1);
                    ++i;
                }
            }

            // Reset the timer
            timeTillNextPunchThroughAttempt_ = timeBetweenClientPunchThroughAttempts_;
        }

        timeTillNextPunchThroughAttempt_ -= dt;
    }

    if (clientToServerSocket_ != NULL && timeTillNextClientConnectAttempt_ <= 0)
    {
        Atomic::Network* network = GetSubsystem<Network>();

        if (!network->GetServerConnection() || !network->GetServerConnection()->IsConnected())
        {
            LOGINFO("Sending packet to server");
            clientToServerSocket_->Send("K",1);
        }

        timeTillNextClientConnectAttempt_ = timeBetweenClientConnectAttempts_;
    }

    timeTillNextClientConnectAttempt_ -= dt;

}

void MasterServerClient::ConnectUDP(float dt)
{
    if (udpConnectionSecondsRemaining_ <= 0)
    {
        connectToMasterState_ = MASTER_CONNECTION_FAILED;

        // TODO - emit error event

        return;
    }

    if (udpSecondsTillRetry_ <= 0)
    {
        String msg = "{ \"cmd\": \"registerUDPPort\", \"id\": \"" + masterServerConnectionId_ + "\"}";

        masterUDPConnection_->Send(msg.CString(), msg.Length());
        udpSecondsTillRetry_ = 0.5;
    }
    else
    {
        udpSecondsTillRetry_ -= dt;
        udpConnectionSecondsRemaining_ -= dt;
    }
}

void MasterServerClient::HandleMasterServerMessage(const String &msg)
{
    LOGINFO("Got master server message: " + msg);

    rapidjson::Document document;
    if (document.Parse<0>(msg.CString()).HasParseError())
    {
        LOGERROR("Could not parse JSON data from string");
        return;
    }

    String cmd = document["cmd"].GetString();

    bool sendEvent = false;

    if (cmd == "connectTCPSuccess")
    {
        // TCP connection success - now try connecting UDP
        connectToMasterState_ = MASTER_CONNECTING_UDP;
        udpSecondsTillRetry_ = 0;
        udpConnectionSecondsRemaining_ = 5.0;
        masterServerConnectionId_ = document["id"].GetString();

        LOGINFO("TCP Connected");
    }
    else if (cmd == "connectUDPSuccess")
    {
        connectToMasterState_ = MASTER_CONNECTED;

        LOGINFO("UDP Connected");

        SendEvent(E_MASTERCONNECTIONREADY);
    }
    else if (cmd == "sendPacketToClient")
    {
        String clientIP = document["clientIP"].GetString();
        int clientPort = document["clientPort"].GetInt();

        LOGINFO("Got request to send packet to client at "+clientIP+":"+String(clientPort));

        kNet::EndPoint clientEndPoint;

        sscanf(clientIP.CString(), "%hu.%hu.%hu.%hu",
               (unsigned short *) &clientEndPoint.ip[0],
               (unsigned short *) &clientEndPoint.ip[1],
               (unsigned short *) &clientEndPoint.ip[2],
               (unsigned short *) &clientEndPoint.ip[3]);


        clientEndPoint.port = clientPort;

        // Create a socket that goes out the same port we are listening on to the client.
        // This will be used until the client actually connects.
        kNet::Socket* s = new kNet::Socket(masterUDPConnection_->GetSocketHandle(),
                                           masterUDPConnection_->LocalEndPoint(),
                                           masterUDPConnection_->LocalAddress(), clientEndPoint, "",
                                           kNet::SocketOverUDP, kNet::ServerClientSocket, 1400);

        String clientId = document["clientId"].GetString();
        clientIdToPunchThroughSocketMap_.Insert(MakePair(clientId, s));
    }
    else if (cmd == "serverList")
    {
        sendEvent = true;
    }

    if (sendEvent)
    {
        using namespace NetworkMessage;

        VariantMap& eventData = GetEventDataMap();
        eventData[P_DATA] = msg;
        SendEvent(E_MASTERMESSAGE, eventData);
    }
}


}