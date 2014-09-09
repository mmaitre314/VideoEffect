#pragma once

// Pieces from https://gist.github.com/ejball/3792072

// Not part of MSDK
const unsigned int COWAIT_DISPATCH_CALLS = 8;

// Not part of MSDK
_Check_return_ WINOLEAPI
CoWaitForMultipleHandles(
    _In_ DWORD dwFlags,
    _In_ DWORD dwTimeout,
    _In_ ULONG cHandles,
    _In_reads_(cHandles) LPHANDLE pHandles,
    _Out_ LPDWORD lpdwindex
);

template <typename T>
T Await(concurrency::task<T> t)
{
    HANDLE hEvent = CreateEventEx(nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, EVENT_ALL_ACCESS);
    if (hEvent == nullptr)
    {
        throw std::bad_alloc();
    }

    T result;
    bool exceptionHandled = false;
    std::wstring exceptionMessage;
    t.then([&hEvent, &exceptionHandled, &exceptionMessage, &result](concurrency::task<T> previousTask){
        try
        {
            result = previousTask.get();
        }
        catch (Platform::Exception^ ex)
        {
            exceptionHandled = true;
            exceptionMessage = std::wstring(ex->Message->Data());
        }
        catch (std::exception & ex)
        {
            exceptionHandled = true;
            exceptionMessage = std::wstring(ex.what(), ex.what() + strlen(ex.what()));
        }
        catch (...)
        {
            exceptionHandled = true;
            exceptionMessage = std::wstring(L"Unrecognized C++ exception.");
        }
        SetEvent(hEvent);
    }, concurrency::task_continuation_context::use_arbitrary());

    DWORD index;
    (void) CoWaitForMultipleHandles(COWAIT_DISPATCH_CALLS, INFINITE, 1, &hEvent, &index);

    CloseHandle(hEvent);

    if (exceptionHandled)
    {
        Microsoft::VisualStudio::CppUnitTestFramework::Assert::Fail(exceptionMessage.c_str());
    }

    return result;
}

template<>
inline void Await<void>(concurrency::task<void> t)
{
    Await(t.then([]{ return true; }));
}

template <typename T>
inline T Await(Windows::Foundation::IAsyncOperation<T>^ t)
{
    return Await(concurrency::task<T>(t));
}

inline void Await(Windows::Foundation::IAsyncAction^ t)
{
    return Await(concurrency::task<void>(t));
}

template <typename T, typename U>
inline T Await(Windows::Foundation::IAsyncOperationWithProgress<T, U>^ t)
{
    return Await(concurrency::task<T>(t));
}

template <typename U>
inline void Await(Windows::Foundation::IAsyncActionWithProgress<U>^ t)
{
    return Await(concurrency::task<void>(t));
}
