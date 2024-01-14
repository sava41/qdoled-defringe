#include <windows.h>
#include <MinHook.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <psapi.h>
#include <cstdio>
#include <intrin.h>
#include <vector>

#include <io.h>
#include <string>
#include <sstream>
#include <iostream>
#include <iomanip>

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")

#pragma intrinsic(_ReturnAddress)

#define RESIZE(x, y) realloc(x, (y) * sizeof(*x));
#define RELEASE_IF_NOT_NULL(x) { if (x != NULL) { x->Release(); } }

#define LOG_FILE_PATH R"(C:\DWMLOG\dwm.log)"
#define MAX_LOG_FILE_SIZE 20 * 1024 * 1024
#define DEBUG_MODE true

#if DEBUG_MODE == true
#define __LOG_ONLY_ONCE(x, y) if (static bool first_log_##y = true) { log_to_file(x); first_log_##y = false; }
#define _LOG_ONLY_ONCE(x, y) __LOG_ONLY_ONCE(x, y)
#define LOG_ONLY_ONCE(x) _LOG_ONLY_ONCE(x, __COUNTER__)
#define MESSAGE_BOX_DBG(x, y) MessageBoxA(NULL, x, "DEBUG HOOK DWM", y);

#define EXECUTE_WITH_LOG(winapi_func_hr) \
	do { \
		HRESULT hr = (winapi_func_hr); \
		if (FAILED(hr)) \
		{ \
			std::stringstream ss; \
			ss << "ERROR AT LINE: " << __LINE__ << " HR: " << hr << " - DETAILS: "; \
			LPSTR error_message = nullptr; \
			FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, \
				NULL, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&error_message, 0, NULL); \
			ss << error_message; \
			log_to_file(ss.str().c_str()); \
			LocalFree(error_message); \
			throw std::exception(ss.str().c_str()); \
		} \
	} while (false);

#define EXECUTE_D3DCOMPILE_WITH_LOG(winapi_func_hr, error_interface) \
	do { \
		HRESULT hr = (winapi_func_hr); \
		if (FAILED(hr)) \
		{ \
			std::stringstream ss; \
			ss << "ERROR AT LINE: " << __LINE__ << " HR: " << hr << " - DETAILS: "; \
			LPSTR error_message = nullptr; \
			FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, \
				NULL, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&error_message, 0, NULL); \
			ss << error_message << " - DX COMPILE ERROR: " << (char*)error_interface->GetBufferPointer(); \
			error_interface->Release(); \
			log_to_file(ss.str().c_str()); \
			LocalFree(error_message); \
			throw std::exception(ss.str().c_str()); \
		} \
	} while (false);

#define LOG_ADDRESS(prefix_message, address) \
	{ \
		std::stringstream ss; \
		ss << prefix_message << " 0x" << std::setw(sizeof(address) * 2) << std::setfill('0') << std::hex << (UINT_PTR)address; \
		log_to_file(ss.str().c_str()); \
	}

#else
#define LOG_ONLY_ONCE(x) // NOP, not in debug mode
#define MESSAGE_BOX_DBG(x, y) // NOP, not in debug mode
#define EXECUTE_WITH_LOG(winapi_func_hr) winapi_func_hr;
#define EXECUTE_D3DCOMPILE_WITH_LOG(winapi_func_hr, error_interface) winapi_func_hr;
#define LOG_ADDRESS(prefix_message, address) // NOP, not in debug mode
#endif


#if DEBUG_MODE == true
void print_error(const char* prefix_message)
{
	DWORD errorCode = GetLastError();
	LPSTR errorMessage = nullptr;
	FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				nullptr, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&errorMessage, 0, nullptr);

	char message_buf[100];
	sprintf(message_buf, "%s: %s - error code: %u", prefix_message, errorMessage, errorCode);
	MESSAGE_BOX_DBG(message_buf, MB_OK | MB_ICONWARNING)
	return;
}

void log_to_file(const char* log_buf)
{
	FILE* pFile = fopen(LOG_FILE_PATH, "a");
	if (pFile == NULL)
	{
		// print_error("Error during logging"); // Comment out to prevent UI freeze when used inside hooked functions
		return;
	}
	fseek(pFile, 0, SEEK_END);
	long size = ftell(pFile);
	if (size > MAX_LOG_FILE_SIZE)
	{
		if (_chsize(_fileno(pFile), 0) == -1)
		{
			fclose(pFile);
			return;
		}
	}
	fseek(pFile, 0, SEEK_END);
	fprintf(pFile, "%s\n", log_buf);
	fclose(pFile);
}
#endif


