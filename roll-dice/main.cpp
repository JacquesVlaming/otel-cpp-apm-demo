#include "oatpp/web/server/HttpConnectionHandler.hpp"
#include "oatpp/network/Server.hpp"
#include "oatpp/network/tcp/server/ConnectionProvider.hpp"
#include <cstdlib>
#include <ctime>
#include <string>
#include <thread>
#include <chrono>
#include <sstream>

using namespace std;

// ----- Handlers per route so we don't depend on request path/method API (Oat++ 1.3) -----

class RootHandler : public oatpp::web::server::HttpRequestHandler {
public:
  shared_ptr<OutgoingResponse> handle(const shared_ptr<IncomingRequest>&) override {
    const char* help = "{\"service\":\"dice-server\",\"endpoints\":{\"GET /rolldice\":\"roll 1-6\",\"GET /rolldice?sides=N\":\"roll 1..N\",\"GET /rolldice/slow?delay=2\":\"sleep then roll\",\"GET /rolldice/error\":\"returns 500\",\"GET /rolldice/big?sides=100\":\"large JSON\",\"POST /rolldice\":\"body optional {\\\"sides\\\":6}\"}}";
    return ResponseFactory::createResponse(Status::CODE_200, help);
  }
};

class RolldiceHandler : public oatpp::web::server::HttpRequestHandler {
public:
  shared_ptr<OutgoingResponse> handle(const shared_ptr<IncomingRequest>& request) override {
    int sides = 6;
    auto sidesParam = request->getQueryParameter("sides");
    if (sidesParam && sidesParam->size() > 0) {
      try {
        int n = std::stoi(sidesParam->c_str());
        if (n >= 1 && n <= 1000) sides = n;
      } catch (...) {}
    }
    int roll = (rand() % sides) + 1;
    return ResponseFactory::createResponse(Status::CODE_200, std::to_string(roll).c_str());
  }
};

// APM: duration / latency (slow path)
class RolldiceSlowHandler : public oatpp::web::server::HttpRequestHandler {
public:
  shared_ptr<OutgoingResponse> handle(const shared_ptr<IncomingRequest>& request) override {
    int sides = 6, delaySec = 1;
    auto p = request->getQueryParameter("sides"); if (p && p->size() > 0) try { int n = std::stoi(p->c_str()); if (n >= 1 && n <= 1000) sides = n; } catch (...) {}
    p = request->getQueryParameter("delay"); if (p && p->size() > 0) try { int n = std::stoi(p->c_str()); if (n >= 0 && n <= 60) delaySec = n; } catch (...) {}
    std::this_thread::sleep_for(std::chrono::seconds(static_cast<size_t>(delaySec)));
    int roll = (rand() % sides) + 1;
    std::string body = "{\"roll\":\"" + std::to_string(roll) + "\",\"sides\":\"" + std::to_string(sides) + "\",\"delay_sec\":\"" + std::to_string(delaySec) + "\"}";
    return ResponseFactory::createResponse(Status::CODE_200, body.c_str());
  }
};

// APM: http.response.status_code 500, span status Error
class RolldiceErrorHandler : public oatpp::web::server::HttpRequestHandler {
public:
  shared_ptr<OutgoingResponse> handle(const shared_ptr<IncomingRequest>&) override {
    return ResponseFactory::createResponse(Status::CODE_500, "{\"error\":\"dice exploded\"}");
  }
};

// APM: http.response.body.size (large payload)
class RolldiceBigHandler : public oatpp::web::server::HttpRequestHandler {
public:
  shared_ptr<OutgoingResponse> handle(const shared_ptr<IncomingRequest>& request) override {
    int sides = 100;
    auto p = request->getQueryParameter("sides"); if (p && p->size() > 0) try { int n = std::stoi(p->c_str()); if (n >= 1 && n <= 500) sides = n; } catch (...) {}
    int roll = (rand() % sides) + 1;
    std::ostringstream oss;
    oss << "{\"roll\":" << roll << ",\"sides\":" << sides << ",\"history\":[";
    for (int i = 0; i < 200; ++i) { if (i) oss << ","; oss << (rand() % sides + 1); }
    oss << "]}";
    std::string body = oss.str();
    return ResponseFactory::createResponse(Status::CODE_200, body.c_str());
  }
};

// APM: http.request.method POST, http.request.body.size (preloader captures body size from read())
class RolldicePostHandler : public oatpp::web::server::HttpRequestHandler {
public:
  shared_ptr<OutgoingResponse> handle(const shared_ptr<IncomingRequest>& request) override {
    int sides = 6;
    (void)request; // body already read by framework; preloader saw request body size
    int roll = (rand() % sides) + 1;
    std::string resp = "{\"roll\":\"" + std::to_string(roll) + "\",\"sides\":\"" + std::to_string(sides) + "\"}";
    return ResponseFactory::createResponse(Status::CODE_200, resp.c_str());
  }
};

void run() {
  auto router = oatpp::web::server::HttpRouter::createShared();
  router->route("GET", "/", std::make_shared<RootHandler>());
  router->route("GET", "/rolldice", std::make_shared<RolldiceHandler>());
  router->route("GET", "/rolldice/", std::make_shared<RolldiceHandler>());
  router->route("GET", "/rolldice/slow", std::make_shared<RolldiceSlowHandler>());
  router->route("GET", "/rolldice/error", std::make_shared<RolldiceErrorHandler>());
  router->route("GET", "/rolldice/big", std::make_shared<RolldiceBigHandler>());
  router->route("POST", "/rolldice", std::make_shared<RolldicePostHandler>());
  router->route("POST", "/rolldice/", std::make_shared<RolldicePostHandler>());
  auto connectionHandler = oatpp::web::server::HttpConnectionHandler::createShared(router);
  auto connectionProvider = oatpp::network::tcp::server::ConnectionProvider::createShared({"0.0.0.0", 8080, oatpp::network::Address::IP_4});
  oatpp::network::Server server(connectionProvider, connectionHandler);
  OATPP_LOGI("Dice Server", "Server running on port %s", static_cast<const char*>(connectionProvider->getProperty("port").getData()));
  server.run();
}

int main() {
  oatpp::base::Environment::init();
  srand(static_cast<unsigned>(time(nullptr)));
  run();
  oatpp::base::Environment::destroy();
  return 0;
}
