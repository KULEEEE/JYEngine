#include "engine/render/JRenderContext.h"

J_RENDER_BEGIN

JRenderContext::JRenderContext(JDevice* device)
	: _device(device)
{
}

JRenderContext::~JRenderContext()
{
	destroyImmediateUploadContext();
}

bool JRenderContext::BeginUploadBatch()
{
	if (_uploadBatchActive)
	{
		return false;
	}

	// 여러 buffer copy를 하나의 command list에 모으기 시작함.
	if (!beginUploadCommandList())
	{
		return false;
	}

	_uploadBatchActive = true;
	return true;
}

bool JRenderContext::EndUploadBatch()
{
	if (!_uploadBatchActive)
	{
		return true;
	}

	// batch에 기록된 copy를 한 번만 submit하고 fence 대기함.
	const bool result = executeUploadCommandList();
	_uploadBatchActive = false;
	_batchedUploadBuffers.clear();
	return result;
}

bool JRenderContext::ensureImmediateUploadContext()
{
	if (_uploadCommandQueue != nullptr
		&& _uploadCommandAllocator != nullptr
		&& _uploadCommandList != nullptr
		&& _uploadFence != nullptr
		&& _uploadFenceEvent != INVALID_HANDLE_VALUE)
	{
		return true;
	}

	// upload 전용 queue/list/fence는 필요할 때 한 번 만들고 재사용함.
	ComPtr<ID3D12Device> device = _device != nullptr ? _device->GetDevice() : nullptr;
	if (device == nullptr)
	{
		return false;
	}

	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	HRESULT hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&_uploadCommandQueue));
	if (FAILED(hr) || _uploadCommandQueue == nullptr)
	{
		return false;
	}

	hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_uploadCommandAllocator));
	if (FAILED(hr) || _uploadCommandAllocator == nullptr)
	{
		destroyImmediateUploadContext();
		return false;
	}

	hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _uploadCommandAllocator.Get(), nullptr, IID_PPV_ARGS(&_uploadCommandList));
	if (FAILED(hr) || _uploadCommandList == nullptr)
	{
		destroyImmediateUploadContext();
		return false;
	}

	hr = _uploadCommandList->Close();
	if (FAILED(hr))
	{
		destroyImmediateUploadContext();
		return false;
	}

	hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_uploadFence));
	if (FAILED(hr) || _uploadFence == nullptr)
	{
		destroyImmediateUploadContext();
		return false;
	}

	_uploadFenceEvent = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (_uploadFenceEvent == nullptr)
	{
		destroyImmediateUploadContext();
		return false;
	}

	_uploadFenceValue = 1;
	return true;
}

bool JRenderContext::beginUploadCommandList()
{
	if (!ensureImmediateUploadContext())
	{
		return false;
	}

	if (_uploadCommandListOpen)
	{
		return true;
	}

	HRESULT hr = _uploadCommandAllocator->Reset();
	if (FAILED(hr))
	{
		return false;
	}

	hr = _uploadCommandList->Reset(_uploadCommandAllocator.Get(), nullptr);
	if (FAILED(hr))
	{
		return false;
	}

	_uploadCommandListOpen = true;
	return true;
}

bool JRenderContext::recordBufferUpload(ID3D12Resource* destination, ComPtr<ID3D12Resource> uploadBuffer, size_t size, D3D12_RESOURCE_STATES finalState)
{
	if (destination == nullptr || uploadBuffer == nullptr || size == 0)
	{
		return false;
	}

	if (!beginUploadCommandList())
	{
		return false;
	}

	CD3DX12_RESOURCE_BARRIER barriers[] =
	{
		CD3DX12_RESOURCE_BARRIER::Transition(
			destination,
			D3D12_RESOURCE_STATE_COMMON,
			D3D12_RESOURCE_STATE_COPY_DEST),
		CD3DX12_RESOURCE_BARRIER::Transition(
			destination,
			D3D12_RESOURCE_STATE_COPY_DEST,
			finalState)
	};

	// DEFAULT buffer는 COMMON -> COPY_DEST -> 실제 사용 상태로 명시 전환함.
	_uploadCommandList->ResourceBarrier(1, &barriers[0]);
	_uploadCommandList->CopyBufferRegion(destination, 0, uploadBuffer.Get(), 0, size);
	_uploadCommandList->ResourceBarrier(1, &barriers[1]);

	if (_uploadBatchActive)
	{
		// batch가 끝날 때까지 staging buffer가 살아 있어야함.
		_batchedUploadBuffers.push_back(uploadBuffer);
		return true;
	}

	// batch가 아니면 즉시 submit하고 완료까지 기다림.
	_batchedUploadBuffers.push_back(uploadBuffer);
	const bool result = executeUploadCommandList();
	_batchedUploadBuffers.clear();
	return result;
}

bool JRenderContext::executeUploadCommandList()
{
	if (!_uploadCommandListOpen)
	{
		return true;
	}

	HRESULT hr = _uploadCommandList->Close();
	if (FAILED(hr))
	{
		return false;
	}

	ID3D12CommandList* commandLists[] = { _uploadCommandList.Get() };
	_uploadCommandQueue->ExecuteCommandLists(1, commandLists);

	// staging buffer 해제 전에 GPU copy 완료를 보장해야함.
	const uint64 signalValue = _uploadFenceValue++;
	hr = _uploadCommandQueue->Signal(_uploadFence.Get(), signalValue);
	if (FAILED(hr))
	{
		return false;
	}

	if (_uploadFence->GetCompletedValue() < signalValue)
	{
		hr = _uploadFence->SetEventOnCompletion(signalValue, _uploadFenceEvent);
		if (FAILED(hr))
		{
			return false;
		}
		::WaitForSingleObject(_uploadFenceEvent, INFINITE);
	}

	_uploadCommandListOpen = false;
	return true;
}

void JRenderContext::destroyImmediateUploadContext()
{
	if (_uploadBatchActive)
	{
		EndUploadBatch();
	}
	else if (_uploadCommandListOpen)
	{
		executeUploadCommandList();
		_batchedUploadBuffers.clear();
	}

	if (_uploadCommandQueue != nullptr && _uploadFence != nullptr)
	{
		// context 해제 전에 남은 upload command가 끝났는지 확인함.
		const uint64 signalValue = _uploadFenceValue++;
		if (SUCCEEDED(_uploadCommandQueue->Signal(_uploadFence.Get(), signalValue)))
		{
			if (_uploadFence->GetCompletedValue() < signalValue)
			{
				if (_uploadFenceEvent != INVALID_HANDLE_VALUE
					&& SUCCEEDED(_uploadFence->SetEventOnCompletion(signalValue, _uploadFenceEvent)))
				{
					::WaitForSingleObject(_uploadFenceEvent, INFINITE);
				}
			}
		}
	}

	if (_uploadFenceEvent != INVALID_HANDLE_VALUE)
	{
		::CloseHandle(_uploadFenceEvent);
		_uploadFenceEvent = INVALID_HANDLE_VALUE;
	}

	_uploadFence.Reset();
	_uploadCommandList.Reset();
	_uploadCommandAllocator.Reset();
	_uploadCommandQueue.Reset();
}

J_RENDER_END
