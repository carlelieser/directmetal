#define DIRECTMETAL_BRIDGE_API
#include <directmetal/bridge/client.hpp>

#include <shared_allocation.hpp>

#include <cstdint>
#include <limits>
#include <windows.h>

extern "C" LONG NTAPI NtQueryVirtualMemory(HANDLE process, PVOID address, int information_class,
                                           PVOID buffer, SIZE_T length, SIZE_T* result_length);

namespace {

using namespace directmetal::bridge;

using Dispatcher = Status(WINAPI*)(std::uint64_t handle, std::uint32_t code, void* params) noexcept;

// Wine-specific MEMORY_INFORMATION_CLASS value that loads a builtin module's unix library.
constexpr int memory_wine_load_unix_lib = 1000;

struct Connection {
    std::uint64_t handle = 0;
    Dispatcher dispatcher = nullptr;
    Status status = status_dll_not_found;
};

Connection connection;

// ntdll exports the dispatcher as a data symbol holding the function pointer.
Dispatcher find_dispatcher() noexcept {
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    FARPROC slot =
        ntdll == nullptr ? nullptr : GetProcAddress(ntdll, "__wine_unix_call_dispatcher");
    return slot == nullptr ? nullptr : *reinterpret_cast<Dispatcher*>(slot);
}

Connection connect(HMODULE module) noexcept {
    Connection result;
    result.dispatcher = find_dispatcher();
    if (result.dispatcher == nullptr) {
        result.status = status_entrypoint_not_found;
        return result;
    }
    result.status = NtQueryVirtualMemory(GetCurrentProcess(), module, memory_wine_load_unix_lib,
                                         &result.handle, sizeof(result.handle), nullptr);
    if (result.status < 0) {
        return result;
    }
    QueryVersionParams query{};
    result.status = result.dispatcher(result.handle, to_index(FunctionCode::query_version), &query);
    if (result.status >= 0 && query.version != protocol_version) {
        result.status = status_revision_mismatch;
    }
    return result;
}

} // namespace

extern "C" Status __cdecl directmetal_bridge_call(std::uint32_t code, void* params) noexcept {
    if (connection.status < 0) {
        return connection.status;
    }
    if (code >= to_index(FunctionCode::count)) {
        return status_invalid_parameter;
    }
    return connection.dispatcher(connection.handle, code, params);
}

// VirtualAlloc returns zeroed memory aligned to the 64 KiB allocation granularity, which is a
// multiple of shared_memory_alignment, and stays below 4 GiB in 32-bit processes.
extern "C" void* __cdecl directmetal_bridge_allocate_shared(std::uint64_t size) noexcept {
    const std::uint64_t rounded = shared_allocation_size(size);
    if (rounded == 0 || rounded > std::numeric_limits<SIZE_T>::max()) {
        return nullptr;
    }
    return VirtualAlloc(nullptr, static_cast<SIZE_T>(rounded), MEM_RESERVE | MEM_COMMIT,
                        PAGE_READWRITE);
}

extern "C" void __cdecl directmetal_bridge_free_shared(void* memory, std::uint64_t) noexcept {
    if (memory != nullptr) {
        VirtualFree(memory, 0, MEM_RELEASE);
    }
}

// Connecting dlopens directmetal.so under the loader lock, so its static initializers must stay
// trivial; the backend initializes lazily on its first call.
extern "C" BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) noexcept {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        connection = connect(instance);
    }
    return TRUE;
}
