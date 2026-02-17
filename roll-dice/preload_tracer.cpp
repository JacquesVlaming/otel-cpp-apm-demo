#include <dlfcn.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdio>
#include <iostream>
#include <string>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <map>
#include <mutex>
#include <sstream>

#include <opentelemetry/context/context.h>
#include <opentelemetry/context/propagation/text_map_propagator.h>
#include <opentelemetry/trace/provider.h>
#include <opentelemetry/trace/span_metadata.h>
#include <opentelemetry/trace/span_startoptions.h>
#include <opentelemetry/trace/propagation/http_trace_context.h>
#include <opentelemetry/sdk/trace/tracer_provider.h>
#include <opentelemetry/sdk/trace/batch_span_processor_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_exporter_factory.h>
#include <opentelemetry/nostd/shared_ptr.h>
#include <opentelemetry/nostd/string_view.h>
// Optional: <opentelemetry/semconv/service_attributes.h> for kServiceName if your SDK installs it

namespace trace_api = opentelemetry::trace;
namespace trace_sdk = opentelemetry::sdk::trace;
namespace otlp      = opentelemetry::exporter::otlp;

using AcceptFuncType = int(*)(int, struct sockaddr*, socklen_t*);
using ReadFuncType   = ssize_t(*)(int, void*, size_t);
using WriteFuncType = ssize_t(*)(int, const void*, size_t);
using CloseFuncType = int(*)(int);

AcceptFuncType real_accept = nullptr;
ReadFuncType   real_read   = nullptr;
WriteFuncType  real_write  = nullptr;
CloseFuncType  real_close  = nullptr;

opentelemetry::nostd::shared_ptr<trace_api::Tracer> tracer;
std::shared_ptr<trace_sdk::TracerProvider> provider;
bool otel_initialized = false;

// Per-fd request context: span and parsed path for display
struct FdSpanContext {
    opentelemetry::nostd::shared_ptr<trace_api::Span> span;
    std::string method;
    std::string path;
};
std::map<int, FdSpanContext> fd_span_map;
std::mutex fd_span_mutex;

// Optional server context (set from env OTEL_SERVER_PORT / OTEL_SERVER_ADDRESS)
static const char* server_address = nullptr;
static const char* server_port    = nullptr;

// Forward decl for RHEL 8 safe log (no iostreams in constructor/destructor path)
static void preload_log(const char* msg);

// ---- Parse "METHOD /path HTTP/1.x" from first line ----
static void parse_request_line(const std::string& line, std::string& method, std::string& path) {
    method.clear();
    path.clear();
    std::istringstream iss(line);
    if (!(iss >> method >> path)) return;
    size_t q = path.find('?');
    if (q != std::string::npos) path = path.substr(0, q);
}

// ---- Parse HTTP headers (Key: Value) into map with lowercase keys ----
using HeaderMap = std::map<std::string, std::string>;
static void parse_request_headers(const std::string& data, HeaderMap& out) {
    out.clear();
    size_t pos = data.find("\r\n");
    if (pos == std::string::npos) return;
    pos += 2;
    while (pos < data.size()) {
        size_t line_end = data.find("\r\n", pos);
        std::string line = (line_end != std::string::npos) ? data.substr(pos, line_end - pos) : data.substr(pos);
        if (line.empty()) break;
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string value = (colon + 1 < line.size()) ? line.substr(colon + 1) : "";
            while (!value.empty() && value[0] == ' ') value.erase(0, 1);
            for (auto& c : key) if (c >= 'A' && c <= 'Z') c += 32;
            out[key] = value;
        }
        pos = (line_end != std::string::npos) ? line_end + 2 : data.size();
    }
}

// ---- Carrier for W3C Trace Context extraction from request headers ----
struct HeaderCarrier : opentelemetry::context::propagation::TextMapCarrier {
    const HeaderMap* headers = nullptr;
    opentelemetry::nostd::string_view Get(opentelemetry::nostd::string_view key) const noexcept override {
        if (!headers) return "";
        std::string k(key.data(), key.size());
        for (auto& c : k) if (c >= 'A' && c <= 'Z') c += 32;
        auto it = headers->find(k);
        if (it == headers->end()) return "";
        return opentelemetry::nostd::string_view(it->second.data(), it->second.size());
    }
    void Set(opentelemetry::nostd::string_view, opentelemetry::nostd::string_view) noexcept override {}
};

