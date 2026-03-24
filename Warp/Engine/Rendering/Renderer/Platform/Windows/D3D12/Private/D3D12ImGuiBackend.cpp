#ifdef WARP_WINDOWS

#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12ImGuiBackend.h>
#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12Device.h>
#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12CommandList.h>
#include <Rendering/Renderer/Platform/Windows/D3D12/D3D12CommandQueue.h>
#include <Rendering/Window/Window.h>

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>

bool D3D12ImGuiBackend::Init(IWindow* window, Device* device, CommandQueue* graphicsQueue, u32 framesInFlight)
{
	D3D12Device* d3dDevice          = static_cast<D3D12Device*>(device);
	D3D12CommandQueue* d3dQueue     = static_cast<D3D12CommandQueue*>(graphicsQueue);
	ID3D12Device2* nativeDevice     = d3dDevice->GetNativeDevice();

	// Create a small dedicated SRV heap for ImGui's font texture.
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.NumDescriptors = 1;
	heapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(nativeDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_fontSrvHeap)));

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	ImGui::StyleColorsDark();

	HWND hwnd = static_cast<HWND>(window->GetNativeHandle());
	ImGui_ImplWin32_Init(hwnd);

	ImGui_ImplDX12_InitInfo initInfo;
	initInfo.Device            = nativeDevice;
	initInfo.CommandQueue      = d3dQueue->GetNative();
	initInfo.NumFramesInFlight = static_cast<int>(framesInFlight);
	initInfo.RTVFormat         = DXGI_FORMAT_B8G8R8A8_UNORM;
	initInfo.SrvDescriptorHeap = m_fontSrvHeap.Get();
	initInfo.LegacySingleSrvCpuDescriptor = m_fontSrvHeap->GetCPUDescriptorHandleForHeapStart();
	initInfo.LegacySingleSrvGpuDescriptor = m_fontSrvHeap->GetGPUDescriptorHandleForHeapStart();
	ImGui_ImplDX12_Init(&initInfo);

	return true;
}

void D3D12ImGuiBackend::Shutdown()
{
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	m_fontSrvHeap.Reset();
}

void D3D12ImGuiBackend::NewFrame()
{
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void D3D12ImGuiBackend::Render(CommandList* commandList)
{
	ImGui::Render();

	D3D12CommandList* d3dCmd = static_cast<D3D12CommandList*>(commandList);
	ID3D12GraphicsCommandList* nativeCmd = d3dCmd->GetNative();

	// Bind ImGui's own SRV heap for the font texture.
	ID3D12DescriptorHeap* heaps[] = { m_fontSrvHeap.Get() };
	nativeCmd->SetDescriptorHeaps(1, heaps);

	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), nativeCmd);
}

// Factory implementation
URef<ImGuiBackend> CreateImGuiBackend()
{
	return std::make_unique<D3D12ImGuiBackend>();
}

#endif
