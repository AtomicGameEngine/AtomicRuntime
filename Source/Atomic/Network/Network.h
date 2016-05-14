//
// Copyright (c) 2008-2015 the Urho3D project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#pragma once

#include "../Container/HashSet.h"
#include "../Core/Object.h"
#include "../IO/VectorBuffer.h"
#include "../Network/Connection.h"
#include "../Network/MasterServerClient.h"

#include <kNet/IMessageHandler.h>
#include <kNet/INetworkServerListener.h>
#include <ThirdParty/kNet/include/kNet/EndPoint.h>

namespace Atomic
{

class HttpRequest;
class MemoryBuffer;
class Scene;

    /// MessageConnection hash function.
template <class T> unsigned MakeHash(kNet::MessageConnection* value)
{
    return ((unsigned)(size_t)value) >> 9;
}

/// %Network subsystem. Manages client-server communications using the UDP protocol.
class ATOMIC_API Network : public Object, public kNet::IMessageHandler, public kNet::INetworkServerListener
{
    OBJECT(Network);

public:
    /// Construct.
    Network(Context* context);
    /// Destruct.
    ~Network();

    /// Handle a kNet message from either a client or the server.
    virtual void HandleMessage
        (kNet::MessageConnection* source, kNet::packet_id_t packetId, kNet::message_id_t msgId, const char* data, size_t numBytes);
    /// Compute the content ID for a message.
    virtual u32 ComputeContentID(kNet::message_id_t msgId, const char* data, size_t numBytes);
    /// Handle a new client connection.
    virtual void NewConnectionEstablished(kNet::MessageConnection* connection);
    /// Handle a client disconnection.
    virtual void ClientDisconnected(kNet::MessageConnection* connection);

    /// Connect to a server using UDP protocol. Return true if connection process successfully started.
    bool Connect(const String& address, unsigned short port, Scene* scene, const VariantMap& identity = Variant::emptyVariantMap);

    bool ConnectSimple(const String& address, unsigned short port, Scene* scene);

    bool ConnectWithExistingSocket(kNet::Socket* existingSocket, Scene* scene);

    void ClientConnectToMaster(const String& address, unsigned short port);

    void ClientConnectToServerViaMaster(const String& serverId,
                                        const String& internalAddress, unsigned short internalPort,
                                        const String& externalAddress, unsigned short externalPort,
                                        Scene* scene);
    void RequestServerListFromMaster();

    /// Disconnect the connection to the server. If wait time is non-zero, will block while waiting for disconnect to finish.
    void Disconnect(int waitMSec = 0);
    /// Start a server on a port using UDP protocol. Return true if successful.
    bool StartServer(unsigned short port);

    /// Start a server on a port using UDP protocol. Return true if successful.
    /// Also register the server with the master server.
    bool StartServerAndRegisterWithMaster(unsigned short serverPort, const String& masterAddress, unsigned short masterPort, const String& serverName);

    /// Stop the server.
    void StopServer();
    /// Broadcast a message with content ID to all client connections.
    void BroadcastMessage(int msgID, bool reliable, bool inOrder, const VectorBuffer& msg, unsigned contentID = 0);
    /// Broadcast a message with content ID to all client connections.
    void BroadcastMessage
        (int msgID, bool reliable, bool inOrder, const unsigned char* data, unsigned numBytes, unsigned contentID = 0);
    /// Broadcast a remote event to all client connections.
    void BroadcastRemoteEvent(StringHash eventType, bool inOrder, const VariantMap& eventData = Variant::emptyVariantMap);
    /// Broadcast a remote event to all client connections in a specific scene.
    void BroadcastRemoteEvent
        (Scene* scene, StringHash eventType, bool inOrder, const VariantMap& eventData = Variant::emptyVariantMap);
    /// Broadcast a remote event with the specified node as a sender. Is sent to all client connections in the node's scene.
    void BroadcastRemoteEvent
        (Node* node, StringHash eventType, bool inOrder, const VariantMap& eventData = Variant::emptyVariantMap);
    /// Set network update FPS.
    void SetUpdateFps(int fps);
    /// Set simulated latency in milliseconds. This adds a fixed delay before sending each packet.
    void SetSimulatedLatency(int ms);
    /// Set simulated packet loss probability between 0.0 - 1.0.
    void SetSimulatedPacketLoss(float probability);
    /// Register a remote event as allowed to be received. There is also a fixed blacklist of events that can not be allowed in any case, such as ConsoleCommand.
    void RegisterRemoteEvent(StringHash eventType);
    /// Unregister a remote event as allowed to received.
    void UnregisterRemoteEvent(StringHash eventType);
    /// Unregister all remote events.
    void UnregisterAllRemoteEvents();
    /// Set the package download cache directory.
    void SetPackageCacheDir(const String& path);
    /// Trigger all client connections in the specified scene to download a package file from the server. Can be used to download additional resource packages when clients are already joined in the scene. The package must have been added as a requirement to the scene, or else the eventual download will fail.
    void SendPackageToClients(Scene* scene, PackageFile* package);
    /// Perform an HTTP request to the specified URL. Empty verb defaults to a GET request. Return a request object which can be used to read the response data.
    SharedPtr<HttpRequest> MakeHttpRequest
        (const String& url, const String& verb = String::EMPTY, const Vector<String>& headers = Vector<String>(),
            const String& postData = String::EMPTY);