const unsigned char COverlayContext_Present_bytes[] = {
	0x48, 0x89, 0x5c, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10, 0x57, 0x48, 0x83, 0xec, 0x40, 0x48, 0x8b, 0xb1, 0x20,
	0x2c, 0x00, 0x00, 0x45, 0x8b, 0xd0, 0x48, 0x8b, 0xfa, 0x48, 0x8b, 0xd9, 0x48, 0x85, 0xf6, 0x0f, 0x85
};
const int IOverlaySwapChain_IDXGISwapChain_offset = -0x118;

const unsigned char COverlayContext_IsCandidateDirectFlipCompatbile_bytes[] = {
	0x48, 0x89, 0x7c, 0x24, 0x20, 0x55, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x8b, 0xec, 0x48, 0x83,
	0xec, 0x40
};
const unsigned char COverlayContext_OverlaysEnabled_bytes[] = {
	0x75, 0x04, 0x32, 0xc0, 0xc3, 0xcc, 0x83, 0x79, 0x30, 0x01, 0x0f, 0x97, 0xc0, 0xc3
};

const int COverlayContext_DeviceClipBox_offset = -0x120;

const int IOverlaySwapChain_HardwareProtected_offset = -0xbc;

/*
 * AOB for function: COverlayContext_Present_bytes_w11
 *
 * 40 53 55 56 57 41 56 41 57 48 81 EC 88 00 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 44 24 78 48
 *
 */
const unsigned char COverlayContext_Present_bytes_w11[] = {
	0x40, 0x53, 0x55, 0x56, 0x57, 0x41, 0x56, 0x41, 0x57, 0x48, 0x81, 0xEC, 0x88, 0x00, 0x00, 0x00, 0x48, 0x8B, 0x05,
	'?', '?', '?', '?', 0x48, 0x33, 0xC4, 0x48, 0x89, 0x44, 0x24, 0x78, 0x48
};
const int IOverlaySwapChain_IDXGISwapChain_offset_w11 = 0xE0;

/*
 * AOB for function: COverlayContext_IsCandidateDirectFlipCompatbile_bytes_w11
 *
 * 40 55 53 56 57 41 54 41 55 41 56 41 57 48 8B EC 48 83 EC 68 48
 */
const unsigned char COverlayContext_IsCandidateDirectFlipCompatbile_bytes_w11[] = {
	0x40, 0x55, 0x53, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x8B, 0xEC, 0x48, 0x83, 0xEC,
	0x68, 0x48,
};

/*
 * AOB for function: COverlayContext_OverlaysEnabled_bytes_w11
 *
 * 83 3D ?? ?? ?? ?? ?? 75 04
 */
const unsigned char COverlayContext_OverlaysEnabled_bytes_w11[] = {
	0x83, 0x3D, '?', '?', '?', '?', '?', 0x75, 0x04
};

int COverlayContext_DeviceClipBox_offset_w11 = 0x466C;

const int IOverlaySwapChain_HardwareProtected_offset_w11 = -0x144;

bool isWindows11;

bool aob_match_inverse(const void* buf1, const void* mask, const int buf_len)
{
	for (int i = 0; i < buf_len; ++i)
	{
		if (((unsigned char*)buf1)[i] != ((unsigned char*)mask)[i] && ((unsigned char*)mask)[i] != '?')
		{
			return true;
		}
	}
	return false;
}

char shaders[] = R"(
    struct VS_INPUT {
	float2 pos : POSITION;
	float2 tex : TEXCOORD;
};

struct VS_OUTPUT {
	float4 pos : SV_POSITION;
	float2 tex : TEXCOORD;
};

Texture2D backBufferTex : register(t0);
SamplerState smp : register(s0);

VS_OUTPUT VS(VS_INPUT input) {
	VS_OUTPUT output;
	output.pos = float4(input.pos, 0, 1);
	output.tex = input.tex;
	return output;
}

