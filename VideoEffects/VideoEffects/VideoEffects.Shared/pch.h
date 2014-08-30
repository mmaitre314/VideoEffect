#pragma once

#include <collection.h>
#include <ppltasks.h>

#include <wrl.h>

#include <robuffer.h>

#include <mfapi.h>
#include <mfidl.h>
#include <Mferror.h>

#include <windows.media.h>

//
// Error handling
//

// Exception-based error handling
#define CHK(statement)  {HRESULT _hr = (statement); if (FAILED(_hr)) { throw ref new Platform::COMException(_hr); };}
#define CHKNULL(p)  {if ((p) == nullptr) { throw ref new Platform::NullReferenceException(L#p); };}

// Exception-free error handling
#define CHK_RETURN(statement) {hr = (statement); if (FAILED(hr)) { return hr; };}

// A class to track error origin
template <size_t N>
HRESULT OriginateError(__in HRESULT hr, __in wchar_t const (&str)[N])
{
    if (FAILED(hr))
    {
        ::RoOriginateErrorW(hr, N - 1, str);
    }
    return hr;
}

// A class to track error origin
inline HRESULT OriginateError(__in HRESULT hr)
{
    if (FAILED(hr))
    {
        ::RoOriginateErrorW(hr, 0, nullptr);
    }
    return hr;
}

// Converts exceptions into HRESULTs
template <typename Lambda>
HRESULT ExceptionBoundary(Lambda&& lambda)
{
    try
    {
        lambda();
        return S_OK;
    }
#ifdef _INC_COMDEF // include comdef.h to enable
    catch (const _com_error& e)
    {
        return e.Error();
    }
#endif
#ifdef __cplusplus_winrt // enable C++/CX to use (/ZW)
    catch (Platform::Exception^ e)
    {
        return e->HResult;
    }
#endif
    catch (const std::bad_alloc&)
    {
        return E_OUTOFMEMORY;
    }
    catch (const std::out_of_range&)
    {
        return E_BOUNDS;
    }
    catch (const std::exception&)
    {
        return E_FAIL;
    }
    catch (...)
    {
        return E_FAIL;
    }
}

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
