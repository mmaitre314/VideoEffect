#pragma once

class D3D11DeviceLock
{
public:
    D3D11DeviceLock(_In_ const ::Microsoft::WRL::ComPtr<IMFDXGIDeviceManager>& deviceManager)
        : _handle(nullptr)
        , _deviceManager(deviceManager)
    {
        CHK(deviceManager->OpenDeviceHandle(&_handle));
        CHK(deviceManager->LockDevice(_handle, IID_PPV_ARGS(&_device), /*block*/true));
    }

    ~D3D11DeviceLock()
    {
        if (_device != nullptr)
        {
            (void)_deviceManager->UnlockDevice(_handle, /*saveState*/false);
        }
        if (_handle != nullptr)
        {
            (void)_deviceManager->CloseDeviceHandle(_handle);
        }
    }

    ID3D11Device* operator->() const
    {
        return _device.Get();
    }

    ::Microsoft::WRL::ComPtr<ID3D11Device> Get() const
    {
        return _device;
    }

private:
    HANDLE _handle;
    ::Microsoft::WRL::ComPtr<ID3D11Device> _device;
    ::Microsoft::WRL::ComPtr<IMFDXGIDeviceManager> _deviceManager;
};
