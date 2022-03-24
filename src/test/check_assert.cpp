// Copyright (c) 2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/check_assert.h>

#include <tinyformat.h> // for strprintf()
#include <util/defer.h> // for Defer

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// If compiling with address or thread sanitizers, this facility doesn't work quite right due to code that ASAN/TSAN
// inserts, which interferes with our ability to trap the assert firing in the subprocess.
#if defined(__SANITIZE_THREAD__) || defined(__SANITIZE_ADDRESS__)
#  define SKIP_SANITIZER_NOT_SUPPORTED
#elif defined(__has_feature)
#  if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer)
#    define SKIP_SANITIZER_NOT_SUPPORTED
#  endif
#endif
#if !defined(SKIP_SANITIZER_NOT_SUPPORTED) && \
    __has_include(<unistd.h>) && __has_include(<poll.h>) && __has_include(<sys/types.h>) && \
    __has_include(<sys/wait.h>) && __has_include(<fcntl.h>)
#define IS_SUPPORTED
#include <fcntl.h>     // for fcntl()
#include <poll.h>      // for poll()
#include <sys/types.h> // for pid_t, etc
#include <sys/wait.h>  // for waitpid()
#include <unistd.h>    // for fork(), pipe(), dup2(), etc
#endif

#if defined(__GNUC__) && (__GNUC__ > 11 || (__GNUC__ == 11 && __GNUC_MINOR__ >= 1))
// GCC version 11.1 and later incorrectly warn of a maybe uninitialized usage
// of std::array, so we disable the warning.
// See https://gitlab.com/bitcoin-cash-node/bitcoin-cash-node/-/issues/351
// See https://godbolt.org/z/vWYxTh4eo
// See https://gcc.gnu.org/bugzilla/show_bug.cgi?id=101831
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