    /// Return network update FPS.
    int GetUpdateFps() const { return updateFps_; }

    /// Return simulated latency in milliseconds.
    int GetSimulatedLatency() const { return simulatedLatency_; }

    /// Return simulated packet loss probability.
    float GetSimulatedPacketLoss() const { return simulatedPacketLoss_; }

    /// Return a client or server connection by kNet MessageConnection, or null if none exist.
    Connection* GetConnection(kNet::MessageConnection* connection) const;
    // Return the connection with the matching endpoint
    bool IsEndPointConnected(const kNet::EndPoint& endPoint) const;

    /// Return the connection to the server. Null if not connected.
    Connection* GetServerConnection() const;
    /// Return all client connections.
    Vector<SharedPtr<Connection> > GetClientConnections() const;
    /// Return whether the server is running.
    bool IsServerRunning() const;
    /// Return whether a remote event is allowed to be received.
    bool CheckRemoteEvent(StringHash eventType) const;

    /// Return the package download cache directory.
    const String& GetPackageCacheDir() const { return packageCacheDir_; }

    /// Process incoming messages from connections. Called by HandleBeginFrame.
    void Update(float timeStep);
    /// Send outgoing messages after frame logic. Called by HandleRenderUpdate.
    void PostUpdate(float timeStep);

    kNet::Network* GetKnetNetwork() { return network_; }

    unsigned int GetServerPort() { return serverPort_; }

private:
    /// Handle begin frame event.
    void HandleBeginFrame(StringHash eventType, VariantMap& eventData);
    /// Handle render update frame event.
    void HandleRenderUpdate(StringHash eventType, VariantMap& eventData);
    /// Handle server connection.
    void OnServerConnected();
    /// Handle server disconnection.
    void OnServerDisconnected();
    /// Reconfigure network simulator parameters on all existing connections.
    void ConfigureNetworkSimulator();
    void HandleClientConnected(StringHash eventType, VariantMap& eventData);


    /// kNet instance.
    kNet::Network* network_;
    /// Client's server connection.
    SharedPtr<Connection> serverConnection_;
    /// Server's client connections.
    HashMap<kNet::MessageConnection*, SharedPtr<Connection> > clientConnections_;
    /// Allowed remote events.
    HashSet<StringHash> allowedRemoteEvents_;
    /// Remote event fixed blacklist.
    HashSet<StringHash> blacklistedRemoteEvents_;
    /// Networked scenes.
    HashSet<Scene*> networkScenes_;
    /// Update FPS.
    int updateFps_;
    /// Simulated latency (send delay) in milliseconds.
    int simulatedLatency_;
    /// Simulated packet loss probability between 0.0 - 1.0.
    float simulatedPacketLoss_;
    /// Update time interval.
    float updateInterval_;
    /// Update time accumulator.
    float updateAcc_;
    /// Package cache directory.
    String packageCacheDir_;
    // MasterServerClient
    MasterServerClient masterServerClient_;
    // Server Port
    unsigned int serverPort_;
};

/// Register Network library objects.
void ATOMIC_API RegisterNetworkLibrary(Context* context);

}
