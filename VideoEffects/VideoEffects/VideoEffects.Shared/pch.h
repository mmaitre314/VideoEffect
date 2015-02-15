#pragma once

#include <algorithm>
#include <sstream>

#include <collection.h>
#include <ppltasks.h>

#include <strsafe.h>

#include <wrl.h>

#include <robuffer.h>

#include <d3d11_2.h>

#include <mfapi.h>
#include <mfidl.h>
#include <Mferror.h>
#include <mfreadwrite.h>

#include <windows.media.h>

//
// Asserts
//

#ifndef NDEBUG
#define NT_ASSERT(expr) if (!(expr)) { __debugbreak(); }
#else
#define NT_ASSERT(expr)
#endif

//
// Tracing
//

#include "Events\Logger.h"
#include "DebuggerLogger.h"

#define Trace(format, ...) { \
    if(s_logger.IsEnabled(LogLevel::Information)) { s_logger.Log(__FUNCTION__, LogLevel::Information, format, __VA_ARGS__); } \
    Logger.Info("%s " ## format, __FUNCTION__, __VA_ARGS__); \
}
#define TraceError(format, ...) { \
    if(s_logger.IsEnabled(LogLevel::Error)) { s_logger.Log(__FUNCTION__, LogLevel::Error, format, __VA_ARGS__); } \
    Logger.Error("%s " ## format, __FUNCTION__, __VA_ARGS__); \
}

//
// Error handling
//

// Exception-based error handling
#define CHK(statement)  {HRESULT _hr = (statement); if (FAILED(_hr)) { throw ref new Platform::COMException(_hr); };}
#define CHKNULL(p)  {if ((p) == nullptr) { throw ref new Platform::NullReferenceException(L#p); };}
#define CHKOOM(p)  {if ((p) == nullptr) { throw ref new Platform::OutOfMemoryException(L#p); };}

// Exception-free error handling
#define CHK_RETURN(statement) {hr = (statement); if (FAILED(hr)) { return hr; };}

//
// Error origin
//

namespace Details
{
    class ErrorOrigin
    {
    public:

        template <size_t N, size_t L>
        static HRESULT TracedOriginateError(_In_ char const (&function)[L], _In_ HRESULT hr, _In_ wchar_t const (&str)[N])
        {
            if (FAILED(hr))
            {
                s_logger.Log(function, LogLevel::Error, "failed hr=%08X: %S", hr, str);
                ::RoOriginateErrorW(hr, N - 1, str);
            }
            return hr;
        }

        // A method to track error origin
        template <size_t L>
        static HRESULT TracedOriginateError(_In_ char const (&function)[L], __in HRESULT hr)
        {
            if (FAILED(hr))
            {
                s_logger.Log(function, LogLevel::Error, "failed hr=%08X", hr);
                ::RoOriginateErrorW(hr, 0, nullptr);
            }
            return hr;
        }

        ErrorOrigin() = delete;
    };
}

#define OriginateError(_hr, ...) ::Details::ErrorOrigin::TracedOriginateError(__FUNCTION__, _hr, __VA_ARGS__)

//
// Exception boundary (converts exceptions into HRESULTs)
//

namespace Details
{
    template<size_t L /*= sizeof(__FUNCTION__)*/>
    class TracedExceptionBoundary
    {
    public:
        TracedExceptionBoundary(_In_ const char *function /*= __FUNCTION__*/)
            : _function(function)
        {
        }

        TracedExceptionBoundary(const TracedExceptionBoundary&) = delete;
        TracedExceptionBoundary& operator=(const TracedExceptionBoundary&) = delete;

