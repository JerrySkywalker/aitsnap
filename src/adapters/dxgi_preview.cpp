// SPDX-License-Identifier: Apache-2.0
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <roapi.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include "dxgi_preview.h"
#include "win_capture.h"
#include <QDateTime>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <map>

namespace aitsnap {
using Microsoft::WRL::ComPtr;
namespace Capture=ABI::Windows::Graphics::Capture;
namespace DirectX=ABI::Windows::Graphics::DirectX;
namespace {
using Clock = std::chrono::steady_clock;
std::int64_t nowNs()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count();
}
std::string dxgiReason(const char* stage,HRESULT status)
{
    char code[12]{};
    std::snprintf(code,sizeof(code),"0x%08lX",static_cast<unsigned long>(status));
    return std::string(stage)+" "+code;
}

struct Library {
    HMODULE handle;
    explicit Library(const wchar_t* name) : handle(LoadLibraryExW(name, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32)) {}
    ~Library() { if (handle) FreeLibrary(handle); }
    template<class T> T function(const char* name) const { return reinterpret_cast<T>(GetProcAddress(handle, name)); }
};

struct Runtime {
    Library library{L"combase.dll"};
    decltype(&RoGetActivationFactory) factory=library.function<decltype(&RoGetActivationFactory)>("RoGetActivationFactory");
    decltype(&WindowsCreateStringReference) reference=library.function<decltype(&WindowsCreateStringReference)>("WindowsCreateStringReference");
    bool initialized{};
    Runtime() {
        const auto initialize=library.function<decltype(&RoInitialize)>("RoInitialize");
        initialized=initialize && SUCCEEDED(initialize(RO_INIT_MULTITHREADED));
    }
    ~Runtime() { if(initialized)library.function<decltype(&RoUninitialize)>("RoUninitialize")(); }
    template<class T,std::size_t N> HRESULT activate(const wchar_t (&name)[N],ComPtr<T>& result) const {
        if(!initialized || !reference || !factory)return E_NOINTERFACE;
        HSTRING_HEADER header{};HSTRING value{};
        const auto hr=reference(name,UINT32(N-1),&header,&value);
        return FAILED(hr)?hr:factory(value,IID_PPV_ARGS(&result));
    }
};

struct GraphicsStream {
    ComPtr<Capture::IDirect3D11CaptureFramePool> pool;
    ComPtr<Capture::IGraphicsCaptureSession> session;
    ~GraphicsStream() {
        ComPtr<ABI::Windows::Foundation::IClosable> close;
        if(session && SUCCEEDED(session.As(&close)))close->Close();
        if(pool && SUCCEEDED(pool.As(&close)))close->Close();
    }
};

struct Output {
    core::Rect bounds;
    Clock::time_point retryAt{};
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGIOutputDuplication> duplication;
    std::unique_ptr<GraphicsStream> graphics;
    HMONITOR monitor{};
    struct Readback {
        ComPtr<ID3D11Texture2D> texture;
        core::DisplayFrame frame;
        bool pending{};
    };
    std::array<Readback,2> readbacks;
    core::Rect area;
    core::DisplayFrame latest;
    core::DisplayFrame compatible;
    Clock::time_point compatibleAt{};
    std::string reason;
};
}

struct DxgiPreview::State {
    Library dxgi{L"dxgi.dll"}, d3d{L"d3d11.dll"};
    decltype(&CreateDXGIFactory1) createFactory = dxgi.function<decltype(&CreateDXGIFactory1)>("CreateDXGIFactory1");
    decltype(&D3D11CreateDevice) createDevice = d3d.function<decltype(&D3D11CreateDevice)>("D3D11CreateDevice");
    WinCaptureBackend compatibility;
    std::map<std::string, Output> outputs;
    std::vector<core::Display> inventory;
    Clock::time_point refreshAt{};
    bool fallback{};
    std::string reason;

