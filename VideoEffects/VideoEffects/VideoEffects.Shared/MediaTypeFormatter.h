#pragma once

class MediaTypeFormatter
{
public:

    static std::string FormatMediaType(_In_opt_ const ::Microsoft::WRL::ComPtr<IMFMediaType>& mt);

private:

    static void _AddGuid(_In_ const GUID& guid, _Inout_ std::ostringstream& stream);
    static LPCSTR _GetGuidFriendlyName(_In_ const GUID& guid);
};
