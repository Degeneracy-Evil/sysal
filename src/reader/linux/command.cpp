#include "reader/linux/command.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

namespace sysal::reader
{
    namespace
    {
        class Pipe
        {
        public:
            Pipe()
            {
                if(pipe2(descriptors_.data(), O_CLOEXEC) != 0)
                {
                    descriptors_ = {-1, -1};
                    return;
                }
                // File actions must never close a standard stream after redirecting it.
                for(auto &descriptor : descriptors_)
                    if(descriptor < 3)
                    {
                        const int replacement = fcntl(descriptor, F_DUPFD_CLOEXEC, 3);
                        close(descriptor);
                        descriptor = replacement;
                    }
            }
            ~Pipe()
            {
                close_reader();
                close_writer();
            }
            Pipe(const Pipe &) = delete;
            Pipe &operator=(const Pipe &) = delete;
            Pipe(Pipe &&) = delete;
            Pipe &operator=(Pipe &&) = delete;
            int reader() const
            {
                return descriptors_[0];
            }
            int writer() const
            {
                return descriptors_[1];
            }
            bool valid() const
            {
                return reader() >= 0 && writer() >= 0;
            }
            void close_reader()
            {
                close_descriptor(descriptors_[0]);
            }
            void close_writer()
            {
                close_descriptor(descriptors_[1]);
            }

        private:
            static void close_descriptor(int &descriptor)
            {
                if(descriptor >= 0)
                    close(descriptor);
                descriptor = -1;
            }
            std::array<int, 2> descriptors_{-1, -1};
        };

        class SpawnConfiguration
        {
        public:
            SpawnConfiguration()
                : actions_ready_(posix_spawn_file_actions_init(&actions_) == 0),
                  attributes_ready_(posix_spawnattr_init(&attributes_) == 0)
            {
            }
            ~SpawnConfiguration()
            {
                if(actions_ready_)
                    posix_spawn_file_actions_destroy(&actions_);
                if(attributes_ready_)
                    posix_spawnattr_destroy(&attributes_);
            }
            SpawnConfiguration(const SpawnConfiguration &) = delete;
            SpawnConfiguration &operator=(const SpawnConfiguration &) = delete;
            SpawnConfiguration(SpawnConfiguration &&) = delete;
            SpawnConfiguration &operator=(SpawnConfiguration &&) = delete;
            bool configure(const Pipe &output, const Pipe &error)
            {
                return actions_ready_ && attributes_ready_ &&
                       posix_spawnattr_setflags(&attributes_, POSIX_SPAWN_SETPGROUP) == 0 &&
                       posix_spawnattr_setpgroup(&attributes_, 0) == 0 &&
                       posix_spawn_file_actions_addopen(&actions_, STDIN_FILENO, "/dev/null", O_RDONLY, 0) == 0 &&
                       redirect(output, STDOUT_FILENO) && redirect(error, STDERR_FILENO);
            }
            int start(pid_t &pid, const std::string &command)
            {
                // Existing probes include shell builtins and pipelines. Keep their syntax in one boundary.
                std::array<char *, 4> args{const_cast<char *>("sh"), const_cast<char *>("-c"),
                                           const_cast<char *>(command.c_str()), nullptr};
                return posix_spawn(&pid, "/bin/sh", &actions_, &attributes_, args.data(), environ);
            }

        private:
            bool redirect(const Pipe &pipe, int destination)
            {
                return posix_spawn_file_actions_adddup2(&actions_, pipe.writer(), destination) == 0 &&
                       posix_spawn_file_actions_addclose(&actions_, pipe.reader()) == 0 &&
                       posix_spawn_file_actions_addclose(&actions_, pipe.writer()) == 0;
            }
            posix_spawn_file_actions_t actions_{};
            posix_spawnattr_t attributes_{};
            bool actions_ready_, attributes_ready_;
        };

