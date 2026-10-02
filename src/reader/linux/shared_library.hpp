#pragma once

#include <bit>
#include <dlfcn.h>

namespace sysal::reader
{
    class SharedLibrary
    {
    public:
        explicit SharedLibrary(const char *name) : handle_(dlopen(name, RTLD_LOCAL | RTLD_LAZY)) {}
        ~SharedLibrary()
        {
            if(handle_ != nullptr)
                dlclose(handle_);
        }
        SharedLibrary(const SharedLibrary &) = delete;
        SharedLibrary &operator=(const SharedLibrary &) = delete;
        SharedLibrary(SharedLibrary &&) = delete;
        SharedLibrary &operator=(SharedLibrary &&) = delete;
        [[nodiscard]] bool available() const
        {
            return handle_ != nullptr;
        }
        template <typename Function> Function symbol(const char *name) const
        {
            return handle_ == nullptr ? nullptr : std::bit_cast<Function>(dlsym(handle_, name));
        }

    private:
        void *handle_{};
    };
} // namespace sysal::reader
