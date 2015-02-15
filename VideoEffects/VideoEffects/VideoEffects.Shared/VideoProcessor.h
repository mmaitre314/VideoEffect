#pragma once

// An RAII version of MFT_OUTPUT_DATA_BUFFER
class MftOutputDataBuffer : public MFT_OUTPUT_DATA_BUFFER
{
public:

    MftOutputDataBuffer(_In_opt_ const Microsoft::WRL::ComPtr<IMFSample>& sample)
    {
        this->dwStreamID = 0;
        this->pSample = sample.Get();
        this->dwStatus = 0;
        this->pEvents = nullptr;

        if (this->pSample != nullptr)
        {
            this->pSample->AddRef();
        }
    }

    ~MftOutputDataBuffer()
    {
        if (this->pSample != nullptr)
        {
            this->pSample->Release();
        }
        if (this->pEvents != nullptr)
        {
            this->pEvents->Release();
        }
    }
};

Microsoft::WRL::ComPtr<IMFTransform> CreateVideoProcessor();
