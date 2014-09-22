#include "pch.h"
#include "DebuggerLogger.h"

DebuggerLogger::DebuggerLogger()
    : _level(LogLevel::Error)
{
    OutputDebugStringA("Starting tracing\n");
}

void DebuggerLogger::_Log(_In_reads_(L) const char *function, _In_ size_t L, _In_ PCSTR format, va_list args)
{
    char message[2048];
    size_t pos = 0;
    size_t size = sizeof(message);

    // Trace header: function name
    if (0 == memcpy_s(message + pos, size, function, L - 1))
    {
        pos += L - 1;
        size -= L - 1;
    }

    if (size >= 1)
    {
        message[pos] = ' ';
        pos++;
        size--;
    }

    // Trace message
    int cch = vsnprintf_s(message + pos, size, _TRUNCATE, format, args);
    if (cch >= 0)
    {
        pos += cch;
        size -= cch;
    }
    else
    {
        pos += size;
        size = 0;
    }

    // Enforce line return and null termination
    if (size < 2)
    {
        pos -= 2;
        size += 2;
    }
    message[pos] = '\n';
    message[pos + 1] = '\0';

    OutputDebugStringA(message);
}
