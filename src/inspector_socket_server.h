#ifndef SRC_INSPECTOR_SOCKET_SERVER_H_
#define SRC_INSPECTOR_SOCKET_SERVER_H_

#include "inspector_agent.h"
#include "inspector_socket.h"
#include "uv.h"

#include <map>
#include <string>
#include <vector>

#if !HAVE_INSPECTOR
#error("This header can only be used when inspector is enabled")
#endif

namespace node {
namespace inspector {

class Closer;
class SocketSession;
class ServerSocket;

class SocketServerDelegate {
 public:
  virtual void StartSession(int session_id, const std::string& target_id) = 0;
  virtual void EndSession(int session_id) = 0;
  virtual void MessageReceived(int session_id, const std::string& message) = 0;
  virtual std::vector<std::string> GetTargetIds() = 0;
  virtual std::string GetTargetTitle(const std::string& id) = 0;
  virtual std::string GetTargetUrl(const std::string& id) = 0;
  virtual void ServerDone() = 0;
};

// HTTP Server, writes messages requested as TransportActions, and responds
// to HTTP requests and WS upgrades.

class InspectorSocketServer {
 public:
  using ServerCallback = void (*)(InspectorSocketServer*);
  InspectorSocketServer(SocketServerDelegate* delegate,
                        uv_loop_t* loop,
                        const std::string& host,
                        int port,
                        FILE* out = stderr);
  ~InspectorSocketServer();

  // Start listening on host/port
  bool Start();

  // Called by the TransportAction sent with InspectorIo::Write():
  //   kKill and kStop
  void Stop(ServerCallback callback);
  //   kSendMessage
  void Send(int session_id, const std::string& message);
  //   kKill
  void TerminateConnections();
  //   kAcceptSession
  void AcceptSession(int session_id);
  //   kDeclineSession
  void DeclineSession(int session_id);

  int Port() const;

  // Server socket lifecycle. There may be multiple sockets
  void ServerSocketListening(ServerSocket* server_socket);
  void ServerSocketClosed(ServerSocket* server_socket);

  // Session connection lifecycle
  void Accept(int server_port, uv_stream_t* server_socket);
  bool HandleGetRequest(int session_id, const std::string& host,
                        const std::string& path);
  void SessionStarted(int session_id, const std::string& target_id,
                      const std::string& ws_id);
  void SessionTerminated(int session_id);
  void MessageReceived(int session_id, const std::string& message) {
    delegate_->MessageReceived(session_id, message);
  }
  SocketSession* Session(int session_id);

 private:
  void SendListResponse(InspectorSocket* socket, const std::string& host,
                        SocketSession* session);
  bool TargetExists(const std::string& id);

  enum class ServerState {kNew, kRunning, kStopping, kStopped};
  uv_loop_t* loop_;
  SocketServerDelegate* const delegate_;
  const std::string host_;
  int port_;
  std::string path_;
  std::vector<ServerSocket*> server_sockets_;
  Closer* closer_;
  std::map<int, std::pair<std::string, std::unique_ptr<SocketSession>>>
      connected_sessions_;
  int next_session_id_;
  FILE* out_;
  ServerState state_;

  friend class Closer;
};

}  // namespace inspector
}  // namespace node

#endif  // SRC_INSPECTOR_SOCKET_SERVER_H_
