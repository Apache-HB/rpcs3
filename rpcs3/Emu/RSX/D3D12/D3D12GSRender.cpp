﻿#ifdef _MSC_VER
#include "stdafx.h"
#include "stdafx_d3d12.h"
#include "D3D12GSRender.h"
#include "Emu/System.h"
#include <wrl/client.h>
#include <dxgi1_4.h>
#include <thread>
#include <chrono>
#include <locale>
#include <codecvt>
#include <cmath>
#include "d3dx12.h"
#include <d3d11on12.h>
#include "D3D12Formats.h"
#include "../rsx_methods.h"

#define ENSURE_RESULT(Expr, Msg) { const auto HR = (Expr); if (FAILED(HR)) fmt::throw_exception("HRESULT = %s" HERE, get_hresult_message(HR)); }

namespace
{
	HMODULE DX12, DX11, DXCC;

	PFN_D3D12_CREATE_DEVICE DX12CreateDevice;
	PFN_D3D12_GET_DEBUG_INTERFACE DX12GetDebugInterface;
	PFN_D3D12_SERIALIZE_ROOT_SIGNATURE DX12SerializeRootSignature;
	PFN_D3D11ON12_CREATE_DEVICE DX11On12CreateDevice;
	pD3DCompile DXCompile;

	void LoadModules()
	{
		DX12 = LoadLibrary(TEXT("d3d12.dll"));
		DX11 = LoadLibrary(TEXT("d3d11.dll"));
		DXCC = LoadLibrary(TEXT("d3dcompiler_47.dll"));

		LOG_TRACE(RSX, "Loaded DirectX dlls");

		DX12CreateDevice = (PFN_D3D12_CREATE_DEVICE)GetProcAddress(DX12, "D3D12CreateDevice");
		DX12GetDebugInterface = (PFN_D3D12_GET_DEBUG_INTERFACE)GetProcAddress(DX12, "D3D12GetDebugInterface");
		DX12SerializeRootSignature = (PFN_D3D12_SERIALIZE_ROOT_SIGNATURE)GetProcAddress(DX12, "D3D12SerializeRootSignature");
		DX11On12CreateDevice = (PFN_D3D11ON12_CREATE_DEVICE)GetProcAddress(DX11, "D3D11On12CreateDevice");
		DXCompile = (pD3DCompile)GetProcAddress(DXCC, "D3DCompile");

		LOG_TRACE(RSX, "Loaded DirectX functions from dlls");
	}

	void UnloadModules()
	{
		FreeLibrary(DX12);
		FreeLibrary(DX11);
		FreeLibrary(DXCC);

		LOG_TRACE(RSX, "Free'd DirectX dlls");
	}
}

std::vector<DX12Render::Adapter*> DX12Render::Adapters()
{
	Adapter* Adapt;
	std::vector<Adapter*> AdaptList;
	IDXGIFactory* Factory = nullptr;

	if (FAILED(CreateDXGIFactory(IID_PPV_ARGS(&Factory))))
		return AdaptList;

	for (UINT I = 0; Factory->EnumAdapters(I, &Adapt) != DXGI_ERROR_NOT_FOUND; I++)
		AdaptList.push_back(Adapt);

	if (Factory)
		Factory->Release();

	return AdaptList;
}

DX12Render::DX12Render()
    : GSRender()
{
	LoadModules();
}

DX12Render::~DX12Render()
{
	UnloadModules();
}

void DX12Render::begin()
{
	rsx::thread::begin();
}

void DX12Render::end()
{
	//push geometry
	u32 index = 0;
	rsx::method_registers.current_draw_clause.begin();
	do
	{
		emit_geometry(index++);
	} while (rsx::method_registers.current_draw_clause.next());

	rsx::thread::end();
}

void DX12Render::emit_geometry(u32 index)
{
}

void DX12Render::on_init_thread()
{
	//aquire frame context
	m_context = get_frame()->make_context();
	m_frame->set_current(m_context);
}

void DX12Render::on_exit()
{
}

bool DX12Render::do_method(u32 id, u32 arg)
{
	return false;
}

void DX12Render::flip(int buffer)
{
	if (skip_frame)
	{
		m_frame->flip(m_context, /* skip_frame = */ true);
		rsx::thread::flip(buffer);
		return;
	}

	m_frame->flip(m_context);
	rsx::thread::flip(buffer);
}

#if 0
// blame visual studio for this stuff being header only.
// it refused to compile it if i put it in a seperate .cpp file

//	directx 12 runtime module, directx 11 runtime module
//	and the directx shader compiler module
HMODULE DX12, DX11, DCC;

PFN_D3D12_CREATE_DEVICE DX12CreateDevice;
PFN_D3D12_GET_DEBUG_INTERFACE DX12GetDebugInterface;
PFN_D3D12_SERIALIZE_ROOT_SIGNATURE DX12SerializeRootSignature;
PFN_D3D11ON12_CREATE_DEVICE DX11On12CreateDevice;
pD3DCompile DXCompile;

