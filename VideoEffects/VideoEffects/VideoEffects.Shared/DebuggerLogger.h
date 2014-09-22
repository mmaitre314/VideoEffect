#pragma once

//
// Logging to debugger's Output window
//

enum class LogLevel
{
    Critical = 1,
    Error,
    Warning,
    Information,
    Verbose
};

class DebuggerLogger
{
public:

    DebuggerLogger();

    bool IsEnabled(_In_ LogLevel level)
    {
        return (level <= _level) && ::IsDebuggerPresent();
    }

    template <size_t L>
    void Log(_In_ char const (&function)[L], _In_ LogLevel level, _In_ PCSTR format, ...)
    {
        if (!IsEnabled(level))
        {
            return;
        }

        va_list args;
        va_start(args, format);
        _Log(function, L, format, args);
        va_end(args);
    }

    void Log(_In_reads_(L) const char *function, _In_ size_t L, _In_ LogLevel level, _In_ PCSTR format, ...)
    {
        if (!IsEnabled(level))
        {
            return;
        }

        va_list args;
        va_start(args, format);
        _Log(function, L, format, args);
        va_end(args);
    }

private:

    void _Log(_In_reads_(L) const char *function, _In_ size_t L, _In_ PCSTR format, va_list args);

    LogLevel _level;
};

extern __declspec(selectany) DebuggerLogger s_logger;

#define Trace(format, ...) s_logger.Log(__FUNCTION__, LogLevel::Information, format, __VA_ARGS__)
#define TraceError(format, ...) s_logger.Log(__FUNCTION__, LogLevel::Error, format, __VA_ARGS__)
