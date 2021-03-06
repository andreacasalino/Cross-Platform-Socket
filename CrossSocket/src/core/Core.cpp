/**
 * Author:    Andrea Casalino
 * Created:   01.28.2020
 *
 * report any bug to andrecasa91@gmail.com.
 **/

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <netdb.h>
#endif
#include "Core.h"
#include <Error.h>

namespace sck {
#ifdef _WIN32
   std::size_t Handler::SocketHandlerFactory::handlerCounter = 0;
   std::mutex Handler::SocketHandlerFactory::handlerCounterMtx;

   void Handler::SocketHandlerFactory::beforeOpen() {
       std::lock_guard<std::mutex> hndLck(handlerCounterMtx);
       ++handlerCounter;
       if (1 == handlerCounter) {
       // first socket opened
           WSADATA wsa;
           WSAStartup(MAKEWORD(2, 0), &wsa);
       }
   }

   void Handler::SocketHandlerFactory::afterClose() {
       std::lock_guard<std::mutex> hndLck(handlerCounterMtx);
       --handlerCounter;
       if (0 == handlerCounter) {
       // last socket closed
           WSACleanup();
       }
   }
#endif

   int getLastErrorCode() {
#ifdef _WIN32
      return WSAGetLastError();
#else
      return static_cast<int>(errno);
#endif
   }

   void throwWithCode(const std::string& what){
      throw Error(what, " , error code: ", getLastErrorCode());
   }

   std::unique_ptr<SocketIp4> convertIpv4(const sck::Ip& address) {
      auto tryConversion = [](SocketIp4& recipient, const sck::Ip& address) -> bool {
   #if !defined(_WIN32)
         in_addr ia;
         if (1 == ::inet_pton(AF_INET, address.getHost().c_str(), &ia)) {
            recipient.sin_addr.s_addr = ia.s_addr;
            return true;
         }
   #endif

         addrinfo* res, hints = addrinfo{};
         hints.ai_family = AF_INET;
         hints.ai_socktype = SOCK_STREAM;

         int gai_err = ::getaddrinfo(address.getHost().c_str(), NULL, &hints, &res);

   #if !defined(_WIN32)
         if (gai_err == EAI_SYSTEM) {
            return false;
         }
   #endif
         if (gai_err != 0) {
            return false;
         }

         auto ipv4 = reinterpret_cast<sockaddr_in*>(res->ai_addr);
         ::freeaddrinfo(res);
         recipient.sin_addr.s_addr = ipv4->sin_addr.s_addr;
         return true;
      };

      if (sck::Family::IP_V4 == address.getFamily()) {
         std::unique_ptr<SocketIp4> resolved = std::make_unique<SocketIp4>();
         // set everything to 0 first
         ::memset(&(*resolved), 0, sizeof(SocketIp4));
         resolved->sin_family = AF_INET;
         if (!tryConversion(*resolved, address)) {
            return nullptr;
         }
         resolved->sin_port = htons(address.getPort());
         return resolved;
      }
      return nullptr;
   }

   std::unique_ptr<SocketIp6> convertIpv6(const sck::Ip& address) {
         auto tryConversion = [](SocketIp6& recipient, const sck::Ip& address) -> bool {
   #if !defined(_WIN32)
         in6_addr ia;
         if (1 == ::inet_pton(AF_INET6, address.getHost().c_str(), &ia)) {
            recipient.sin6_addr = ia;
            return true;
         }
   #endif

         addrinfo* res, hints = addrinfo{};
         hints.ai_family = AF_INET6;
         hints.ai_socktype = SOCK_STREAM;

         int gai_err = ::getaddrinfo(address.getHost().c_str(), NULL, &hints, &res);

   #if !defined(_WIN32)
         if (gai_err == EAI_SYSTEM) {
            return false;
         }
   #endif
         if (gai_err != 0) {
            return false;
         }

         auto ipv6 = reinterpret_cast<sockaddr_in6*>(res->ai_addr);
         ::freeaddrinfo(res);
         recipient.sin6_addr = ipv6->sin6_addr;
         return true;
      };

      if (sck::Family::IP_V6 == address.getFamily()) {
         std::unique_ptr<SocketIp6> resolved = std::make_unique<SocketIp6>();
         // set everything to 0 first
         ::memset(&(*resolved), 0, sizeof(SocketIp6));
         resolved->sin6_family = AF_INET6;
         resolved->sin6_flowinfo = 0;
         if (!tryConversion(*resolved, address)) {
            return nullptr;
         }
         resolved->sin6_port = htons(address.getPort());
         return resolved;
      }
      return nullptr;
   }