DX12Render::DX12Render()
{
	LoadModules();
	InitDevice();
	InitQueue();
	InitSwapChain();
	InitAllocatorsAndLists();
	InitViewport();
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
	DX12                       = LoadLibrary(TEXT("d3d12.dll"));
	DX12CreateDevice           = (PFN_D3D12_CREATE_DEVICE)GetProcAddress(DX12, "D3D12CreateDevice");
	DX12GetDebugInterface      = (PFN_D3D12_GET_DEBUG_INTERFACE)GetProcAddress(DX12, "D3D12GetDebugInterface");
	DX12SerializeRootSignature = (PFN_D3D12_SERIALIZE_ROOT_SIGNATURE)GetProcAddress(DX12, "D3D12SerializeRootSignature");

	DX11                 = LoadLibrary(TEXT("d3d11.dll"));
	DX11On12CreateDevice = (PFN_D3D11ON12_CREATE_DEVICE)GetProcAddress(DX11, "D3D11On12CreateDevice");

	DCC       = LoadLibrary(TEXT("d3dcompiler_47.dll"));
	DXCompile = (pD3DCompile)GetProcAddress(DCC, "D3DCompile");
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
	auto Result = DX12CreateDevice(nullptr,
	    //	Q: so why are you casting a number instead of using the enum?
	    //	A: because its not defined even thoughts its avaiable
	    //	Q: what?
	    //	A: because microsoft hates you :)
	    (D3D_FEATURE_LEVEL)0xc100, IID_PPV_ARGS(&m_device));

	//	works if the card supports DX12 features, even though the enum is missing
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

void DX12Render::SetupSwapChain()
{
	m_currentFenceValue = 1;

	for (u32 I = 0; I < queueSlotLen; I++)
	{
		m_frameFenceEvents[I] = CreateEvent(nullptr, false, false, nullptr);
		m_fenceValues[I]      = 0;
		m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fences[I]));
	}

	for (u32 I = 0; I < queueSlotLen; I++)
	{
		m_swapChain->GetBuffer(I, IID_PPV_ARGS(&m_renderTargets[I]));
	}
}

void DX12Render::InitRenderTargets()
{
	m_targetViewDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
}

void DX12Render::SetupRenderTargets()
{
	D3D12_DESCRIPTOR_HEAP_DESC Desc = {};
	Desc.NumDescriptors             = queueSlotLen;
	Desc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	Desc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	m_device->CreateDescriptorHeap(&Desc, IID_PPV_ARGS(&m_renderTargetDescriptorHeap));

	CD3DX12_CPU_DESCRIPTOR_HANDLE RTVHandle
	{
		m_renderTargetDescriptorHeap->GetCPUDescriptorHandleForHeapStart()
	};

	for (u32 I = 0; I < queueSlotLen; I++)
	{
		D3D12_RENDER_TARGET_VIEW_DESC View;
		View.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		View.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		View.Texture2D.MipSlice = 0;
		View.Texture2D.PlaneSlice = 0;

		m_device->CreateRenderTargetView(m_renderTargets[I].Get(), &View, RTVHandle);

		RTVHandle.Offset(m_renderTargetViewDescriptorSize);
	}
}

void DX12Render::InitAllocatorsAndLists()
{
	for (u32 I = 0; I < queueSlotLen; I++)
	{
		m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_allocators[I]));
		m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_allocators[I].Get(), nullptr, IID_PPV_ARGS(&m_commandLists[I]));
		m_commandLists[I]->Close();
	}
}

void DX12Render::InitViewport()
{
	m_rect = {0, 0, get_frame()->client_width(), get_frame()->client_height()};

	m_viewport = {0.f, 0.f, static_cast<float>(get_frame()->client_width()), static_cast<float>(get_frame()->client_height()), 0.f, 1.f};
}

void DX12Render::ShutDown()
{
}

void DX12Render::PrepareRender()
{
}

void DX12Render::Render()
{
}

void DX12Render::Present()
{
	m_swapChain->Present(1, 0);
}

void DX12Render::FinalizeRender()
{
}

void DX12Render::WaitForFence()
{
}

#endif

PFN_D3D12_CREATE_DEVICE wrapD3D12CreateDevice;
PFN_D3D12_GET_DEBUG_INTERFACE wrapD3D12GetDebugInterface;
PFN_D3D12_SERIALIZE_ROOT_SIGNATURE wrapD3D12SerializeRootSignature;
PFN_D3D11ON12_CREATE_DEVICE wrapD3D11On12CreateDevice;
pD3DCompile wrapD3DCompile;

ID3D12Device* g_d3d12_device = nullptr;

#define VERTEX_BUFFERS_SLOT 0
#define FRAGMENT_CONSTANT_BUFFERS_SLOT 1
#define VERTEX_CONSTANT_BUFFERS_SLOT 2
#define TEXTURES_SLOT 3
#define SAMPLERS_SLOT 4
#define SCALE_OFFSET_SLOT 5

namespace
{
	HMODULE D3D12Module;
	HMODULE D3D11Module;
	HMODULE D3DCompiler;

	void loadD3D12FunctionPointers()
	{
		D3D12Module                     = verify("d3d12.dll", LoadLibrary(L"d3d12.dll"));
		wrapD3D12CreateDevice           = (PFN_D3D12_CREATE_DEVICE)GetProcAddress(D3D12Module, "D3D12CreateDevice");
		wrapD3D12GetDebugInterface      = (PFN_D3D12_GET_DEBUG_INTERFACE)GetProcAddress(D3D12Module, "D3D12GetDebugInterface");
		wrapD3D12SerializeRootSignature = (PFN_D3D12_SERIALIZE_ROOT_SIGNATURE)GetProcAddress(D3D12Module, "D3D12SerializeRootSignature");
		D3D11Module                     = verify("d3d11.dll", LoadLibrary(L"d3d11.dll"));
		wrapD3D11On12CreateDevice       = (PFN_D3D11ON12_CREATE_DEVICE)GetProcAddress(D3D11Module, "D3D11On12CreateDevice");
		D3DCompiler                     = verify("d3dcompiler_47.dll", LoadLibrary(L"d3dcompiler_47.dll"));
		wrapD3DCompile                  = (pD3DCompile)GetProcAddress(D3DCompiler, "D3DCompile");
	}