// ---- Parse "HTTP/1.x CODE" from response ----
static bool parse_response_status(const char* buf, size_t len, int& status_code) {
    if (len < 12) return false;
    if (std::strncmp(buf, "HTTP/1.", 7) != 0) return false;
    const char* p = buf + 8;
    while (p < buf + len && *p == ' ') ++p;
    if (p >= buf + len) return false;
    char* end = nullptr;
    long code = std::strtol(p, &end, 10);
    if (end == p || code < 0 || code > 999) return false;
    status_code = static_cast<int>(code);
    return true;
}

// ---- End and remove span for fd (called from write or close) ----
static void end_span_for_fd(int fd) {
    std::lock_guard<std::mutex> lock(fd_span_mutex);
    auto it = fd_span_map.find(fd);
    if (it != fd_span_map.end()) {
        it->second.span->End();
        fd_span_map.erase(it);
    }
}

// ---- Lazy OTLP tracer initialization ----
void init_tracer_lazy() {
    if (otel_initialized) return;
    otel_initialized = true;

    server_address = std::getenv("OTEL_SERVER_ADDRESS");
    server_port    = std::getenv("OTEL_SERVER_PORT");
    if (!server_address) server_address = "localhost";
    if (!server_port)    server_port    = "8080";

    try {

        const char* endpoint_env = std::getenv("OTEL_EXPORTER_OTLP_ENDPOINT");
        const char* headers_env  = std::getenv("OTEL_EXPORTER_OTLP_HEADERS");
        const char* service_env  = std::getenv("OTEL_SERVICE_NAME");

        if (!endpoint_env || !*endpoint_env) {
            std::cerr << "[OTEL PRELOAD] OTEL_EXPORTER_OTLP_ENDPOINT not set; tracing disabled." << std::endl;
            return;
        }

        otlp::OtlpHttpExporterOptions opts;
        opts.url = std::string(endpoint_env);
        opts.http_headers = {{"Authorization", headers_env && *headers_env ? headers_env : ""}};

        auto exporter  = otlp::OtlpHttpExporterFactory::Create(opts);
        trace_sdk::BatchSpanProcessorOptions bspOpts{};
        auto processor = trace_sdk::BatchSpanProcessorFactory::Create(std::move(exporter), bspOpts);

        // ---- TracerProvider (use default resource; custom resource requires SDK-specific AttributeMap) ----
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
    preload_log("[OTEL PRELOAD] Flushing spans...");
    provider->ForceFlush();
}

// ---- Hook accept() ----
extern "C" int accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen) {
    init_tracer_lazy();
    if (!real_accept) real_accept = (AcceptFuncType)dlsym(RTLD_NEXT, "accept");

    int client_fd = real_accept(sockfd, addr, addrlen);
    if (client_fd != -1 && tracer) {
        trace_api::StartSpanOptions opts;
        opts.kind = trace_api::SpanKind::kServer;
        auto span = tracer->StartSpan("HTTP accept", opts);
        span->SetAttribute("server.address", server_address);
        span->SetAttribute("server.port", std::stol(server_port));
        span->SetAttribute("net.sock.peer.fd", client_fd);
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
        std::string data(static_cast<char*>(buf), static_cast<size_t>(bytes));

        // Find first line (request line)
        size_t line_end = data.find("\r\n");
        std::string first_line = (line_end != std::string::npos) ? data.substr(0, line_end) : data;
        std::string method, path;
        parse_request_line(first_line, method, path);

        // Start request span for any HTTP-like request (METHOD /path HTTP/x.x)
        if (!method.empty() && path.size() >= 1 && path[0] == '/' &&
            (data.find("HTTP/1.") != std::string::npos || data.find("HTTP/2") != std::string::npos)) {
            HeaderMap header_map;
            parse_request_headers(data, header_map);

            trace_api::StartSpanOptions opts;
            opts.kind = trace_api::SpanKind::kServer;
            // W3C Trace Context: use incoming traceparent/tracestate as parent for distributed tracing
            HeaderCarrier carrier;
            carrier.headers = &header_map;
            opentelemetry::context::Context ctx_in;
            opentelemetry::context::Context ctx_out =
                trace_api::propagation::HttpTraceContext().Extract(carrier, ctx_in);
            opts.parent = ctx_out;

            std::string span_name = method + " " + path;
            auto span = tracer->StartSpan(span_name, opts);

            span->SetAttribute("http.request.method", method);
            span->SetAttribute("url.path", path);
            span->SetAttribute("http.request.body.size", static_cast<int64_t>(bytes));
            span->SetAttribute("server.address", server_address);
            span->SetAttribute("server.port", std::stol(server_port));
            // Request headers for APM breakdown / debugging
            auto it_ua = header_map.find("user-agent");
            if (it_ua != header_map.end() && !it_ua->second.empty())
                span->SetAttribute("http.request.header.user_agent", it_ua->second);
            auto it_req_id = header_map.find("x-request-id");
            if (it_req_id != header_map.end() && !it_req_id->second.empty())
                span->SetAttribute("http.request.header.x_request_id", it_req_id->second);
            span->AddEvent("Request received");

            FdSpanContext ctx{std::move(span), std::move(method), std::move(path)};
            std::lock_guard<std::mutex> lock(fd_span_mutex);
            fd_span_map[fd] = std::move(ctx);
        }
    }
    return bytes;
}