    bool openGraphics(Output& result)
    {
        thread_local Runtime runtime;
        const auto fail=[&](const char* stage,HRESULT status){result.reason=dxgiReason(stage,status);return false;};
        const auto wrap=d3d.function<decltype(&CreateDirect3D11DeviceFromDXGIDevice)>("CreateDirect3D11DeviceFromDXGIDevice");
        if(!wrap)return fail("Graphics Capture D3D11",E_NOINTERFACE);
        ComPtr<IGraphicsCaptureItemInterop> interop;
        auto hr=runtime.activate(RuntimeClass_Windows_Graphics_Capture_GraphicsCaptureItem,interop);
        if(FAILED(hr))return fail("GraphicsCaptureItem",hr);
        ComPtr<Capture::IGraphicsCaptureItem> item;
        hr=interop->CreateForMonitor(result.monitor,IID_PPV_ARGS(&item));
        if(FAILED(hr))return fail("CreateForMonitor",hr);
        ComPtr<IDXGIDevice> device;
        ComPtr<IInspectable> inspectable;
        ComPtr<DirectX::Direct3D11::IDirect3DDevice> wrapped;
        if(FAILED(hr=result.device.As(&device)) || FAILED(hr=wrap(device.Get(),&inspectable)) ||
           FAILED(hr=inspectable.As(&wrapped)))return fail("Graphics Capture device",hr);
        ComPtr<Capture::IDirect3D11CaptureFramePoolStatics2> factory;
        hr=runtime.activate(RuntimeClass_Windows_Graphics_Capture_Direct3D11CaptureFramePool,factory);
        if(FAILED(hr))return fail("Capture frame pool",hr);
        ABI::Windows::Graphics::SizeInt32 size{};
        if(FAILED(hr=item->get_Size(&size)))return fail("Graphics Capture size",hr);
        auto stream=std::make_unique<GraphicsStream>();
        hr=factory->CreateFreeThreaded(wrapped.Get(),DirectX::DirectXPixelFormat_B8G8R8A8UIntNormalized,2,size,&stream->pool);
        if(FAILED(hr))return fail("CreateFreeThreaded",hr);
        if(FAILED(hr=stream->pool->CreateCaptureSession(item.Get(),&stream->session)))return fail("Create monitor capture",hr);
        ComPtr<Capture::IGraphicsCaptureSession5> cadence;
        if(SUCCEEDED(stream->session.As(&cadence)))cadence->put_MinUpdateInterval({0});
        if(FAILED(hr=stream->session->StartCapture()))return fail("Start monitor capture",hr);
        result.duplication.Reset();result.graphics=std::move(stream);return true;
    }

    bool open(Output& result)
    {
        const auto failed=[&](const char* stage,HRESULT status){
            result.reason=dxgiReason(stage,status);return false;
        };
        ComPtr<IDXGIFactory1> factory;
        if(!createFactory || !createDevice)return failed("DXGI/D3D11",HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND));
        if(const auto hr=createFactory(IID_PPV_ARGS(&factory));FAILED(hr))return failed("CreateDXGIFactory1",hr);
        ComPtr<IDXGIAdapter1> adapter;
        for (UINT a = 0; factory->EnumAdapters1(a, adapter.ReleaseAndGetAddressOf()) == S_OK; ++a) {
            ComPtr<IDXGIOutput> output;
            for (UINT i = 0; adapter->EnumOutputs(i, output.ReleaseAndGetAddressOf()) == S_OK; ++i) {
                DXGI_OUTPUT_DESC desc{};
                if (FAILED(output->GetDesc(&desc)) || !desc.AttachedToDesktop) continue;
                const auto& r = desc.DesktopCoordinates;
                if (result.bounds != core::Rect{r.left, r.top, r.right - r.left, r.bottom - r.top}) continue;
                result.monitor=desc.Monitor;
                const auto deviceStatus=createDevice(adapter.Get(),D3D_DRIVER_TYPE_UNKNOWN,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                                    nullptr,0,D3D11_SDK_VERSION,&result.device,nullptr,&result.context);
                if(FAILED(deviceStatus))return failed("D3D11CreateDevice",deviceStatus);
                ComPtr<IDXGIOutput1> output1;
                if(desc.Rotation!=DXGI_MODE_ROTATION_IDENTITY || FAILED(output.As(&output1)))return openGraphics(result);
                const auto duplicationStatus=output1->DuplicateOutput(result.device.Get(),&result.duplication);
                if(SUCCEEDED(duplicationStatus))return true;
                return duplicationStatus==E_ACCESSDENIED?failed("DuplicateOutput",duplicationStatus):openGraphics(result);
            }
        }
        return failed("Match DXGI output",DXGI_ERROR_NOT_FOUND);
    }
};