float4 PS(VS_OUTPUT input) : SV_TARGET{
	float ratio = 0.35;
	float2 offset = float2(0.0, 1/1440.0);
	
	float3 sample = backBufferTex.Sample(smp, input.tex).rgb;
	float3 sampleOffset = backBufferTex.Sample(smp, input.tex - offset).rgb;

	sample.g = 0;(1 - ratio) * sample.g + ratio * sampleOffset.g;

	return float4(sample, 1);
}
)";

ID3D11Device* device;
ID3D11DeviceContext* deviceContext;
ID3D11VertexShader* vertexShader;
ID3D11PixelShader* pixelShader;
ID3D11InputLayout* inputLayout;

ID3D11Buffer* vertexBuffer;
UINT numVerts;
UINT stride;
UINT offset;

D3D11_TEXTURE2D_DESC backBufferDesc;
D3D11_TEXTURE2D_DESC textureDesc[2];

ID3D11SamplerState* samplerState;
ID3D11Texture2D* texture[2];
ID3D11ShaderResourceView* textureView[2];

ID3D11Buffer* constantBuffer;

void DrawRectangle(struct tagRECT* rect, int index)
{
	float width = backBufferDesc.Width;
	float height = backBufferDesc.Height;

	float screenLeft = rect->left / width;
	float screenTop = rect->top / height;
	float screenRight = rect->right / width;
	float screenBottom = rect->bottom / height;

	float left = screenLeft * 2 - 1;
	float top = screenTop * -2 + 1;
	float right = screenRight * 2 - 1;
	float bottom = screenBottom * -2 + 1;

	width = textureDesc[index].Width;
	height = textureDesc[index].Height;
	float texLeft = rect->left / width;
	float texTop = rect->top / height;
	float texRight = rect->right / width;
	float texBottom = rect->bottom / height;

	float vertexData[] = {
		left, bottom, texLeft, texBottom,
		left, top, texLeft, texTop,
		right, bottom, texRight, texBottom,
		right, top, texRight, texTop
	};

	D3D11_MAPPED_SUBRESOURCE resource;
	deviceContext->Map(vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);
	memcpy(resource.pData, vertexData, stride * numVerts);
	deviceContext->Unmap(vertexBuffer, 0);

	deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);

	deviceContext->Draw(numVerts, 0);
}

int numLutTargets;
void** lutTargets;

bool IsLUTActive(void* target)
{
	for (int i = 0; i < numLutTargets; i++)
	{
		if (lutTargets[i] == target)
		{
			return true;
		}
	}
	return false;
}

void SetLUTActive(void* target)
{
	if (!IsLUTActive(target))
	{
		lutTargets = (void**)RESIZE(lutTargets, numLutTargets + 1)
		lutTargets[numLutTargets++] = target;
	}
}

void UnsetLUTActive(void* target)
{
	for (int i = 0; i < numLutTargets; i++)
	{
		if (lutTargets[i] == target)
		{
			lutTargets[i] = lutTargets[--numLutTargets];
			lutTargets = (void**)RESIZE(lutTargets, numLutTargets)
			return;
		}
	}
}