        HRESULT operator()(std::function<void()>&& lambda)
        {
            s_logger.Log(_function, L, LogLevel::Verbose, "boundary enter");

            HRESULT hr = S_OK;
            try
            {
                lambda();
            }
#ifdef _INC_COMDEF // include comdef.h to enable
            catch (const _com_error& e)
            {
                hr = e.Error();
            }
#endif
#ifdef __cplusplus_winrt // enable C++/CX to use (/ZW)
            catch (Platform::Exception^ e)
            {
                hr = e->HResult;
            }
#endif
            catch (const std::bad_alloc&)
            {
                hr = E_OUTOFMEMORY;
            }
            catch (const std::out_of_range&)
            {
                hr = E_BOUNDS;
            }
            catch (const std::exception& e)
            {
                s_logger.Log(_function, L, LogLevel::Error, "caught unknown STL exception: %s", e.what());
                hr = E_FAIL;
            }
            catch (...)
            {
                s_logger.Log(_function, L, LogLevel::Error, "caught unknown exception");
                hr = E_FAIL;
            }

            if (FAILED(hr))
            {
                s_logger.Log(_function, L, LogLevel::Error, "boundary exit - failed hr=%08X", hr);
            }
            else
            {
                s_logger.Log(_function, L, LogLevel::Verbose, "boundary exit");
            }

            return hr;
        }

    private:
        const char* _function;
    };
}

#define ExceptionBoundary ::Details::TracedExceptionBoundary<sizeof(__FUNCTION__)>(__FUNCTION__)

//
// Casting
//

template<typename T, typename U>
Microsoft::WRL::ComPtr<T> As(const Microsoft::WRL::ComPtr<U>& in)
{
    Microsoft::WRL::ComPtr<T> out;
    CHK(in.As(&out));
    return out;
}

template<typename T, typename U>
Microsoft::WRL::ComPtr<T> As(U* in)
{
    Microsoft::WRL::ComPtr<T> out;
    CHK(in->QueryInterface(IID_PPV_ARGS(&out)));
    return out;
}

template<typename T, typename U>
Microsoft::WRL::ComPtr<T> As(U^ in)
{
    Microsoft::WRL::ComPtr<T> out;
    CHK(reinterpret_cast<IInspectable*>(in)->QueryInterface(IID_PPV_ARGS(&out)));
    return out;
}

//
// IBuffer data access
//

inline unsigned char* GetData(Windows::Storage::Streams::IBuffer^ buffer)
{
    unsigned char* bytes = nullptr;
    Microsoft::WRL::ComPtr<::Windows::Storage::Streams::IBufferByteAccess> bufferAccess;
    CHK(((IUnknown*)buffer)->QueryInterface(IID_PPV_ARGS(&bufferAccess)));
    CHK(bufferAccess->Buffer(&bytes));
    return bytes;
}

//
// Add a local IVideoEffectDefinition on Winows 8.1 
// (interface was added in Windows Phone 8.1)
//

namespace VideoEffects
{
#if WINAPI_FAMILY!=WINAPI_FAMILY_PHONE_APP
    public interface class IVideoEffectDefinition
    {
        property Platform::String^ ActivatableClassId { Platform::String^ get(); }
        property Windows::Foundation::Collections::IPropertySet^ Properties { Windows::Foundation::Collections::IPropertySet^ get(); }
    };
#endif
}

//
// Exception-safe PROPVARIANT
//

class PropVariant : public PROPVARIANT
{
public:
    PropVariant()
    {
        PropVariantInit(this);
    }
    ~PropVariant()
    {
        (void)PropVariantClear(this);
    }

    PropVariant(const PropVariant&) = delete;
    PropVariant& operator&(const PropVariant&) = delete;
};

//
// IPropertySet helpers
//

inline unsigned int GetUInt32(_In_ Windows::Foundation::Collections::IMap<Platform::String^, Platform::Object^>^ properties, _In_ Platform::String^ key, _In_ unsigned int defaultValue)
{
    unsigned int value = defaultValue;
    if (properties->HasKey(key))
    {
        Platform::Object^ boxedValue = properties->Lookup(key);
        if (Platform::Type::GetTypeCode(boxedValue->GetType()) == Platform::TypeCode::UInt32)
        {
            value = (unsigned int)boxedValue;
        }
    }
    return value;
}
