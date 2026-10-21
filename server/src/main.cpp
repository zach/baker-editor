// baker-lsp: Baker's diagnostics for editors, over LSP on stdin and stdout.
#include "protocol.hpp"
#include "server.hpp"
#include "setup.hpp"

#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <exception>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    for (const std::string& a : args)
        if (a == "--help" || a == "-h") {
            std::fputs(baker_lsp::kUsage, stdout);
            return 0;
        }
    baker_lsp::Setup setup;
    std::string error;
    if (!baker_lsp::parse_args(args, setup, error)) {
        std::fprintf(stderr, "baker-lsp: %s\n%s", error.c_str(), baker_lsp::kUsage);
        return 2;
    }
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    // Messages are read on their own thread, so a check waits for edits to settle.
    std::mutex mutex;
    std::condition_variable wake;
    std::deque<std::string> inbox;
    bool closed = false;
    std::thread reader([&] {
        std::string body;
        while (baker_lsp::read_message(stdin, body)) {
            std::lock_guard lock(mutex);
            inbox.push_back(std::move(body));
            wake.notify_one();
        }
        std::lock_guard lock(mutex);
        closed = true;
        wake.notify_one();
    });
    reader.detach();  // it may be blocked reading when the client says exit

    baker_lsp::Server server(std::move(setup), [](const nlohmann::json& message) {
        baker_lsp::write_message(stdout, message.dump());
    });
    for (;;) {
        std::deque<std::string> batch;
        {
            std::unique_lock lock(mutex);
            const auto ready = [&] { return !inbox.empty() || closed; };
            if (const auto due = server.due())
                wake.wait_until(lock, *due, ready);
            else
                wake.wait(lock, ready);
            batch.swap(inbox);
            if (batch.empty() && closed) {  // the client went away without exit
                std::fflush(stdout);
                std::_Exit(1);
            }
        }
        for (const std::string& body : batch) {
            nlohmann::json message = nlohmann::json::parse(body, nullptr, false);
            if (message.is_discarded()) {
                std::fprintf(stderr, "baker-lsp: a message that is not JSON\n");
                continue;
            }
            try {
                server.handle(message, baker_lsp::Server::Clock::now());
            } catch (const std::exception& e) {
                std::fprintf(stderr, "baker-lsp: a malformed message: %s\n", e.what());
            }
            if (const auto code = server.exit_code()) {
                // Not return: exit() would wait on stdin's lock, held by the blocked reader.
                std::fflush(stdout);
                std::_Exit(*code);
            }
        }
        server.tick(baker_lsp::Server::Clock::now());
    }
}
