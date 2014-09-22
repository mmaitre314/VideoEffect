#include "pch.h"
#include "MediaTypeFormatter.h"
#include "SampleFormatter.h"

using namespace std;
using namespace Microsoft::WRL;

string SampleFormatter::Format(_In_opt_ const ComPtr<IMFSample>& sample)
{
    if (sample == nullptr)
    {
        return "<null>";
    }

    ostringstream stream;

    long long time;
    if (SUCCEEDED(sample->GetSampleTime(&time)))
    {
        stream << "t=" << time << "hns";
    }
    else
    {
        stream << "t=<none>";
    }

    long long duration;
    if (SUCCEEDED(sample->GetSampleTime(&duration)))
    {
        stream << ",d=" << duration << "hns";
    }
    else
    {
        stream << ",d=<none>";
    }

    unsigned long bufferCount;
    CHK(sample->GetBufferCount(&bufferCount));
    for (unsigned int j = 0; j < bufferCount; j++)
    {
        stream << ",buffer " << j << "=[";

        ComPtr<IMFMediaBuffer> buffer1D;
        CHK(sample->GetBufferByIndex(j, &buffer1D));

        ComPtr<IMFDXGIBuffer> bufferDXGI;
        ComPtr<IMF2DBuffer2> buffer2D;
        if (SUCCEEDED(buffer1D.As(&bufferDXGI)))
        {
            ComPtr<ID3D11Texture2D> texture;
            D3D11_TEXTURE2D_DESC texDesc;
            CHK(bufferDXGI->GetResource(IID_PPV_ARGS(&texture)));
            texture->GetDesc(&texDesc);

            stream << "2D DX,bind=0x" << hex << texDesc.BindFlags << dec << ",usage=" << texDesc.Usage;
        }
        else if (SUCCEEDED(buffer1D.As(&buffer2D)))
        {
            stream << "2D CPU";
        }
        else
        {
            stream << "1D";
        }

        unsigned long length;
        CHK(buffer1D->GetCurrentLength(&length));

        stream << ",len=" << length << "B]";
    }

    unsigned int attributeCount;
    CHK(sample->GetCount(&attributeCount));
    for (unsigned int i = 0; i < attributeCount; i++)
    {
        stream << ',';

        MediaTypeFormatter::AddAttribute(sample, i, stream);
    }

    return stream.str();
}
