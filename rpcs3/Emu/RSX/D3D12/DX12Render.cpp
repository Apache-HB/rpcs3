#if 0
#include "DX12Render.h"

#include <d3d11on12.h>
#include <d3dcompiler.h>
#include <dxgi1_4.h>

using namespace Microsoft::WRL;

class DX12Render : public GSRender
{
	using Super = GSRender;

	using Device                       = ID3D12Device;
	using Queue                        = ID3D12CommandQueue;
	using SwapChain                    = IDXGISwapChain;
	using Resource                     = ID3D12Resource;
	using DescriptorHeap               = ID3D12DescriptorHeap;
	static constexpr u32 backbufferLen = 2;

public:
	DX12Render();
	virtual ~DX12Render();

	u64 get_cycles() override final
	{
		return thread_ctrl::get_cycles(static_cast<named_thread<DX12Render>&>(*this));
	}

	bool do_method(u32 cmd, u32 value) override final;

	void end() override
	{
		thread::end();
	}

	void on_exit() override;
	void on_init_thread() override;

private:
	static void LoadModules();
	static void UnloadModules();

	void InitDevice();
	void InitQueue();
	void InitSwapChain();
	void InitRenderTargets();

	void ShutDown();
	ComPtr<Device> m_device;
	ComPtr<Queue> m_queue;
	ComPtr<SwapChain> m_swapChain;

	ComPtr<Resource> m_backbuffer[backbufferLen];
	ComPtr<DescriptorHeap> m_backbufferDescriptors[backbufferLen];

	int m_currentBackbuffer = 0;
	int32_t m_targetViewDescriptorSize;
};

//	directx 12 runtime module, directx 11 runtime module
//	and the directx shader compiler module
static HMODULE DX12, DX11, DCC;

static PFN_D3D12_CREATE_DEVICE DX12CreateDevice;
static PFN_D3D12_GET_DEBUG_INTERFACE DX12GetDebugInterface;
static PFN_D3D12_SERIALIZE_ROOT_SIGNATURE DX12SerializeRootSignature;
static PFN_D3D11ON12_CREATE_DEVICE DX11On12CreateDevice;
static pD3DCompile DXCompile;

DX12Render::DX12Render()
{
	LoadModules();
	InitDevice();
	InitQueue();
	InitSwapChain();
}

DX12Render::~DX12Render()
{
	UnloadModules();
}

bool DX12Render::do_method(u32 cmd, u32 value)
{
	return false;
}

void DX12Render::on_exit()
{
	Super::on_exit();
}

void DX12Render::on_init_thread()
{
}

inline void DX12Render::LoadModules()
{
	DX12Render::DX12           = LoadLibrary(TEXT("d3d12.dll"));
	DX12CreateDevice           = (PFN_D3D12_CREATE_DEVICE)GetProcAddress(DX12, "D3D12CreateDevice");
	DX12GetDebugInterface      = (PFN_D3D12_GET_DEBUG_INTERFACE)GetProcAddress(DX12, "D3D12GetDebugInterface");
	DX12SerializeRootSignature = (PFN_D3D12_SERIALIZE_ROOT_SIGNATURE)GetProcAddress(DX12, "D3D12SerializeRootSignature");

	DX12Render::DX11     = LoadLibrary(TEXT("d3d11.dll"));
	DX11On12CreateDevice = (PFN_D3D11ON12_CREATE_DEVICE)GetProcAddress(DX11, "D3D11On12CreateDevice");

	DX12Render::DCC = LoadLibrary(TEXT("d3dcompiler_47.dll"));
	DXCompile       = (pD3DCompile)GetProcAddress(DCC, "D3DCompile");
}

inline void DX12Render::UnloadModules()
{
	FreeLibrary(DX12);
	FreeLibrary(DX11);
	FreeLibrary(DCC);
}

void DX12Render::InitDevice()
{
	//	create a dx12 device to render with
	auto Result = DX12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&m_device));

	if (FAILED(Result))
		throw std::runtime_error("Failed to create DX12 Device");
}

void DX12Render::InitQueue()
{
	D3D12_COMMAND_QUEUE_DESC Desc = {};
	Desc.Flags                    = D3D12_COMMAND_QUEUE_FLAG_NONE;
	Desc.Type                     = D3D12_COMMAND_LIST_TYPE_DIRECT;

	auto Result = m_device->CreateCommandQueue(&Desc, IID_PPV_ARGS(&m_queue));

	if (FAILED(Result))
		throw std::runtime_error("Failed to create DX12 command queue");
}

void DX12Render::InitSwapChain()
{
	ComPtr<IDXGIFactory4> Factory;
	auto Result = CreateDXGIFactory1(IID_PPV_ARGS(&Factory));

	if (FAILED(Result))
		throw std::runtime_error("Failed to create DXGI factory");

	DXGI_SWAP_CHAIN_DESC Swap = {};

	Swap.BufferCount       = m_currentBackbuffer;
	Swap.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	Swap.BufferUsage       = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	Swap.BufferDesc.Width  = get_frame()->client_width();
	Swap.BufferDesc.Height = get_frame()->client_height();
	Swap.OutputWindow      = get_frame()->handle();
	Swap.SampleDesc.Count  = 1;
	Swap.SwapEffect        = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	Swap.Windowed          = true;

	Result = Factory->CreateSwapChain(m_queue.Get(), &Swap, &m_swapChain);

	if (FAILED(Result))
		throw std::runtime_error("Falied to create DX12 swap chain");
}

void DX12Render::InitRenderTargets()
{
	m_targetViewDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
}

void DX12Render::ShutDown()
{
}

#endif