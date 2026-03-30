#pragma once

#include <Rendering/Renderer/Pipeline.h>

#ifdef WARP_WINDOWS

#include <d3d12.h>

// ---------------------------------------------------------------------------
// Graphics PSO
// ---------------------------------------------------------------------------

class D3D12Pipeline : public PipelineState
{
public:
	// Device-specific init — called by D3D12Device::CreatePipelineState().
	void InitializeWithDevice(ID3D12Device* device, const PipelineDesc& desc);

	// PipelineState interface
	void  Initialize(const PipelineDesc& desc) override; // no-op (use InitializeWithDevice)
	void  Cleanup() override;
	void* GetNativeHandle() const override { return m_pso.Get(); }

	// D3D12-specific accessors used by D3D12CommandList
	ID3D12PipelineState* GetNativePSO()     const { return m_pso.Get(); }
	ID3D12RootSignature* GetNativeRootSig() const { return m_rootSignature.Get(); }

private:
	void BuildRootSignature(ID3D12Device* device, const Vector<BindingSlot>& bindings,
	                        const Vector<SamplerDesc>& samplers);

	ComRef<ID3D12PipelineState> m_pso;
	ComRef<ID3D12RootSignature> m_rootSignature;
};

// ---------------------------------------------------------------------------
// Compute PSO
// ---------------------------------------------------------------------------

class D3D12ComputePipeline : public ComputePipelineState
{
public:
	// Device-specific init — called by D3D12Device::CreateComputePipelineState().
	void InitializeWithDevice(ID3D12Device* device, const ComputePipelineDesc& desc);

	// ComputePipelineState interface
	void  Initialize(const ComputePipelineDesc& desc) override; // no-op (use InitializeWithDevice)
	void  Cleanup() override;
	void* GetNativeHandle() const override { return m_pso.Get(); }

	// D3D12-specific accessors used by D3D12CommandList
	ID3D12PipelineState* GetNativePSO()     const { return m_pso.Get(); }
	ID3D12RootSignature* GetNativeRootSig() const { return m_rootSignature.Get(); }

private:
	void BuildRootSignature(ID3D12Device* device);

	ComRef<ID3D12PipelineState> m_pso;
	ComRef<ID3D12RootSignature> m_rootSignature;
};

#endif