// ---- Hook write() ----
extern "C" ssize_t write(int fd, const void* buf, size_t count) {
    init_tracer_lazy();
    if (!real_write) real_write = (WriteFuncType)dlsym(RTLD_NEXT, "write");

    ssize_t n = real_write(fd, buf, count);
    if (n > 0 && tracer) {
        std::lock_guard<std::mutex> lock(fd_span_mutex);
        auto it = fd_span_map.find(fd);
        if (it != fd_span_map.end()) {
            int status_code = 0;
            if (parse_response_status(static_cast<const char*>(buf), static_cast<size_t>(n), status_code)) {
                it->second.span->SetAttribute("http.response.status_code", static_cast<int64_t>(status_code));
                it->second.span->SetAttribute("http.response.body.size", static_cast<int64_t>(n));
                if (status_code >= 400)
                    it->second.span->SetStatus(trace_api::StatusCode::kError, "HTTP " + std::to_string(status_code));
                else
                    it->second.span->SetStatus(trace_api::StatusCode::kOk);
            }
            it->second.span->End();
            fd_span_map.erase(it);
        }
    }
    return n;
}

// ---- Hook close() ----
extern "C" int close(int fd) {
    if (real_close == nullptr) real_close = (CloseFuncType)dlsym(RTLD_NEXT, "close");
    end_span_for_fd(fd);
    return real_close(fd);
}

// Safe log from constructor/destructor (no iostreams — avoids segfault on RHEL 8)
static void preload_log(const char* msg) {
    size_t n = 0;
    while (msg[n]) ++n;
    (void)write(STDERR_FILENO, msg, n);
    (void)write(STDERR_FILENO, "\n", 1);
}

// ---- Preload library constructor / destructor ----
// constructor(101) runs later, reducing init-order issues; no std::cout (RHEL 8 safe)
__attribute__((constructor(101)))
void preload_init() {
    preload_log("[OTEL PRELOAD] Library loaded");
}

__attribute__((destructor))
void preload_cleanup() {
    flush_spans();
    preload_log("[OTEL PRELOAD] Library unloaded");
}

// ---- Handle Ctrl+C ----
void sigint_handler(int signum) {
    flush_spans();
    std::exit(signum);
}

__attribute__((constructor(102)))
void setup_signal_handler() {
    std::signal(SIGINT, sigint_handler);
}