	void unloadD3D12FunctionPointers()
	{
		FreeLibrary(D3D12Module);
		FreeLibrary(D3D11Module);
		FreeLibrary(D3DCompiler);
	}

	/**
	 * Wait until command queue has completed all task.
	 */
	void wait_for_command_queue(ID3D12Device* device, ID3D12CommandQueue* command_queue)
	{
		ComPtr<ID3D12Fence> fence;
		CHECK_HRESULT(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.GetAddressOf())));
		HANDLE handle = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
		fence->SetEventOnCompletion(1, handle);
		command_queue->Signal(fence.Get(), 1);
		WaitForSingleObjectEx(handle, INFINITE, FALSE);
		CloseHandle(handle);
	}
} // namespace

void D3D12GSRender::shader::release()
{
	pso->Release();
	root_signature->Release();
	vertex_buffer->Release();
	texture_descriptor_heap->Release();
	sampler_descriptor_heap->Release();
}

bool D3D12GSRender::invalidate_address(u32 addr)
{
	bool result = false;
	result |= m_texture_cache.invalidate_address(addr);
	return result;
}

D3D12DLLManagement::D3D12DLLManagement()
{
	loadD3D12FunctionPointers();
}

D3D12DLLManagement::~D3D12DLLManagement()
{
	unloadD3D12FunctionPointers();
}

