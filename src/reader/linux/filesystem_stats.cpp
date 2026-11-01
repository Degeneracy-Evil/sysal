#include "reader/linux/filesystem_stats.hpp"
#include "reader/linux/file_utils.hpp"

#include <fcntl.h>
#include <iterator>
#include <linux/magic.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/sysmacros.h>
#include <unistd.h>

namespace sysal::reader
{
    namespace
    {
        class PathDescriptor
        {
        public:
            explicit PathDescriptor(int fd) : fd_(fd) {}
            ~PathDescriptor()
            {
                reset(-1);
            }
            PathDescriptor(const PathDescriptor &) = delete;
            PathDescriptor &operator=(const PathDescriptor &) = delete;
            PathDescriptor(PathDescriptor &&) = delete;
            PathDescriptor &operator=(PathDescriptor &&) = delete;
            int get() const
            {
                return fd_;
            }
            void reset(int next)
            {
                if(fd_ >= 0)
                    ::close(fd_);
                fd_ = next;
            }

        private:
            int fd_;
        };

        bool queryable(int fd, FilesystemStatistics &result)
        {
            struct statfs filesystem
            {
            };
            if(::fstatfs(fd, &filesystem) != 0)
            {
                result.failure = file_failure(errno);
                return false;
            }
            if(filesystem.f_type == AUTOFS_SUPER_MAGIC)
            {
                result.skip = SkipReason::NotApplicable;
                result.note = "Automount filesystem not queried";
                return false;
            }
            return true;
        }
    } // namespace

    FilesystemStatistics read_filesystem_statistics(const StorageMount &mount)
    {
        FilesystemStatistics result;
        const std::filesystem::path path{mount.path.value};
        if(!path.is_absolute() || mount.path.value.find('\0') != std::string::npos)
        {
            result.failure = ReadFailure::Unsupported;
            return result;
        }
        // O_PATH does not activate an automount at the opened component. Check each
        // descriptor before walking further, and never resolve a symlink or reopen by path.
        PathDescriptor directory{::open("/", O_PATH | O_DIRECTORY | O_CLOEXEC)};
        if(directory.get() < 0)
        {
            result.failure = file_failure(errno);
            return result;
        }
        const auto relative = path.relative_path();
        for(auto part = relative.begin(); part != relative.end(); ++part)
        {
            const auto &component = *part;
            if(component == "." || component == "..")
            {
                result.failure = ReadFailure::Unsupported;
                return result;
            }
            if(!queryable(directory.get(), result))
                return result;
            const auto flags = O_PATH | O_NOFOLLOW | O_CLOEXEC | (std::next(part) == relative.end() ? 0 : O_DIRECTORY);
            const int next = ::openat(directory.get(), component.c_str(), flags);
            if(next < 0)
            {
                result.failure = file_failure(errno);
                return result;
            }
            directory.reset(next);
        }
        if(!queryable(directory.get(), result))
            return result;
        struct stat identity
        {
        };
        if(::fstat(directory.get(), &identity) != 0)
        {
            result.failure = file_failure(errno);
            return result;
        }
        if(S_ISLNK(identity.st_mode))
            result.failure = ReadFailure::Unsupported;
        else if(major(identity.st_dev) != mount.device_number.major ||
                minor(identity.st_dev) != mount.device_number.minor)
        {
            result.failure = ReadFailure::NotPresent;
            result.note = "Mount changed or is covered; statistics not associated";
        }
        else if(::fstatvfs(directory.get(), &result.values) != 0)
            result.failure = file_failure(errno);
        return result;
    }
} // namespace sysal::reader