DxgiPreview::DxgiPreview() : state_(std::make_unique<State>()) {}
DxgiPreview::~DxgiPreview() = default;
void DxgiPreview::reset()
{
    state_->outputs.clear();
    state_->refreshAt={};
}
void DxgiPreview::retainOutputs(const std::vector<core::Region>& regions,const std::vector<core::Rect>& blocked)
{
    std::erase_if(state_->outputs,[&](const auto& entry){
        return std::none_of(regions.begin(),regions.end(),[&](const auto& region){
            return region.enabled && core::intersect(entry.second.bounds,region.rect) &&
                std::none_of(blocked.begin(),blocked.end(),[&](const auto& rect){return core::intersect(region.rect,rect).has_value();});
        });
    });
}
std::vector<core::Display> DxgiPreview::displays()
{
    state_->fallback = false;
    state_->reason.clear();
    if (Clock::now() < state_->refreshAt) return state_->inventory;
    state_->inventory = state_->compatibility.displays();
    state_->refreshAt = Clock::now() + std::chrono::milliseconds(500);
    std::erase_if(state_->outputs, [&](const auto& entry) {
        for (const auto& display : state_->inventory)
            if (display.id == entry.first && display.rect == entry.second.bounds) return false;
        return true;
    });
    return state_->inventory;
}
bool DxgiPreview::usesCompatibilityCapture() const { return state_->fallback; }
std::string DxgiPreview::compatibilityReason() const { return state_->reason; }
core::DisplayFrame DxgiPreview::acquire(const core::Display& display) { return acquireRegion(display, display.rect); }
core::DisplayFrame DxgiPreview::acquireRegion(const core::Display& display, const core::Rect& area)
{
    core::DisplayFrame frame;
    frame.display = display;
    frame.display.rect = area;
    if (const auto status = interactiveDesktopStatus(); status) {
        state_->outputs.clear();
        frame.error = status;
        return frame;
    }
    auto& output = state_->outputs[display.id];
    const auto compatibilityFrame = [&] {
        state_->fallback = true;
        state_->reason = output.reason;
        const auto now=Clock::now();
        if (output.compatible.display.rect==area && output.compatible.image.valid() &&
            now-output.compatibleAt<std::chrono::milliseconds(200)) return output.compatible;
        const std::unique_lock guard(desktopCaptureMutex(), std::try_to_lock);
        if (guard) {
            output.compatible=state_->compatibility.acquire(frame.display);
            output.compatibleAt=now;
            return output.compatible;
        }
        frame.error = {"CAPTURE_BUSY", "正式截图优先"};
        return frame;
    };
    const auto reset = [&] {
        auto reason=std::move(output.reason);
        output = {};
        output.reason=std::move(reason);
        output.bounds = display.rect;
        output.retryAt = Clock::now() + std::chrono::seconds(1);
    };
    if (!output.duplication && !output.graphics) {
        if (Clock::now() < output.retryAt) return compatibilityFrame();
        output.bounds = display.rect;
        if (!state_->open(output)) { reset(); return compatibilityFrame(); }
    }
    if(output.area!=area){output.area=area;output.readbacks={};output.latest={};}
    auto* first=&output.readbacks[0];
    auto* second=&output.readbacks[1];
    if(first->frame.acquisitionEndNs<second->frame.acquisitionEndNs)std::swap(first,second);
    for(auto* slot:{first,second}){
        if(!slot->pending)continue;
        D3D11_MAPPED_SUBRESOURCE mapped{};
        const auto hr=output.context->Map(slot->texture.Get(),0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&mapped);
        if(hr==DXGI_ERROR_WAS_STILL_DRAWING)continue;
        if(FAILED(hr)){output.reason=dxgiReason("Read staging texture",hr);reset();return compatibilityFrame();}
        struct Mapped { ID3D11DeviceContext* context; ID3D11Texture2D* texture; ~Mapped(){context->Unmap(texture,0);} } release{output.context.Get(),slot->texture.Get()};
        slot->pending=false;
        if(slot->frame.acquisitionEndNs<=output.latest.acquisitionEndNs)continue;
        const auto stride=std::size_t(area.width)*4;
        auto pixels=std::make_shared<std::vector<std::uint8_t>>(stride*area.height);
        for(int row=0;row<area.height;++row)
            std::memcpy(pixels->data()+row*stride,static_cast<const std::uint8_t*>(mapped.pData)+row*mapped.RowPitch,stride);
        for(std::size_t alpha=3;alpha<pixels->size();alpha+=4)(*pixels)[alpha]=255;
        if(output.latest.image.valid() && *pixels==*output.latest.image.bgra)continue;
        slot->frame.image={area.width,area.height,std::move(pixels)};
        output.latest=std::move(slot->frame);
    }
    auto slot=std::find_if(output.readbacks.begin(),output.readbacks.end(),[](const auto& item){return !item.pending;});
    const auto latest=[&] {
        if(output.latest.image.valid())return output.latest;
        frame.error={"PREVIEW_WAIT","正在等待桌面更新"};return frame;
    };
    if(slot==output.readbacks.end())return latest();
    frame.acquisitionStartNs = nowNs();
    struct Acquired { ComPtr<IDXGIOutputDuplication> output; ~Acquired() { if(output)output->ReleaseFrame(); } } acquired;
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<Capture::IDirect3D11CaptureFrame> captured;
    if(output.graphics){
        auto hr=output.graphics->pool->TryGetNextFrame(&captured);
        if(SUCCEEDED(hr) && !captured)return latest();
        ComPtr<DirectX::Direct3D11::IDirect3DSurface> surface;
        ComPtr<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess> access;
        ABI::Windows::Graphics::SizeInt32 size{};
        if(SUCCEEDED(hr))hr=captured->get_ContentSize(&size);
        if(SUCCEEDED(hr) && (size.Width!=display.rect.width || size.Height!=display.rect.height))hr=DXGI_ERROR_ACCESS_LOST;
        if(SUCCEEDED(hr))hr=captured->get_Surface(&surface);
        if(SUCCEEDED(hr))hr=surface.As(&access);
        if(SUCCEEDED(hr))hr=access->GetInterface(IID_PPV_ARGS(&texture));
        if(FAILED(hr)){output.reason=dxgiReason("Graphics Capture frame",hr);state_->refreshAt={};reset();return compatibilityFrame();}
    }else{
        DXGI_OUTDUPL_FRAME_INFO info{};
        ComPtr<IDXGIResource> resource;
        const auto status=output.duplication->AcquireNextFrame(0,&info,&resource);
        if(status==DXGI_ERROR_WAIT_TIMEOUT)return latest();
        if(FAILED(status)){
            output.reason=dxgiReason("AcquireNextFrame",status);
            if(status==DXGI_ERROR_ACCESS_LOST && state_->openGraphics(output))return latest();
            state_->refreshAt={};reset();return compatibilityFrame();
        }
        acquired.output=output.duplication;
        if((output.latest.image.valid() || first->pending || second->pending) && info.LastPresentTime.QuadPart==0)return latest();
        if(const auto hr=resource.As(&texture);FAILED(hr)){output.reason=dxgiReason("Desktop texture",hr);reset();return compatibilityFrame();}
    }
    if (!slot->texture) {
        D3D11_TEXTURE2D_DESC desc{};
        texture->GetDesc(&desc);
        desc.Width = area.width; desc.Height = area.height;
        desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags = 0; desc.MiscFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        if (const auto hr=output.device->CreateTexture2D(&desc,nullptr,slot->texture.ReleaseAndGetAddressOf());FAILED(hr)) {
            output.reason=dxgiReason("Staging texture",hr);reset();return compatibilityFrame();
        }
    }
    const UINT x = area.x - display.rect.x, y = area.y - display.rect.y;
    const D3D11_BOX box{x, y, 0, x + UINT(area.width), y + UINT(area.height), 1};
    output.context->CopySubresourceRegion(slot->texture.Get(),0,0,0,0,texture.Get(),0,&box);
    output.context->Flush();
    frame.acquisitionEndNs = nowNs();
    frame.acquiredAtUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString();
    slot->frame=frame;slot->pending=true;
    return latest();
}
}