void InitializeStuff(IDXGISwapChain* swapChain)
{
	try
	{
		swapChain->GetDevice(IID_ID3D11Device, (void**)&device);

		device->GetImmediateContext(&deviceContext);
		{
			ID3DBlob* vsBlob;
			ID3DBlob* compile_error_interface;
			D3DCompile(shaders, sizeof shaders, NULL, NULL, NULL, "VS", "vs_5_0", 0, 0, &vsBlob, &
				compile_error_interface);

			device->CreateVertexShader(vsBlob->GetBufferPointer(),
				vsBlob->GetBufferSize(), NULL, &vertexShader);

			D3D11_INPUT_ELEMENT_DESC inputElementDesc[] =
			{
				{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
				{
					"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT,
					D3D11_INPUT_PER_VERTEX_DATA, 0
				}
			};
			device->CreateInputLayout(inputElementDesc, ARRAYSIZE(inputElementDesc),
				vsBlob->GetBufferPointer(),
				vsBlob->GetBufferSize(), &inputLayout);

			vsBlob->Release();
		}
		{
			ID3DBlob* psBlob;
			ID3DBlob* compile_error_interface;
			D3DCompile(shaders, sizeof shaders, NULL, NULL, NULL, "PS", "ps_5_0", 0, 0, &psBlob, &
				compile_error_interface);

			device->CreatePixelShader(psBlob->GetBufferPointer(),
			                          psBlob->GetBufferSize(), NULL, &pixelShader);
			psBlob->Release();
		}
		{
			stride = 4 * sizeof(float);
			numVerts = 4;
			offset = 0;

			D3D11_BUFFER_DESC vertexBufferDesc = {};
			vertexBufferDesc.ByteWidth = stride * numVerts;
			vertexBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
			vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			vertexBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

			device->CreateBuffer(&vertexBufferDesc, NULL, &vertexBuffer);
		}
		{
			D3D11_SAMPLER_DESC samplerDesc = {};
			samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
			samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
			samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;

			device->CreateSamplerState(&samplerDesc, &samplerState);
		}
		{
			D3D11_BUFFER_DESC constantBufferDesc = {};
			constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			constantBufferDesc.ByteWidth = 16;
			constantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
			constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

			device->CreateBuffer(&constantBufferDesc, NULL, &constantBuffer);
		}
	}
	catch (std::exception& ex)
	{
		std::stringstream ex_message;
		ex_message << "Exception caught at line " << __LINE__ << ": " << ex.what() << std::endl;
		throw;
	}
	catch (...)
	{
		std::stringstream ex_message;
		ex_message << "Exception caught at line " << __LINE__ << ": " << std::endl;
		throw;
	}
}

void UninitializeStuff()
{
	RELEASE_IF_NOT_NULL(device)
	RELEASE_IF_NOT_NULL(deviceContext)
	RELEASE_IF_NOT_NULL(vertexShader)
	RELEASE_IF_NOT_NULL(pixelShader)
	RELEASE_IF_NOT_NULL(inputLayout)
	RELEASE_IF_NOT_NULL(vertexBuffer)
	RELEASE_IF_NOT_NULL(samplerState)
	for (int i = 0; i < 2; i++)
	{
		RELEASE_IF_NOT_NULL(texture[i])
		RELEASE_IF_NOT_NULL(textureView[i])
	}
	RELEASE_IF_NOT_NULL(constantBuffer)
	free(lutTargets);
}

bool ApplyLUT(void* cOverlayContext, IDXGISwapChain* swapChain, struct tagRECT* rects, int numRects)
{
	try
	{
		if (!device)
		{
			InitializeStuff(swapChain);
		}

		ID3D11Texture2D* backBuffer;
		ID3D11RenderTargetView* renderTargetView;


		swapChain->GetBuffer(0, IID_ID3D11Texture2D, (void**)&backBuffer);

		D3D11_TEXTURE2D_DESC newBackBufferDesc;
		backBuffer->GetDesc(&newBackBufferDesc);

		int index = -1;
		if (newBackBufferDesc.Format == DXGI_FORMAT_B8G8R8A8_UNORM)
		{
			index = 0;
		}
		else if (newBackBufferDesc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT)
		{
			index = 1;
		}

		D3D11_TEXTURE2D_DESC oldTextureDesc = textureDesc[index];
		if (newBackBufferDesc.Width > oldTextureDesc.Width || newBackBufferDesc.Height > oldTextureDesc.Height)
		{
			if (texture[index] != NULL)
			{
				texture[index]->Release();
				textureView[index]->Release();
			}

			UINT newWidth = max(newBackBufferDesc.Width, oldTextureDesc.Width);
			UINT newHeight = max(newBackBufferDesc.Height, oldTextureDesc.Height);

			D3D11_TEXTURE2D_DESC newTextureDesc;

			newTextureDesc = newBackBufferDesc;
			newTextureDesc.Width = newWidth;
			newTextureDesc.Height = newHeight;
			newTextureDesc.Usage = D3D11_USAGE_DEFAULT;
			newTextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			newTextureDesc.CPUAccessFlags = 0;
			newTextureDesc.MiscFlags = 0;

			textureDesc[index] = newTextureDesc;

			device->CreateTexture2D(&textureDesc[index], NULL, &texture[index]);
			device->CreateShaderResourceView((ID3D11Resource*)texture[index], NULL, &textureView[index]);
		}

		backBufferDesc = newBackBufferDesc;

		device->CreateRenderTargetView((ID3D11Resource*)backBuffer, NULL, &renderTargetView);
		const D3D11_VIEWPORT d3d11_viewport(0, 0, backBufferDesc.Width, backBufferDesc.Height, 0.0f, 1.0f);
		deviceContext->RSSetViewports(1, &d3d11_viewport);

		deviceContext->OMSetRenderTargets(1, &renderTargetView, NULL);
		renderTargetView->Release();

		deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		deviceContext->IASetInputLayout(inputLayout);

		deviceContext->VSSetShader(vertexShader, NULL, 0);
		deviceContext->PSSetShader(pixelShader, NULL, 0);

		deviceContext->PSSetShaderResources(0, 1, &textureView[index]);
		deviceContext->PSSetSamplers(0, 1, &samplerState);

		for (int i = 0; i < numRects; i++)
		{
			D3D11_BOX sourceRegion;
			sourceRegion.left = rects[i].left;
			sourceRegion.right = rects[i].right;
			sourceRegion.top = rects[i].top;
			sourceRegion.bottom = rects[i].bottom;
			sourceRegion.front = 0;
			sourceRegion.back = 1;

			deviceContext->CopySubresourceRegion((ID3D11Resource*)texture[index], 0, rects[i].left,
			                                     rects[i].top, 0, (ID3D11Resource*)backBuffer, 0, &sourceRegion);
			DrawRectangle(&rects[i], index);
		}

		backBuffer->Release();
		return true;
	}
	catch (std::exception& ex)
	{
		std::stringstream ex_message;
		ex_message << "Exception caught at line " << __LINE__ << ": " << ex.what() << std::endl;
		return false;
	}
	catch (...)
	{
		std::stringstream ex_message;
		ex_message << "Exception caught at line " << __LINE__ << std::endl;
		return false;
	}
}

typedef struct rectVec
{
	struct tagRECT* start;
	struct tagRECT* end;
	struct tagRECT* cap;
} rectVec;

typedef long (COverlayContext_Present_t)(void*, void*, unsigned int, rectVec*, unsigned int, bool);

COverlayContext_Present_t* COverlayContext_Present_orig;
COverlayContext_Present_t* COverlayContext_Present_real_orig;


long COverlayContext_Present_hook(void* self, void* overlaySwapChain, unsigned int a3, rectVec* rectVec,
                                  unsigned int a5, bool a6)
{
	if (_ReturnAddress() < (void*)COverlayContext_Present_real_orig)
	{
		LOG_ONLY_ONCE("I am inside COverlayContext::Present hook inside the main if condition")

		if (isWindows11 && *((bool*)overlaySwapChain + IOverlaySwapChain_HardwareProtected_offset_w11) ||
			!isWindows11 && *((bool*)overlaySwapChain + IOverlaySwapChain_HardwareProtected_offset))
		{
			std::stringstream hw_protection_message;
			hw_protection_message << "I'm inside the Hardware protection condition - 0x" << std::hex << (bool*)
				overlaySwapChain + IOverlaySwapChain_HardwareProtected_offset_w11 << " - value: 0x" << *((bool*)
					overlaySwapChain + IOverlaySwapChain_HardwareProtected_offset_w11);
			LOG_ONLY_ONCE(hw_protection_message.str().c_str())
			UnsetLUTActive(self);
		}
		else
		{
			std::stringstream hw_protection_message;
			hw_protection_message << "I'm outside the Hardware protection condition - 0x" << std::hex << (bool*)
				overlaySwapChain + IOverlaySwapChain_HardwareProtected_offset_w11 << " - value: 0x" << *((bool*)
					overlaySwapChain + IOverlaySwapChain_HardwareProtected_offset_w11);
			LOG_ONLY_ONCE(hw_protection_message.str().c_str())

			IDXGISwapChain* swapChain;
			if (isWindows11)
			{
				LOG_ONLY_ONCE("Gathering IDXGISwapChain pointer")
				int sub_from_legacy_swapchain = *(int*)((unsigned char*)overlaySwapChain - 4);
				void* real_overlay_swap_chain = (unsigned char*)overlaySwapChain - sub_from_legacy_swapchain -
					0x1b0;
				swapChain = *(IDXGISwapChain**)((unsigned char*)real_overlay_swap_chain +
					IOverlaySwapChain_IDXGISwapChain_offset_w11);
			}
			else
			{
				swapChain = *(IDXGISwapChain**)((unsigned char*)overlaySwapChain +
					IOverlaySwapChain_IDXGISwapChain_offset);
			}

			if (ApplyLUT(self, swapChain, rectVec->start, rectVec->end - rectVec->start))
			{
				LOG_ONLY_ONCE("Setting LUTactive")
				SetLUTActive(self);
			}
			else
			{
				LOG_ONLY_ONCE("Un-setting LUTactive")
				UnsetLUTActive(self);
			}
		}
	}

	return COverlayContext_Present_orig(self, overlaySwapChain, a3, rectVec, a5, a6);
}

typedef bool (COverlayContext_IsCandidateDirectFlipCompatbile_t)(void*, void*, void*, void*, int, unsigned int, bool,
                                                                 bool);

COverlayContext_IsCandidateDirectFlipCompatbile_t* COverlayContext_IsCandidateDirectFlipCompatbile_orig;

bool COverlayContext_IsCandidateDirectFlipCompatbile_hook(void* self, void* a2, void* a3, void* a4, int a5,
                                                          unsigned int a6, bool a7, bool a8)
{
	if (IsLUTActive(self))
	{
		return false;
	}
	return COverlayContext_IsCandidateDirectFlipCompatbile_orig(self, a2, a3, a4, a5, a6, a7, a8);
}

typedef bool (COverlayContext_OverlaysEnabled_t)(void*);

COverlayContext_OverlaysEnabled_t* COverlayContext_OverlaysEnabled_orig;

bool COverlayContext_OverlaysEnabled_hook(void* self)
{
	if (IsLUTActive(self))
	{
		return false;
	}
	return COverlayContext_OverlaysEnabled_orig(self);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved)
{
	MESSAGE_BOX_DBG("HEHE", MB_OK)
	printf("lel\n");
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		{
			HMODULE dwmcore = GetModuleHandle("dwmcore.dll");
			MODULEINFO moduleInfo;
			GetModuleInformation(GetCurrentProcess(), dwmcore, &moduleInfo, sizeof moduleInfo);

			OSVERSIONINFOEX versionInfo;
			ZeroMemory(&versionInfo, sizeof OSVERSIONINFOEX);
			versionInfo.dwOSVersionInfoSize = sizeof OSVERSIONINFOEX;
			versionInfo.dwBuildNumber = 22000;

			ULONGLONG dwlConditionMask = 0;
			VER_SET_CONDITION(dwlConditionMask, VER_BUILDNUMBER, VER_GREATER_EQUAL);

			if (VerifyVersionInfo(&versionInfo, VER_BUILDNUMBER, dwlConditionMask))
			{
				isWindows11 = true;
			}
			else
			{
				isWindows11 = false;
			}

			if (isWindows11)
			{
				for (size_t i = 0; i <= moduleInfo.SizeOfImage - sizeof COverlayContext_OverlaysEnabled_bytes_w11; i++)
				{
					unsigned char* address = (unsigned char*)dwmcore + i;
					if (!COverlayContext_Present_orig && sizeof COverlayContext_Present_bytes_w11 <= moduleInfo.
						SizeOfImage - i && !aob_match_inverse(address, COverlayContext_Present_bytes_w11,
						                                      sizeof COverlayContext_Present_bytes_w11))
					{
						COverlayContext_Present_orig = (COverlayContext_Present_t*)address;
						COverlayContext_Present_real_orig = COverlayContext_Present_orig;
					}
					else if (!COverlayContext_IsCandidateDirectFlipCompatbile_orig && sizeof
						COverlayContext_IsCandidateDirectFlipCompatbile_bytes_w11 <= moduleInfo.SizeOfImage - i && !
						aob_match_inverse(
							address, COverlayContext_IsCandidateDirectFlipCompatbile_bytes_w11,
							sizeof COverlayContext_IsCandidateDirectFlipCompatbile_bytes_w11))
					{
						COverlayContext_IsCandidateDirectFlipCompatbile_orig = (
							COverlayContext_IsCandidateDirectFlipCompatbile_t*)address;
					}
					else if (!COverlayContext_OverlaysEnabled_orig && sizeof COverlayContext_OverlaysEnabled_bytes_w11
						<= moduleInfo.SizeOfImage - i && !aob_match_inverse(
							address, COverlayContext_OverlaysEnabled_bytes_w11,
							sizeof COverlayContext_OverlaysEnabled_bytes_w11))
					{
						COverlayContext_OverlaysEnabled_orig = (COverlayContext_OverlaysEnabled_t*)address;
					}
					if (COverlayContext_Present_orig && COverlayContext_IsCandidateDirectFlipCompatbile_orig &&
						COverlayContext_OverlaysEnabled_orig)
					{
						break;
					}
				}

				DWORD rev;
				DWORD revSize = sizeof(rev);
				RegGetValueA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "UBR", RRF_RT_DWORD,
				             NULL, &rev, &revSize);

				if (rev >= 706)
				{
					// COverlayContext_DeviceClipBox_offset_w11 += 8;
				}
			}
			else
			{
				for (size_t i = 0; i <= moduleInfo.SizeOfImage - sizeof(COverlayContext_Present_bytes); i++)
				{
					unsigned char* address = (unsigned char*)dwmcore + i;
					if (!COverlayContext_Present_orig && !memcmp(address, COverlayContext_Present_bytes,
					                                             sizeof(COverlayContext_Present_bytes)))
					{
						COverlayContext_Present_orig = (COverlayContext_Present_t*)address;
						COverlayContext_Present_real_orig = COverlayContext_Present_orig;
					}
					else if (!COverlayContext_IsCandidateDirectFlipCompatbile_orig && !memcmp(
						address, COverlayContext_IsCandidateDirectFlipCompatbile_bytes,
						sizeof(COverlayContext_IsCandidateDirectFlipCompatbile_bytes)))
					{
						static int found = 0;
						found++;
						if (found == 2)
						{
							COverlayContext_IsCandidateDirectFlipCompatbile_orig = (
								COverlayContext_IsCandidateDirectFlipCompatbile_t*)(address - 0xa);
						}
					}
					else if (!COverlayContext_OverlaysEnabled_orig && !memcmp(
						address, COverlayContext_OverlaysEnabled_bytes, sizeof(COverlayContext_OverlaysEnabled_bytes)))
					{
						COverlayContext_OverlaysEnabled_orig = (COverlayContext_OverlaysEnabled_t*)(address - 0x7);
					}
					if (COverlayContext_Present_orig && COverlayContext_IsCandidateDirectFlipCompatbile_orig &&
						COverlayContext_OverlaysEnabled_orig)
					{
						break;
					}
				}
			}

			char variable_message_states[300];
			sprintf(variable_message_states, "Current variable states: COverlayContext::Present - %p\t"
			        "COverlayContext::IsCandidateDirectFlipCompatible - %p\tCOverlayContext::OverlaysEnabled - %p",
			        COverlayContext_Present_orig,
			        COverlayContext_IsCandidateDirectFlipCompatbile_orig, COverlayContext_OverlaysEnabled_orig);

			MESSAGE_BOX_DBG(variable_message_states, MB_OK)

			if (COverlayContext_Present_orig && COverlayContext_IsCandidateDirectFlipCompatbile_orig &&
				COverlayContext_OverlaysEnabled_orig)

			{
				MH_Initialize();
				MH_CreateHook((PVOID)COverlayContext_Present_orig, (PVOID)COverlayContext_Present_hook,
				              (PVOID*)&COverlayContext_Present_orig);
				MH_CreateHook((PVOID)COverlayContext_IsCandidateDirectFlipCompatbile_orig,
				              (PVOID)COverlayContext_IsCandidateDirectFlipCompatbile_hook,
				              (PVOID*)&COverlayContext_IsCandidateDirectFlipCompatbile_orig);
				MH_CreateHook((PVOID)COverlayContext_OverlaysEnabled_orig, (PVOID)COverlayContext_OverlaysEnabled_hook,
				              (PVOID*)&COverlayContext_OverlaysEnabled_orig);
				MH_EnableHook(MH_ALL_HOOKS);
				LOG_ONLY_ONCE("DWM HOOK DLL INITIALIZATION. START LOGGING")
				MESSAGE_BOX_DBG("DWM HOOK INITIALIZATION", MB_OK)
				break;
			}
			return FALSE;
		}
	case DLL_PROCESS_DETACH:
		MH_Uninitialize();
		Sleep(100);
		UninitializeStuff();
		break;
	default:
		break;
	}
	return TRUE;
}