#pragma once
// Operation journal: independent of anomaly deduplication and full-log mode.
// Producers only enqueue; the existing maintenance worker writes readable parts.
// No historical parts are removed. A full disk or a busy queue produces an
// explicit gap count on recovery, never a claim of complete coverage.
#include <windows.h>
#include <atomic>
#include <cstdio>
#include <cstring>
#include "../../common/event-log.hpp"

class ActionSink {
    static constexpr size_t kCapacity = 1024;
    struct Item { char line[2048]; };
    Item items_[kCapacity]{};
    SRWLOCK lock_ = SRWLOCK_INIT;
    size_t begin_ = 0, count_ = 0;
    std::atomic<unsigned long long> sequence_{0}, lost_{0};
    wchar_t directory_[MAX_PATH]{}, session_[96]{};
    HANDLE file_ = INVALID_HANDLE_VALUE;
    unsigned part_ = 0;
    unsigned long long bytes_ = 0, retry_at_ = 0;
    bool configured_ = false;

    bool Write(const char* text) {
        DWORD written = 0;
        const DWORD size = static_cast<DWORD>(strlen(text));
        if (!WriteFile(file_, text, size, &written, nullptr) || written != size) {
            CloseHandle(file_); file_ = INVALID_HANDLE_VALUE;
            return false;
        }
        bytes_ += written;
        return true;
    }
public:
    ~ActionSink() { if (file_ != INVALID_HANDLE_VALUE) CloseHandle(file_); }
    void Configure(const wchar_t* directory) {
        wcscpy_s(directory_, directory);
        SYSTEMTIME st{}; GetSystemTime(&st);
        _snwprintf_s(session_, _TRUNCATE, L"%04u%02u%02u-%02u%02u%02u-%03u-%lu",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
            st.wMilliseconds, GetCurrentProcessId());
        configured_ = true;
        Add(dlss5::eventlog::kSystem, "[action] journal-start schema=1 clock=UTC sequence=enqueue_order; observed state is not user intent");
    }
    void Add(uint32_t source, const char* text) noexcept {
        if (!text) return;
        const DWORD error = GetLastError();
        if (!TryAcquireSRWLockExclusive(&lock_)) {
            lost_.fetch_add(1); SetLastError(error); return;
        }
        const auto seq = sequence_.fetch_add(1) + 1;
        if (count_ == kCapacity) {
            lost_.fetch_add(1); ReleaseSRWLockExclusive(&lock_); SetLastError(error); return;
        }
        auto& item = items_[(begin_ + count_) % kCapacity];
        SYSTEMTIME st{}; GetSystemTime(&st);
        LARGE_INTEGER qpc{}; QueryPerformanceCounter(&qpc);
        const bool truncated = strlen(text) > 1600;
        _snprintf_s(item.line, _TRUNCATE,
            "%04u-%02u-%02uT%02u:%02u:%02u.%03uZ seq=%llu qpc=%lld pid=%lu tid=%lu source=%s truncated=%u %.1600s\n",
            st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond,st.wMilliseconds,
            seq,qpc.QuadPart,GetCurrentProcessId(),GetCurrentThreadId(),
            dlss5::eventlog::SourceName(source),truncated?1u:0u,text);
        for (char* p=item.line; *p; ++p) if ((*p=='\n' && p[1]) || *p=='\r') *p=' ';
        ++count_; ReleaseSRWLockExclusive(&lock_); SetLastError(error);
    }
    // Called only by the maintenance worker, outside graphics/runtime locks.
    void Flush() {
        if (!configured_) return;
        const auto now = GetTickCount64();
        if (now < retry_at_) return;
        ULARGE_INTEGER free{};
        if (!GetDiskFreeSpaceExW(directory_, &free, nullptr, nullptr) || free.QuadPart < (512ull<<20)) {
            retry_at_ = now + 5000; return;
        }
        if (file_ != INVALID_HANDLE_VALUE && bytes_ >= (16ull<<20)) {
            CloseHandle(file_); file_=INVALID_HANDLE_VALUE;
        }
        if (file_ == INVALID_HANDLE_VALUE) {
            wchar_t path[MAX_PATH]{};
            if (_snwprintf_s(path,_TRUNCATE,L"%ls\\dlss5-actions-%ls-%04u.log",directory_,session_,part_++)<0) {
                retry_at_=now+5000; return;
            }
            file_=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(file_==INVALID_HANDLE_VALUE) {retry_at_=now+5000;return;}
            bytes_=0;
            LARGE_INTEGER frequency{}; QueryPerformanceFrequency(&frequency);
            char head[192];
            _snprintf_s(head,_TRUNCATE,"# action-journal schema=1 qpc_frequency=%lld pid=%lu; requested/config/effective are distinct\n",frequency.QuadPart,GetCurrentProcessId());
            if (!Write(head)) {retry_at_=now+5000;return;}
        }
        const auto lost=lost_.exchange(0);
        if(lost) {
            char gap[192]; SYSTEMTIME st{}; GetSystemTime(&st);
            _snprintf_s(gap,_TRUNCATE,"%04u-%02u-%02uT%02u:%02u:%02u.%03uZ [action-gap] lost=%llu reason=queue_contention_or_capacity_or_write_failure\n",
                st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond,st.wMilliseconds,lost);
            if(!Write(gap)) {lost_.fetch_add(lost);retry_at_=now+5000;return;}
        }
        for(size_t n=0;n<kCapacity;++n) {
            Item item{};
            AcquireSRWLockExclusive(&lock_);
            if(!count_) {ReleaseSRWLockExclusive(&lock_);break;}
            item=items_[begin_]; begin_=(begin_+1)%kCapacity; --count_;
            ReleaseSRWLockExclusive(&lock_);
            if(!Write(item.line)) {lost_.fetch_add(1);retry_at_=now+5000;break;}
        }
    }
};