   IpPtr convert(const SocketIp& address) {
      // refer to https://stackoverflow.com/questions/11684008/how-do-you-cast-sockaddr-structure-to-a-sockaddr-in-c-networking-sockets-ubu
      std::string ip;      
      std::uint16_t port;
      if (AF_INET == address.sa_family) {
         // ipv4 address
         // inet_ntoa is deprecated, but using inet_ntop for ipv4 address, leads to an ip that has no sense
         ip = std::string(::inet_ntoa(reinterpret_cast<const SocketIp4*>(&address)->sin_addr));
         port = ntohs(reinterpret_cast<const SocketIp4*>(&address)->sin_port);
      }
      else {
         // ipv6 address
         char temp[INET6_ADDRSTRLEN]; //this is the longest one
         // refer to https://www.gnu.org/software/libc/manual/html_node/Host-Address-Functions.html
         ::memset(temp, 0, INET6_ADDRSTRLEN);
         ::inet_ntop(address.sa_family, &address, temp, INET6_ADDRSTRLEN);
         ip = std::string(temp, INET6_ADDRSTRLEN);
         port = ntohs(reinterpret_cast<const SocketIp6*>(&address)->sin6_port);
      }
      return sck::Ip::create(ip, port);
   }

   Handler::~Handler() {
      if(this->opened){
         this->close();
      }
   }

   Handler::Handler()
      : opened(false)
      , hndl(SCK_INVALID_SOCKET) {
   }

   Handler::Handler(const SocketHandler& hndl)
       : opened(true)
       , hndl(hndl) {
#ifdef _WIN32
       SocketHandlerFactory::beforeOpen();
#endif
   }

   void Handler::open(const Protocol& type, const Family& family) {
      if(this->opened) return;
#ifdef _WIN32
      SocketHandlerFactory::beforeOpen();
#endif
      auto toIntFamily = [](const Family& family) -> int {
         switch (family) {
         case sck::Family::IP_V4:
            return AF_INET;
         case sck::Family::IP_V6:
            return AF_INET6;
         default:
            throw Error("unknown address family type");
         }
         throw Error("unknown address family type");
      };

      switch (type) {
      case Protocol::TCP: 
         this->hndl = ::socket(toIntFamily(family), SOCK_STREAM, 0);
         if (this->hndl == SCK_INVALID_SOCKET) {
            this->close();
            throwWithCode("Stream socket could not be created");
         }         
         break;
      case Protocol::UDP:
         this->hndl = ::socket(toIntFamily(family), SOCK_DGRAM, 0);
         if (this->hndl == SCK_INVALID_SOCKET) {
            this->close();
            throwWithCode("DataGram socket could not be created");
         }        
         break;
      default:
         throw Error("unknown protocol type");
      }

      this->opened = true;
   }

   void Handler::close() {
      if (!this->opened) return;
#ifdef _WIN32
      shutdown(this->hndl, 2);
      closesocket(this->hndl);
#else 
      ::shutdown(this->hndl, SHUT_RDWR);
      ::close(this->hndl);
#endif
      this->opened = false;
      this->hndl = SCK_INVALID_SOCKET;
#ifdef _WIN32
      SocketHandlerFactory::afterClose();
#endif
   }
}