namespace
{
	ComPtr<ID3DBlob> get_shared_root_signature_blob()
	{
		CD3DX12_ROOT_PARAMETER RP[6];

		// vertex buffer are bound each draw calls
		CD3DX12_DESCRIPTOR_RANGE vertex_buffer_descriptors(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 16, 0);
		RP[VERTEX_BUFFERS_SLOT].InitAsDescriptorTable(1, &vertex_buffer_descriptors, D3D12_SHADER_VISIBILITY_VERTEX);

		// fragment constants are bound each draw calls
		RP[FRAGMENT_CONSTANT_BUFFERS_SLOT].InitAsConstantBufferView(2, 0, D3D12_SHADER_VISIBILITY_PIXEL);

		// vertex constants are bound often
		CD3DX12_DESCRIPTOR_RANGE vertex_constant_buffer_descriptors(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);
		RP[VERTEX_CONSTANT_BUFFERS_SLOT].InitAsDescriptorTable(1, &vertex_constant_buffer_descriptors, D3D12_SHADER_VISIBILITY_VERTEX);

		// textures are bound often
		CD3DX12_DESCRIPTOR_RANGE texture_descriptors(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 16, 0);
		RP[TEXTURES_SLOT].InitAsDescriptorTable(1, &texture_descriptors, D3D12_SHADER_VISIBILITY_PIXEL);
		// samplers are bound often
		CD3DX12_DESCRIPTOR_RANGE sampler_descriptors(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 16, 0);
		RP[SAMPLERS_SLOT].InitAsDescriptorTable(1, &sampler_descriptors, D3D12_SHADER_VISIBILITY_PIXEL);

		// scale offset matrix are bound once in a while
		CD3DX12_DESCRIPTOR_RANGE scale_offset_descriptors(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
		RP[SCALE_OFFSET_SLOT].InitAsDescriptorTable(1, &scale_offset_descriptors, D3D12_SHADER_VISIBILITY_ALL);

		Microsoft::WRL::ComPtr<ID3DBlob> root_signature_blob;
		Microsoft::WRL::ComPtr<ID3DBlob> error_blob;
		CHECK_HRESULT(wrapD3D12SerializeRootSignature(&CD3DX12_ROOT_SIGNATURE_DESC(6, RP, 0, 0), D3D_ROOT_SIGNATURE_VERSION_1, root_signature_blob.GetAddressOf(), error_blob.GetAddressOf()));

		return root_signature_blob;
	}
} // namespace

u64 D3D12GSRender::get_cycles()
{
	return thread_ctrl::get_cycles(static_cast<named_thread<D3D12GSRender>&>(*this));
}

D3D12GSRender::D3D12GSRender()
    : GSRender()
    , m_d3d12_lib()
    , m_current_pso({})
{
	if (g_cfg.video.debug_output)
	{
		Microsoft::WRL::ComPtr<ID3D12Debug> debugInterface;
		wrapD3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface));
		debugInterface->EnableDebugLayer();
	}

	Microsoft::WRL::ComPtr<IDXGIFactory4> dxgi_factory;
	CHECK_HRESULT(CreateDXGIFactory(IID_PPV_ARGS(&dxgi_factory)));

	// Create adapter
	ComPtr<IDXGIAdapter> adapter;
	const std::wstring adapter_name = std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(g_cfg.video.d3d12.adapter);

	for (UINT id = 0; dxgi_factory->EnumAdapters(id, adapter.ReleaseAndGetAddressOf()) != DXGI_ERROR_NOT_FOUND; id++)
	{
		DXGI_ADAPTER_DESC desc;
		adapter->GetDesc(&desc);

		// Adapter with specified name
		if (adapter_name == desc.Description)
		{
			break;
		}

		// Default adapter
		if (id == 1 && adapter_name.empty())
		{
			break;
		}
	}

	if (FAILED(wrapD3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device))))
	{
		LOG_ERROR(RSX, "Failed to initialize D3D device on adapter '%s', falling back to first available GPU", g_cfg.video.d3d12.adapter.get());

		//	Try to create a device on the first available device
		if (FAILED(wrapD3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device))))
		{
			LOG_FATAL(RSX, "Unable to create D3D12 device. Your GPU(s) may not have D3D12 support.");
			return;
		}
	}

	g_d3d12_device = m_device.Get();

	// Queues
	D3D12_COMMAND_QUEUE_DESC graphic_queue_desc = {D3D12_COMMAND_LIST_TYPE_DIRECT};
	CHECK_HRESULT(m_device->CreateCommandQueue(&graphic_queue_desc, IID_PPV_ARGS(m_command_queue.GetAddressOf())));

	m_descriptor_stride_srv_cbv_uav = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	m_descriptor_stride_dsv         = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	m_descriptor_stride_rtv         = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	m_descriptor_stride_samplers    = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

	// Create swap chain and put them in a descriptor heap as rendertarget
	DXGI_SWAP_CHAIN_DESC swap_chain = {};
	swap_chain.BufferCount          = 2;
	swap_chain.Windowed             = true;
	swap_chain.OutputWindow         = (HWND)m_frame->handle();
	swap_chain.BufferDesc.Format    = DXGI_FORMAT_R8G8B8A8_UNORM;
	swap_chain.BufferUsage          = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swap_chain.SampleDesc.Count     = 1;
	swap_chain.Flags                = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	swap_chain.SwapEffect           = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;

	CHECK_HRESULT(dxgi_factory->CreateSwapChain(m_command_queue.Get(), &swap_chain, (IDXGISwapChain**)m_swap_chain.GetAddressOf()));
	m_swap_chain->GetBuffer(0, IID_PPV_ARGS(&m_backbuffer[0]));
	m_swap_chain->GetBuffer(1, IID_PPV_ARGS(&m_backbuffer[1]));

	D3D12_DESCRIPTOR_HEAP_DESC render_target_descriptor_heap_desc = {D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1};
	D3D12_RENDER_TARGET_VIEW_DESC renter_target_view_desc         = {};
	renter_target_view_desc.ViewDimension                         = D3D12_RTV_DIMENSION_TEXTURE2D;
	renter_target_view_desc.Format                                = DXGI_FORMAT_R8G8B8A8_UNORM;
	m_device->CreateDescriptorHeap(&render_target_descriptor_heap_desc, IID_PPV_ARGS(&m_backbuffer_descriptor_heap[0]));
	m_device->CreateRenderTargetView(m_backbuffer[0].Get(), &renter_target_view_desc, m_backbuffer_descriptor_heap[0]->GetCPUDescriptorHandleForHeapStart());
	m_device->CreateDescriptorHeap(&render_target_descriptor_heap_desc, IID_PPV_ARGS(&m_backbuffer_descriptor_heap[1]));
	m_device->CreateRenderTargetView(m_backbuffer[1].Get(), &renter_target_view_desc, m_backbuffer_descriptor_heap[1]->GetCPUDescriptorHandleForHeapStart());

	D3D12_DESCRIPTOR_HEAP_DESC current_texture_descriptors_desc = {D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 16};
	CHECK_HRESULT(m_device->CreateDescriptorHeap(&current_texture_descriptors_desc, IID_PPV_ARGS(m_current_texture_descriptors.GetAddressOf())));
	D3D12_DESCRIPTOR_HEAP_DESC current_sampler_descriptors_desc = {D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 16};
	CHECK_HRESULT(m_device->CreateDescriptorHeap(&current_sampler_descriptors_desc, IID_PPV_ARGS(m_current_sampler_descriptors.GetAddressOf())));

	ComPtr<ID3DBlob> root_signature_blob = get_shared_root_signature_blob();

	m_device->CreateRootSignature(0, root_signature_blob->GetBufferPointer(), root_signature_blob->GetBufferSize(), IID_PPV_ARGS(m_shared_root_signature.GetAddressOf()));

	m_per_frame_storage[0].init(m_device.Get());
	m_per_frame_storage[0].reset();
	m_per_frame_storage[1].init(m_device.Get());
	m_per_frame_storage[1].reset();

	m_output_scaling_pass.init(m_device.Get(), m_command_queue.Get());

	CHECK_HRESULT(m_device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, 2, 2, 1, 1),
	    D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_dummy_texture)));

	m_rtts.init(m_device.Get());
	m_readback_resources.init(m_device.Get(), 1024 * 1024 * 128, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);
	m_buffer_data.init(m_device.Get(), 1024 * 1024 * 896, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);

	CHECK_HRESULT(m_device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(1024 * 1024 * 16),
	    D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, nullptr, IID_PPV_ARGS(m_vertex_buffer_data.GetAddressOf())));

	if (g_cfg.video.overlay)
		init_d2d_structures();
}

D3D12GSRender::~D3D12GSRender()
{
	if (!m_device)
	{
		//	Initialization must have failed
		return;
	}

	wait_for_command_queue(m_device.Get(), m_command_queue.Get());

	m_texture_cache.unprotect_all();

	m_dummy_texture->Release();
	m_per_frame_storage[0].release();
	m_per_frame_storage[1].release();
	m_output_scaling_pass.release();

	release_d2d_structures();
}

void D3D12GSRender::on_init_thread()
{
	if (!m_device)
	{
		//	Init must have failed
		fmt::throw_exception("No D3D12 device was created");
	}
}

void D3D12GSRender::on_exit()
{
	GSRender::on_exit();
}

void D3D12GSRender::do_local_task(rsx::FIFO_state state)
{
	if (state != rsx::FIFO_state::lock_wait)
	{
		//	TODO
		m_frame->clear_wm_events();
	}

	rsx::thread::do_local_task(state);
}

