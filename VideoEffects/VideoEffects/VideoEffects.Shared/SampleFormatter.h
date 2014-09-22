#pragma once

class SampleFormatter
{
public:

    static std::string Format(_In_opt_ const ::Microsoft::WRL::ComPtr<IMFSample>& sample);

};