        class ChildProcess
        {
        public:
            explicit ChildProcess(pid_t pid) : pid_(pid) {}
            ~ChildProcess()
            {
                if(!reaped_)
                    terminate();
            }
            ChildProcess(const ChildProcess &) = delete;
            ChildProcess &operator=(const ChildProcess &) = delete;
            ChildProcess(ChildProcess &&) = delete;
            ChildProcess &operator=(ChildProcess &&) = delete;
            void terminate()
            {
                // Kill the probe's process group, including subprocesses retaining output pipes.
                kill(-pid_, SIGKILL);
                if(!reaped_)
                    reap(0);
            }
            void poll()
            {
                if(!reaped_)
                    reap(WNOHANG);
            }
            bool finished() const
            {
                return reaped_;
            }
            std::optional<int> status() const
            {
                return status_;
            }

        private:
            void reap(int flags)
            {
                int status{};
                pid_t result;
                do
                {
                    result = waitpid(pid_, &status, flags);
                } while(result < 0 && errno == EINTR);
                if(result == pid_)
                {
                    status_ = status;
                    reaped_ = true;
                }
                else if(result < 0)
                    reaped_ = true;
            }
            pid_t pid_;
            bool reaped_{};
            std::optional<int> status_;
        };

        bool drain(Pipe &pipe, std::string &output, std::size_t limit, CommandStatus &status)
        {
            std::array<char, 4096> buffer{};
            // Bound each drain so a continuously writing child cannot starve the deadline check.
            for(unsigned reads = 0; reads < 16; ++reads)
            {
                const auto count = read(pipe.reader(), buffer.data(), buffer.size());
                if(count > 0)
                {
                    const auto bytes = static_cast<std::size_t>(count);
                    if(bytes > limit - output.size())
                    {
                        status = CommandStatus::OutputLimit;
                        return false;
                    }
                    output.append(buffer.data(), bytes);
                }
                else if(count == 0)
                {
                    pipe.close_reader();
                    return true;
                }
                else if(errno == EAGAIN || errno == EWOULDBLOCK)
                    return true;
                else if(errno != EINTR)
                {
                    status = CommandStatus::ReadError;
                    return false;
                }
            }
            return true;
        }
    } // namespace

    CommandResult execute_command(const std::string &command, const CommandOptions &options)
    {
        CommandResult result;
        Pipe output, error;
        SpawnConfiguration configuration;
        if(!output.valid() || !error.valid() || !configuration.configure(output, error))
            return result;
        pid_t pid{};
        if(configuration.start(pid, command) != 0)
            return result;
        ChildProcess child(pid);
        output.close_writer();
        error.close_writer();
        if(fcntl(output.reader(), F_SETFL, O_NONBLOCK) < 0 || fcntl(error.reader(), F_SETFL, O_NONBLOCK) < 0)
        {
            result.status = CommandStatus::ReadError;
            return result;
        }
        const auto deadline = std::chrono::steady_clock::now() + options.timeout;
        result.status = CommandStatus::Success;
        while(output.reader() >= 0 || error.reader() >= 0 || !child.finished())
        {
            const auto remaining =
                std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
            if(remaining.count() <= 0)
            {
                result.status = CommandStatus::TimedOut;
                break;
            }
            std::array<pollfd, 2> descriptors{{{output.reader(), POLLIN, 0}, {error.reader(), POLLIN, 0}}};
            const auto interval = output.reader() >= 0 || error.reader() >= 0 ? 50 : 1;
            const auto wait = static_cast<int>(std::min<std::int64_t>(remaining.count(), interval));
            if(::poll(descriptors.data(), descriptors.size(), wait) < 0 && errno != EINTR)
            {
                result.status = CommandStatus::ReadError;
                break;
            }
            if((output.reader() >= 0 && !drain(output, result.output, options.output_limit, result.status)) ||
               (error.reader() >= 0 && !drain(error, result.error, options.output_limit, result.status)))
                break;
            child.poll();
        }
        if(result.status != CommandStatus::Success)
            child.terminate();
        if(const auto status = child.status())
        {
            if(WIFEXITED(*status))
                result.exit_code = WEXITSTATUS(*status);
            if(WIFSIGNALED(*status))
                result.signal = WTERMSIG(*status);
            if(result.status == CommandStatus::Success && result.exit_code != std::optional<int>{0})
                result.status = CommandStatus::Failed;
        }
        else if(result.status == CommandStatus::Success)
            result.status = CommandStatus::ReadError;
        return result;
    }
} // namespace sysal::reader