bool D3D12GSRender::do_method(u32 cmd, u32 arg)
{
	switch (cmd)
	{
	case NV4097_CLEAR_SURFACE: clear_surface(arg); return true;
	case NV4097_TEXTURE_READ_SEMAPHORE_RELEASE: copy_render_target_to_dma_location(); return false;   //	call rsx::thread method implementation
	case NV4097_BACK_END_WRITE_SEMAPHORE_RELEASE: copy_render_target_to_dma_location(); return false; //	call rsx::thread method implementation

	default: return false;
	}
}

void D3D12GSRender::end()
{
	std::chrono::time_point<steady_clock> start_duration = steady_clock::now();

	std::chrono::time_point<steady_clock> program_load_start = steady_clock::now();
	load_program();
	std::chrono::time_point<steady_clock> program_load_end = steady_clock::now();
	m_timers.program_load_duration += std::chrono::duration_cast<std::chrono::microseconds>(program_load_end - program_load_start).count();

	if (!current_fragment_program.valid)
	{
		rsx::thread::end();
		return;
	}

	std::chrono::time_point<steady_clock> rtt_duration_start = steady_clock::now();
	prepare_render_targets(get_current_resource_storage().command_list.Get());

	std::chrono::time_point<steady_clock> rtt_duration_end = steady_clock::now();
	m_timers.prepare_rtt_duration += std::chrono::duration_cast<std::chrono::microseconds>(rtt_duration_end - rtt_duration_start).count();

	std::chrono::time_point<steady_clock> vertex_index_duration_start = steady_clock::now();

	size_t currentDescriptorIndex = get_current_resource_storage().descriptors_heap_index;

	size_t vertex_count;
	bool indexed_draw;
	std::vector<D3D12_SHADER_RESOURCE_VIEW_DESC> vertex_buffer_views;
	std::tie(indexed_draw, vertex_count, vertex_buffer_views) = upload_and_set_vertex_index_data(get_current_resource_storage().command_list.Get());

	UINT vertex_buffer_count = static_cast<UINT>(vertex_buffer_views.size());

	std::chrono::time_point<steady_clock> vertex_index_duration_end = steady_clock::now();
	m_timers.vertex_index_duration += std::chrono::duration_cast<std::chrono::microseconds>(vertex_index_duration_end - vertex_index_duration_start).count();

	get_current_resource_storage().command_list->SetGraphicsRootSignature(m_shared_root_signature.Get());
	get_current_resource_storage().command_list->OMSetStencilRef(rsx::method_registers.stencil_func_ref());

	std::chrono::time_point<steady_clock> constants_duration_start = steady_clock::now();

	INT offset = 0;
	for (const auto view : vertex_buffer_views)
	{
		m_device->CreateShaderResourceView(m_vertex_buffer_data.Get(), &view,
		    CD3DX12_CPU_DESCRIPTOR_HANDLE(get_current_resource_storage().descriptors_heap->GetCPUDescriptorHandleForHeapStart())
		        .Offset((INT)currentDescriptorIndex + offset++, m_descriptor_stride_srv_cbv_uav));
	}
	// Bind vertex buffer
	get_current_resource_storage().command_list->SetGraphicsRootDescriptorTable(VERTEX_BUFFERS_SLOT,
	    CD3DX12_GPU_DESCRIPTOR_HANDLE(get_current_resource_storage().descriptors_heap->GetGPUDescriptorHandleForHeapStart()).Offset((INT)currentDescriptorIndex, m_descriptor_stride_srv_cbv_uav));

	// Constants
	const D3D12_CONSTANT_BUFFER_VIEW_DESC& fragment_constant_view = upload_fragment_shader_constants();
	get_current_resource_storage().command_list->SetGraphicsRootConstantBufferView(FRAGMENT_CONSTANT_BUFFERS_SLOT, fragment_constant_view.BufferLocation);

	upload_and_bind_scale_offset_matrix(currentDescriptorIndex + vertex_buffer_count);
	get_current_resource_storage().command_list->SetGraphicsRootDescriptorTable(
	    SCALE_OFFSET_SLOT, CD3DX12_GPU_DESCRIPTOR_HANDLE(get_current_resource_storage().descriptors_heap->GetGPUDescriptorHandleForHeapStart())
	                           .Offset((INT)currentDescriptorIndex + vertex_buffer_count, m_descriptor_stride_srv_cbv_uav));

	if (!g_cfg.video.debug_output && (m_graphics_state & rsx::pipeline_state::transform_constants_dirty))
	{
		m_current_transform_constants_buffer_descriptor_id = (u32)currentDescriptorIndex + 1 + vertex_buffer_count;
		upload_and_bind_vertex_shader_constants(currentDescriptorIndex + 1 + vertex_buffer_count);
		get_current_resource_storage().command_list->SetGraphicsRootDescriptorTable(
		    VERTEX_CONSTANT_BUFFERS_SLOT, CD3DX12_GPU_DESCRIPTOR_HANDLE(get_current_resource_storage().descriptors_heap->GetGPUDescriptorHandleForHeapStart())
		                                      .Offset(m_current_transform_constants_buffer_descriptor_id, m_descriptor_stride_srv_cbv_uav));
	}

	m_graphics_state &= ~rsx::pipeline_state::memory_barrier_bits;

	std::chrono::time_point<steady_clock> constants_duration_end = steady_clock::now();
	m_timers.constants_duration += std::chrono::duration_cast<std::chrono::microseconds>(constants_duration_end - constants_duration_start).count();

	get_current_resource_storage().command_list->SetPipelineState(std::get<0>(m_current_pso).Get());

	std::chrono::time_point<steady_clock> texture_duration_start = steady_clock::now();

	get_current_resource_storage().descriptors_heap_index += 2 + vertex_buffer_count;
	size_t texture_count = std::get<2>(m_current_pso);
	if (texture_count > 0)
	{
		if (get_current_resource_storage().current_sampler_index + 16 > 2048)
		{
			get_current_resource_storage().sampler_descriptors_heap_index = 1;
			get_current_resource_storage().current_sampler_index          = 0;

			ID3D12DescriptorHeap* descriptors[] = {
			    get_current_resource_storage().descriptors_heap.Get(),
			    get_current_resource_storage().sampler_descriptor_heap[get_current_resource_storage().sampler_descriptors_heap_index].Get(),
			};
			get_current_resource_storage().command_list->SetDescriptorHeaps(2, descriptors);
		}

		upload_textures(get_current_resource_storage().command_list.Get(), texture_count);

		m_device->CopyDescriptorsSimple(16,
		    CD3DX12_CPU_DESCRIPTOR_HANDLE(get_current_resource_storage().descriptors_heap->GetCPUDescriptorHandleForHeapStart())
		        .Offset((UINT)get_current_resource_storage().descriptors_heap_index, m_descriptor_stride_srv_cbv_uav),
		    CD3DX12_CPU_DESCRIPTOR_HANDLE(m_current_texture_descriptors->GetCPUDescriptorHandleForHeapStart()), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		m_device->CopyDescriptorsSimple(16,
		    CD3DX12_CPU_DESCRIPTOR_HANDLE(get_current_resource_storage().sampler_descriptor_heap[get_current_resource_storage().sampler_descriptors_heap_index]->GetCPUDescriptorHandleForHeapStart())
		        .Offset((UINT)get_current_resource_storage().current_sampler_index, m_descriptor_stride_samplers),
		    CD3DX12_CPU_DESCRIPTOR_HANDLE(m_current_sampler_descriptors->GetCPUDescriptorHandleForHeapStart()), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

		get_current_resource_storage().command_list->SetGraphicsRootDescriptorTable(
		    TEXTURES_SLOT, CD3DX12_GPU_DESCRIPTOR_HANDLE(get_current_resource_storage().descriptors_heap->GetGPUDescriptorHandleForHeapStart())
		                       .Offset((INT)get_current_resource_storage().descriptors_heap_index, m_descriptor_stride_srv_cbv_uav));
		get_current_resource_storage().command_list->SetGraphicsRootDescriptorTable(SAMPLERS_SLOT,
		    CD3DX12_GPU_DESCRIPTOR_HANDLE(get_current_resource_storage().sampler_descriptor_heap[get_current_resource_storage().sampler_descriptors_heap_index]->GetGPUDescriptorHandleForHeapStart())
		        .Offset((INT)get_current_resource_storage().current_sampler_index, m_descriptor_stride_samplers));

		get_current_resource_storage().current_sampler_index += texture_count;
		get_current_resource_storage().descriptors_heap_index += texture_count;
	}

	std::chrono::time_point<steady_clock> texture_duration_end = steady_clock::now();
	m_timers.texture_duration += std::chrono::duration_cast<std::chrono::microseconds>(texture_duration_end - texture_duration_start).count();
	set_rtt_and_ds(get_current_resource_storage().command_list.Get());

	int clip_w = rsx::method_registers.surface_clip_width();
	int clip_h = rsx::method_registers.surface_clip_height();

	D3D12_VIEWPORT viewport = {
	    0.f,
	    0.f,
	    (float)clip_w,
	    (float)clip_h,
	    0.f,
	    1.f,
	};
	get_current_resource_storage().command_list->RSSetViewports(1, &viewport);

	get_current_resource_storage().command_list->RSSetScissorRects(
	    1, &get_scissor(rsx::method_registers.scissor_origin_x(), rsx::method_registers.scissor_origin_y(), rsx::method_registers.scissor_width(), rsx::method_registers.scissor_height()));

	get_current_resource_storage().command_list->IASetPrimitiveTopology(get_primitive_topology(rsx::method_registers.current_draw_clause.primitive));

	if (indexed_draw)
		get_current_resource_storage().command_list->DrawIndexedInstanced((UINT)vertex_count, 1, 0, 0, 0);
	else
		get_current_resource_storage().command_list->DrawInstanced((UINT)vertex_count, 1, 0, 0);

	std::chrono::time_point<steady_clock> end_duration = steady_clock::now();
	m_timers.draw_calls_duration += std::chrono::duration_cast<std::chrono::microseconds>(end_duration - start_duration).count();
	m_timers.draw_calls_count++;

	if (g_cfg.video.debug_output)
	{
		CHECK_HRESULT(get_current_resource_storage().command_list->Close());
		m_command_queue->ExecuteCommandLists(1, (ID3D12CommandList**)get_current_resource_storage().command_list.GetAddressOf());
		get_current_resource_storage().set_new_command_list();
	}
	thread::end();
}

namespace
{
	bool is_flip_surface_in_global_memory(rsx::surface_target color_target)
	{
		switch (color_target)
		{
		case rsx::surface_target::surface_a:
		case rsx::surface_target::surface_b:
		case rsx::surface_target::surfaces_a_b:
		case rsx::surface_target::surfaces_a_b_c:
		case rsx::surface_target::surfaces_a_b_c_d: return true;
		case rsx::surface_target::none: return false;
		}
		fmt::throw_exception("Wrong color_target" HERE);
	}
} // namespace

void D3D12GSRender::flip(int buffer)
{
	ID3D12Resource* resource_to_flip;
	float viewport_w, viewport_h;

	if (!is_flip_surface_in_global_memory(rsx::method_registers.surface_color_target()))
	{
		resource_storage& storage = get_current_resource_storage();
		verify(HERE), storage.ram_framebuffer == nullptr;

		size_t w = 0, h = 0, row_pitch = 0;

		size_t offset = 0;
		if (false)
		{
			u32 addr       = rsx::get_address(display_buffers[current_display_buffer].offset, CELL_GCM_LOCATION_LOCAL);
			w              = display_buffers[current_display_buffer].width;
			h              = display_buffers[current_display_buffer].height;
			u8* src_buffer = vm::_ptr<u8>(addr);

			row_pitch           = align(w * 4, 256);
			size_t texture_size = row_pitch * h; // * 4 for mipmap levels
			size_t heap_offset  = m_buffer_data.alloc<D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT>(texture_size);

			void* mapped_buffer = m_buffer_data.map<void>(heap_offset);
			for (unsigned row = 0; row < h; row++)
				memcpy((char*)mapped_buffer + row * row_pitch, (char*)src_buffer + row * w * 4, w * 4);
			m_buffer_data.unmap(CD3DX12_RANGE(heap_offset, heap_offset + texture_size));
			offset = heap_offset;

			CHECK_HRESULT(m_device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
			    &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, (UINT)w, (UINT)h, 1, 1), D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(storage.ram_framebuffer.GetAddressOf())));
			get_current_resource_storage().command_list->CopyTextureRegion(&CD3DX12_TEXTURE_COPY_LOCATION(storage.ram_framebuffer.Get(), 0), 0, 0, 0,
			    &CD3DX12_TEXTURE_COPY_LOCATION(m_buffer_data.get_heap(), {offset, {DXGI_FORMAT_R8G8B8A8_UNORM, (UINT)w, (UINT)h, 1, (UINT)row_pitch}}), nullptr);

			get_current_resource_storage().command_list->ResourceBarrier(
			    1, &CD3DX12_RESOURCE_BARRIER::Transition(storage.ram_framebuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));
			resource_to_flip = storage.ram_framebuffer.Get();
		}
		else
			resource_to_flip = nullptr;

		viewport_w = (float)w, viewport_h = (float)h;
	}
	else
	{
		if (std::get<1>(m_rtts.m_bound_render_targets[0]) != nullptr)
		{
			get_current_resource_storage().command_list->ResourceBarrier(
			    1, &CD3DX12_RESOURCE_BARRIER::Transition(std::get<1>(m_rtts.m_bound_render_targets[0]), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));
			resource_to_flip = std::get<1>(m_rtts.m_bound_render_targets[0]);
		}
		else if (std::get<1>(m_rtts.m_bound_render_targets[1]) != nullptr)
		{
			get_current_resource_storage().command_list->ResourceBarrier(
			    1, &CD3DX12_RESOURCE_BARRIER::Transition(std::get<1>(m_rtts.m_bound_render_targets[1]), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));
			resource_to_flip = std::get<1>(m_rtts.m_bound_render_targets[1]);
		}
		else
			resource_to_flip = nullptr;
	}

	get_current_resource_storage().command_list->ResourceBarrier(
	    1, &CD3DX12_RESOURCE_BARRIER::Transition(m_backbuffer[m_swap_chain->GetCurrentBackBufferIndex()].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	D3D12_VIEWPORT viewport = {
	    0.f, 0.f, (float)m_backbuffer[m_swap_chain->GetCurrentBackBufferIndex()]->GetDesc().Width, (float)m_backbuffer[m_swap_chain->GetCurrentBackBufferIndex()]->GetDesc().Height, 0.f, 1.f};
	get_current_resource_storage().command_list->RSSetViewports(1, &viewport);

	D3D12_RECT box = {
	    0,
	    0,
	    (LONG)m_backbuffer[m_swap_chain->GetCurrentBackBufferIndex()]->GetDesc().Width,
	    (LONG)m_backbuffer[m_swap_chain->GetCurrentBackBufferIndex()]->GetDesc().Height,
	};
	get_current_resource_storage().command_list->RSSetScissorRects(1, &box);
	get_current_resource_storage().command_list->SetGraphicsRootSignature(m_output_scaling_pass.root_signature);
	get_current_resource_storage().command_list->SetPipelineState(m_output_scaling_pass.pso);

	D3D12_SHADER_RESOURCE_VIEW_DESC shader_resource_view_desc = {};
	// FIXME: Not always true
	shader_resource_view_desc.Format              = DXGI_FORMAT_R8G8B8A8_UNORM;
	shader_resource_view_desc.ViewDimension       = D3D12_SRV_DIMENSION_TEXTURE2D;
	shader_resource_view_desc.Texture2D.MipLevels = 1;
	if (is_flip_surface_in_global_memory(rsx::method_registers.surface_color_target()))
		shader_resource_view_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	else
		shader_resource_view_desc.Shader4ComponentMapping = D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_1,
		    D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_2, D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_3, D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0);
	m_device->CreateShaderResourceView(resource_to_flip, &shader_resource_view_desc,
	    CD3DX12_CPU_DESCRIPTOR_HANDLE(m_output_scaling_pass.texture_descriptor_heap->GetCPUDescriptorHandleForHeapStart())
	        .Offset(m_swap_chain->GetCurrentBackBufferIndex(), m_descriptor_stride_srv_cbv_uav));

	D3D12_SAMPLER_DESC sampler_desc = {};
	sampler_desc.Filter             = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
	sampler_desc.AddressU           = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler_desc.AddressV           = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler_desc.AddressW           = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	m_device->CreateSampler(&sampler_desc, CD3DX12_CPU_DESCRIPTOR_HANDLE(m_output_scaling_pass.sampler_descriptor_heap->GetCPUDescriptorHandleForHeapStart())
	                                           .Offset(m_swap_chain->GetCurrentBackBufferIndex(), m_descriptor_stride_samplers));

	ID3D12DescriptorHeap* descriptors_heaps[] = {m_output_scaling_pass.texture_descriptor_heap, m_output_scaling_pass.sampler_descriptor_heap};
	get_current_resource_storage().command_list->SetDescriptorHeaps(2, descriptors_heaps);
	get_current_resource_storage().command_list->SetGraphicsRootDescriptorTable(0, CD3DX12_GPU_DESCRIPTOR_HANDLE(m_output_scaling_pass.texture_descriptor_heap->GetGPUDescriptorHandleForHeapStart())
	                                                                                   .Offset(m_swap_chain->GetCurrentBackBufferIndex(), m_descriptor_stride_srv_cbv_uav));
	get_current_resource_storage().command_list->SetGraphicsRootDescriptorTable(1, CD3DX12_GPU_DESCRIPTOR_HANDLE(m_output_scaling_pass.sampler_descriptor_heap->GetGPUDescriptorHandleForHeapStart())
	                                                                                   .Offset(m_swap_chain->GetCurrentBackBufferIndex(), m_descriptor_stride_samplers));

	get_current_resource_storage().command_list->OMSetRenderTargets(
	    1, &CD3DX12_CPU_DESCRIPTOR_HANDLE(m_backbuffer_descriptor_heap[m_swap_chain->GetCurrentBackBufferIndex()]->GetCPUDescriptorHandleForHeapStart()), true, nullptr);
	D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view = {};
	vertex_buffer_view.BufferLocation           = m_output_scaling_pass.vertex_buffer->GetGPUVirtualAddress();
	vertex_buffer_view.StrideInBytes            = 4 * sizeof(float);
	vertex_buffer_view.SizeInBytes              = 16 * sizeof(float);
	get_current_resource_storage().command_list->IASetVertexBuffers(0, 1, &vertex_buffer_view);
	get_current_resource_storage().command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	if (resource_to_flip)
		get_current_resource_storage().command_list->DrawInstanced(4, 1, 0, 0);

	if (!g_cfg.video.overlay)
		get_current_resource_storage().command_list->ResourceBarrier(
		    1, &CD3DX12_RESOURCE_BARRIER::Transition(m_backbuffer[m_swap_chain->GetCurrentBackBufferIndex()].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
	if (is_flip_surface_in_global_memory(rsx::method_registers.surface_color_target()) && resource_to_flip != nullptr)
		get_current_resource_storage().command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(resource_to_flip, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));
	CHECK_HRESULT(get_current_resource_storage().command_list->Close());
	m_command_queue->ExecuteCommandLists(1, (ID3D12CommandList**)get_current_resource_storage().command_list.GetAddressOf());

	if (g_cfg.video.overlay)
		render_overlay();

	reset_timer();

	std::chrono::time_point<steady_clock> flip_start = steady_clock::now();

	CHECK_HRESULT(m_swap_chain->Present(g_cfg.video.vsync ? 1 : 0, 0));
	// Add an event signaling queue completion

	resource_storage& storage = get_non_current_resource_storage();

	m_command_queue->Signal(storage.frame_finished_fence.Get(), storage.fence_value);
	storage.frame_finished_fence->SetEventOnCompletion(storage.fence_value, storage.frame_finished_handle);
	storage.fence_value++;

	storage.in_use = true;
	storage.dirty_textures.merge(m_rtts.invalidated_resources);
	m_rtts.invalidated_resources.clear();

	// Get the put pos - 1. This way after cleaning we can set the get ptr to
	// this value, allowing heap to proceed even if we cleant before allocating
	// a new value (that's the reason of the -1)
	storage.buffer_heap_get_pos   = m_buffer_data.get_current_put_pos_minus_one();
	storage.readback_heap_get_pos = m_readback_resources.get_current_put_pos_minus_one();

	// Now get ready for next frame
	resource_storage& new_storage = get_current_resource_storage();

	new_storage.wait_and_clean();
	if (new_storage.in_use)
	{
		m_buffer_data.m_get_pos        = new_storage.buffer_heap_get_pos;
		m_readback_resources.m_get_pos = new_storage.readback_heap_get_pos;
	}

	m_frame->flip(nullptr);

	std::chrono::time_point<steady_clock> flip_end = steady_clock::now();
	m_timers.flip_duration += std::chrono::duration_cast<std::chrono::microseconds>(flip_end - flip_start).count();
}

bool D3D12GSRender::on_access_violation(u32 address, bool is_writing)
{
	if (!is_writing)
	{
		return false;
	}

	if (invalidate_address(address))
	{
		LOG_WARNING(RSX, "Reporting Cell writing to 0x%x", address);
		return true;
	}

	return false;
}

void D3D12GSRender::reset_timer()
{
	m_timers.draw_calls_count      = 0;
	m_timers.draw_calls_duration   = 0;
	m_timers.prepare_rtt_duration  = 0;
	m_timers.vertex_index_duration = 0;
	m_timers.buffer_upload_size    = 0;
	m_timers.program_load_duration = 0;
	m_timers.constants_duration    = 0;
	m_timers.texture_duration      = 0;
	m_timers.flip_duration         = 0;
}

resource_storage& D3D12GSRender::get_current_resource_storage()
{
	return m_per_frame_storage[m_swap_chain->GetCurrentBackBufferIndex()];
}

resource_storage& D3D12GSRender::get_non_current_resource_storage()
{
	return m_per_frame_storage[1 - m_swap_chain->GetCurrentBackBufferIndex()];
}
#endif