CheckAssertResult CheckAssert(std::function<void()> func, std::string_view expectMessage) {
#ifdef IS_SUPPORTED
    constexpr int exit_status_cannot_dup2 = 120;
    constexpr int exit_status_aborted = 201; // boost::exit_test_failure value
    static_assert(exit_status_cannot_dup2 != exit_status_aborted);
    std::array<int, 2> pipe_stdout{{-1, -1}}, pipe_stderr{{-1, -1}};
    Defer pipeCloser([&pipe_stdout, &pipe_stderr]{
        // close all fds on scope end to not leak fds in case we throw, etc
        for (auto & fds : {&pipe_stdout, &pipe_stderr}) {
            for (int & fd : *fds) {
                if (fd > -1) {
                    ::close(fd);
                    fd = -1;
                }
            }
        }
    });
    auto ChkRet = [](std::string_view call, int ret) {
        if (ret != 0) {
            throw std::runtime_error(strprintf("Error from `%s`: %s", std::string{call}, std::strerror(errno)));
        }
    };
    // create 2 pipes (to replace stdout and stderr)
    ChkRet("pipe", ::pipe(pipe_stdout.data()));
    ChkRet("pipe", ::pipe(pipe_stderr.data()));

    // fork and run the lambda in the child, checking if it assert()'ed (SIGABRT)
    // we also capture stdout and stderr from the child via the pipe created above.
    if (auto pid = ::fork(); pid == -1) {
        // fork failed
        ChkRet("fork", pid);
    } else if (pid == 0) {
        // child
        const int fd_stdout = fileno(stdout); // should be 1
        const int fd_stderr = fileno(stderr); // should be 2
        if (fd_stdout < 0 || fd_stderr < 0
                // make our pipe_stdout writing end be the new stdout
                || ::dup2(pipe_stdout[1], fd_stdout) != fd_stdout
                // make our pipe_stderr writing end be the new stderr
                || ::dup2(pipe_stderr[1], fd_stderr) != fd_stderr) {
            std::_Exit(exit_status_cannot_dup2);
        }
        // close reading ends in subprocess (they are open in parent process and should be closed here)
        ::close(pipe_stdout[0]); ::close(pipe_stderr[0]);
        pipe_stderr[0] = pipe_stderr[0] = -1;
        // disable stdout and stderr buffering for this operation since we are capturing
        // the output in the parent process and want to get it as soon as it occurs.
        std::setvbuf(stdout, nullptr, _IONBF, 0);
        std::setvbuf(stderr, nullptr, _IONBF, 0);
        // lastly, call the function
        try {
            func();
        } catch (...) {}
        // this should not be reached if the above resulted in an assert()
        std::_Exit(0);
    } else {
        // parent
        Defer pidKiller([&pid]{
            // kill & reap child process if it hasn't already been reaped
            if (pid > 0) {
                ::kill(pid, SIGKILL);
                int status;
                pid = ::waitpid(pid, &status, 0) == pid ? -1 : pid;
            }
        });
        ::close(pipe_stdout[1]); ::close(pipe_stderr[1]); // close writing end in parent process
        pipe_stderr[1] = pipe_stderr[1] = -1;

        std::string pipeData;
        int hadError = 0, status = 0;
        {
            // set up subordinate thread to read pipe (stdout and stderr from child are combined into pipeData buffer)
            std::atomic_bool stopFlag = false;
            std::thread pipeReaderThr([&]{
                Defer readRemainingData([&]{
                    // before the thread exits, we want to read any remaining
                    // straggling data from the pipes in non-blocking mode
                    for (const int fd : {pipe_stdout[0], pipe_stderr[0]}) {
                        if (const int flags = ::fcntl(fd, F_GETFL, nullptr); flags != -1) {
                            if (::fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1) { // set non-blocking reads
                                Defer restoreFlags([&]{ ::fcntl(fd, F_SETFL, flags); }); // restore flags on scope end
                                std::array<char, 256> buf;
                                int nread;
                                // keep draining fd in non-blocking mode until no data is left
                                while ((nread = ::read(fd, buf.data(), buf.size())) > 0) {
                                    pipeData.append(buf.data(), nread);
                                }
                            }
                        }
                    }
                });
                std::vector<pollfd> fds;
                for (int fd : {pipe_stdout[0], pipe_stderr[0]}) {
                    auto &p = fds.emplace_back();
                    p.fd = fd;
                    p.events = POLLIN;
                }

                while (!stopFlag) {
                    if (fds.empty()) return; // all file descriptors are closed, just return early
                    for (auto &p : fds) p.revents = 0; // reset pollfd state

                    const int r = ::poll(fds.data(), fds.size(), 10 /* msec */);
                    if (r > 0) {
                        auto do_read = [&pipeData](const pollfd &pfd) { // returns true if fd closed
                            if (!(pfd.revents & POLLIN)) return false;
                            std::array<char, 1024> buf;

                            if (const auto nread = ::read(pfd.fd, buf.data(), buf.size()); nread > 0) {
                                pipeData.append(buf.data(), nread);
                            } else if (nread == 0 || (nread < 0 && errno != EAGAIN && errno != EINTR)) {
                                // file closed (all done) or error
                                return true;
                            }
                            return false;
                        };
                        // some fds are ready, read from pipe(s) that are ready
                        // if do_read() returns false, erase the element from the vector
                        fds.erase(std::remove_if(fds.begin(), fds.end(), do_read), fds.end());
                    } else if (r < 0) {
                        // some error
                        if (errno == EAGAIN || errno == EINTR) {
                            // try again
                            continue;
                        }
                        hadError = errno;
                        return;
                    }
                }
            });
            Defer threadJoiner([&] {
                stopFlag = true;
                if (pipeReaderThr.joinable()) pipeReaderThr.join();
            }); // just in case the below throws

            const int ret = ::waitpid(pid, &status, 0);
            ChkRet("waitpid", ret == pid ? 0 : -1);
            pid = -1; // indicate pid is gone so that pidKiller above is a no-op

            using namespace std::chrono_literals;
            std::this_thread::sleep_for(10ms); // wait to read data
        }
        // at this point the pipe reader thread is stopped

        if (hadError != 0) {
            throw std::runtime_error(strprintf("Failed to read from pipe to subordinate process: %s",
                                               std::strerror(hadError)));
        }

        if (WIFEXITED(status) && WEXITSTATUS(status) == exit_status_cannot_dup2) {
            // special case, subordinate process cannot dup2()
            throw std::runtime_error("Low-level error: failed to dup2() stdout/stderr");
        }

        // we expect the subordinate process to have exited with a SIGABRT
        // however, if the boost execution monitor is running, no SIGABRT
        // will have been sent for an assert() failure, instead the
        // exit status will be 201.
        if ((WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT)
                || (WIFEXITED(status) && WEXITSTATUS(status) == exit_status_aborted)) {
            if (expectMessage.empty() || pipeData.find(expectMessage) != pipeData.npos) {
                return CheckAssertResult::AssertEncountered;
            }
            return CheckAssertResult::AssertEncounteredWrongMessage;
        }
        return CheckAssertResult::NoAssertEncountered;
    }
#endif
    return CheckAssertResult::Unsupported;
}

#if defined(__GNUC__) && (__GNUC__ > 11 || (__GNUC__ == 11 && __GNUC_MINOR__ >= 1))
#pragma GCC diagnostic pop
#endif
