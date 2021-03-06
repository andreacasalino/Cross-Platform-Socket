/**
 * Author:    Andrea Casalino
 * Created:   01.28.2020
 *
 * report any bug to andrecasa91@gmail.com.
 **/

#include <async/AsyncTcpServer.h>

namespace sck::async {
    AsyncTcpServer::AsyncTcpServer(std::unique_ptr<tcp::TcpServer> server)
        : AsyncDecorator<listener::TcpServerListener>(std::move(server)) {
    };

    class AsyncTcpServer::AcceptanceService : public Service {
    public:
        AcceptanceService(AsyncTcpServer& server, listener::ErrorListener* list) 
            : Service([&server](){
                auto client = dynamic_cast<tcp::TcpServer*>(server.wrapped.get())->acceptClient();
                std::lock_guard<std::mutex> lk(server.listenerMtx);
                if(nullptr != server.listener) {
                    server.listener->handle(std::move(client));
                }
            }, list) {
        }
    };

    std::unique_ptr<Service> AsyncTcpServer::make_service() {
        std::lock_guard<std::mutex> lk(this->errorListenerMtx);
        return std::make_unique<AcceptanceService>(*this, this->errorListener);
    }
}
