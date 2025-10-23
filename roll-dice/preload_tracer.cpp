#include <dlfcn.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <csignal>
#include <cstdlib>
#include <chrono>
#include <map>
#include <mutex>

#include <opentelemetry/trace/provider.h>
#include <opentelemetry/sdk/trace/tracer_provider.h>
#include <opentelemetry/sdk/trace/batch_span_processor_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_exporter_factory.h>
#include <opentelemetry/nostd/shared_ptr.h>

namespace trace_api = opentelemetry::trace;
namespace trace_sdk = opentelemetry::sdk::trace;
namespace otlp      = opentelemetry::exporter::otlp;

using AcceptFuncType = int(*)(int, struct sockaddr*, socklen_t*);
using ReadFuncType   = ssize_t(*)(int, void*, size_t);

AcceptFuncType real_accept = nullptr;
ReadFuncType   real_read   = nullptr;

opentelemetry::nostd::shared_ptr<trace_api::Tracer> tracer;
std::shared_ptr<trace_sdk::TracerProvider> provider;
bool otel_initialized = false;

// Map fd -> span to track ongoing HTTP requests
std::map<int, opentelemetry::nostd::shared_ptr<trace_api::Span>> fd_span_map;
std::mutex fd_span_mutex;

// ---- Lazy OTLP tracer initialization ----
void init_tracer_lazy() {
    if (otel_initialized) return;
    otel_initialized = true;

    try {
        const char* endpoint_env = std::getenv("OTEL_EXPORTER_OTLP_ENDPOINT");
        const char* headers_env  = std::getenv("OTEL_EXPORTER_OTLP_HEADERS");

        if (!endpoint_env) {
            std::cerr << "[OTEL PRELOAD] OTEL_EXPORTER_OTLP_ENDPOINT not set" << std::endl;
            return;
        }

        otlp::OtlpHttpExporterOptions opts;
        opts.url = std::string(endpoint_env);

        if (headers_env) {
            // Split headers by comma if multiple, e.g., "key1=value1,key2=value2"
            std::string headers_str(headers_env);
            size_t start = 0;
            while (start < headers_str.size()) {
                size_t end = headers_str.find(',', start);
                if (end == std::string::npos) end = headers_str.size();
                std::string header = headers_str.substr(start, end - start);
                size_t eq = header.find('=');
                if (eq != std::string::npos) {
                    std::string key = header.substr(0, eq);
                    std::string value = header.substr(eq + 1);
                    opts.http_headers[key] = value;
                }
                start = end + 1;
            }
        }

        auto exporter  = otlp::OtlpHttpExporterFactory::Create(opts);
        trace_sdk::BatchSpanProcessorOptions bspOpts{};
        auto processor = trace_sdk::BatchSpanProcessorFactory::Create(std::move(exporter), bspOpts);

        provider = std::make_shared<trace_sdk::TracerProvider>(std::move(processor));
        opentelemetry::nostd::shared_ptr<trace_api::TracerProvider> provider_nostd(provider);
        trace_api::Provider::SetTracerProvider(provider_nostd);

        tracer = provider->GetTracer("dice-server-tracer", "1.0");

        std::cout << "[OTEL PRELOAD] Tracing initialized ✅" << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "[OTEL PRELOAD] Failed to initialize tracing: " << e.what() << std::endl;
    }
}

// ---- Flush spans on exit ----
void flush_spans() {
    if (!provider) return;
    std::cout << "[OTEL PRELOAD] Flushing spans..." << std::endl;
    provider->ForceFlush();
}

// ---- Hook accept() ----
extern "C" int accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen) {
    init_tracer_lazy();
    if (!real_accept) real_accept = (AcceptFuncType)dlsym(RTLD_NEXT, "accept");

    int client_fd = real_accept(sockfd, addr, addrlen);
    if (client_fd != -1 && tracer) {
        auto span = tracer->StartSpan("HTTP accept_connection");
        span->AddEvent("Accepted client fd: " + std::to_string(client_fd));
        span->End();
    }
    return client_fd;
}

// ---- Hook read() ----
extern "C" ssize_t read(int fd, void* buf, size_t count) {
    init_tracer_lazy();
    if (!real_read) real_read = (ReadFuncType)dlsym(RTLD_NEXT, "read");

    ssize_t bytes = real_read(fd, buf, count);
    if (bytes > 0 && tracer) {
        std::string data(static_cast<char*>(buf), bytes);

        // If HTTP GET /rolldice request
        if (data.find("GET /rolldice") != std::string::npos) {
            auto span = tracer->StartSpan("HTTP GET /rolldice");
            span->AddEvent("Received /rolldice request of " + std::to_string(bytes) + " bytes");

            // Track span by fd
            std::lock_guard<std::mutex> lock(fd_span_mutex);
            fd_span_map[fd] = span;
        }

        // If HTTP response is sent (detect simple end of request)
        else if (data.find("\r\n\r\n") != std::string::npos) {
            std::lock_guard<std::mutex> lock(fd_span_mutex);
            auto it = fd_span_map.find(fd);
            if (it != fd_span_map.end()) {
                it->second->End();
                fd_span_map.erase(it);
            }
        }
    }
    return bytes;
}

// ---- Preload library constructor / destructor ----
__attribute__((constructor))
void preload_init() {
    std::cout << "[OTEL PRELOAD] Library loaded ✅" << std::endl;
}

__attribute__((destructor))
void preload_cleanup() {
    flush_spans();
    std::cout << "[OTEL PRELOAD] Library unloaded ✅" << std::endl;
}

// ---- Handle Ctrl+C ----
void sigint_handler(int signum) {
    flush_spans();
    std::exit(signum);
}

__attribute__((constructor))
void setup_signal_handler() {
    std::signal(SIGINT, sigint_handler);
}
