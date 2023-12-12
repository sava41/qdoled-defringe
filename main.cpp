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

#define RELEASE_IF_NOT_NULL(x) \
    {                          \
        if (x != NULL)         \
        {                      \
            x->Release();      \
        }                      \
    }
#define _STRINGIFY(x) #x
#define STRINGIFY(x) _STRINGIFY(x)
#define RESIZE(x, y) realloc(x, (y) * sizeof(*x));
#define LOG_FILE_PATH R"(C:\DWMLOG\dwm.log)"
#define MAX_LOG_FILE_SIZE 20 * 1024 * 1024
#define DEBUG_MODE true

#if DEBUG_MODE == true
#define __LOG_ONLY_ONCE(x, y)             \
    if (static bool first_log_##y = true) \
    {                                     \
        log_to_file(x);                   \
        first_log_##y = false;            \
    }
#define _LOG_ONLY_ONCE(x, y) __LOG_ONLY_ONCE(x, y)
#define LOG_ONLY_ONCE(x) _LOG_ONLY_ONCE(x, __COUNTER__)
#define MESSAGE_BOX_DBG(x, y) MessageBoxA(NULL, x, "DEBUG HOOK DWM", y);

#define EXECUTE_WITH_LOG(winapi_func_hr)                                                                                \
    do                                                                                                                  \
    {                                                                                                                   \
        HRESULT hr = (winapi_func_hr);                                                                                  \
        if (FAILED(hr))                                                                                                 \
        {                                                                                                               \
            std::stringstream ss;                                                                                       \
            ss << "ERROR AT LINE: " << __LINE__ << " HR: " << hr << " - DETAILS: ";                                     \
            LPSTR error_message = nullptr;                                                                              \
            FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, \
                           NULL, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR) & error_message, 0, NULL);      \
            ss << error_message;                                                                                        \
            log_to_file(ss.str().c_str());                                                                              \
            LocalFree(error_message);                                                                                   \
            throw std::exception(ss.str().c_str());                                                                     \
        }                                                                                                               \
    } while (false);

#define EXECUTE_D3DCOMPILE_WITH_LOG(winapi_func_hr, error_interface)                                                    \
    do                                                                                                                  \
    {                                                                                                                   \
        HRESULT hr = (winapi_func_hr);                                                                                  \
        if (FAILED(hr))                                                                                                 \
        {                                                                                                               \
            std::stringstream ss;                                                                                       \
            ss << "ERROR AT LINE: " << __LINE__ << " HR: " << hr << " - DETAILS: ";                                     \
            LPSTR error_message = nullptr;                                                                              \
            FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, \
                           NULL, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR) & error_message, 0, NULL);      \
            ss << error_message << " - DX COMPILE ERROR: " << (char *)error_interface->GetBufferPointer();              \
            error_interface->Release();                                                                                 \
            log_to_file(ss.str().c_str());                                                                              \
            LocalFree(error_message);                                                                                   \
            throw std::exception(ss.str().c_str());                                                                     \
        }                                                                                                               \
    } while (false);

#define LOG_ADDRESS(prefix_message, address)                                                                                   \
    {                                                                                                                          \
        std::stringstream ss;                                                                                                  \
        ss << prefix_message << " 0x" << std::setw(sizeof(address) * 2) << std::setfill('0') << std::hex << (UINT_PTR)address; \
        log_to_file(ss.str().c_str());                                                                                         \
    }

#else
#define LOG_ONLY_ONCE(x)      // NOP, not in debug mode
#define MESSAGE_BOX_DBG(x, y) // NOP, not in debug mode
#define EXECUTE_WITH_LOG(winapi_func_hr) winapi_func_hr;
#define EXECUTE_D3DCOMPILE_WITH_LOG(winapi_func_hr, error_interface) winapi_func_hr;
#define LOG_ADDRESS(prefix_message, address) // NOP, not in debug mode
#endif

#if DEBUG_MODE == true
void print_error(const char *prefix_message)
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

void log_to_file(const char *log_buf)
{
    FILE *pFile = fopen(LOG_FILE_PATH, "a");
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
    0x2c, 0x00, 0x00, 0x45, 0x8b, 0xd0, 0x48, 0x8b, 0xfa, 0x48, 0x8b, 0xd9, 0x48, 0x85, 0xf6, 0x0f, 0x85};
const int IOverlaySwapChain_IDXGISwapChain_offset = -0x118;

const unsigned char COverlayContext_IsCandidateDirectFlipCompatbile_bytes[] = {
    0x48, 0x89, 0x7c, 0x24, 0x20, 0x55, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x8b, 0xec, 0x48, 0x83,
    0xec, 0x40};
const unsigned char COverlayContext_OverlaysEnabled_bytes[] = {
    0x75, 0x04, 0x32, 0xc0, 0xc3, 0xcc, 0x83, 0x79, 0x30, 0x01, 0x0f, 0x97, 0xc0, 0xc3};

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
    '?', '?', '?', '?', 0x48, 0x33, 0xC4, 0x48, 0x89, 0x44, 0x24, 0x78, 0x48};
const int IOverlaySwapChain_IDXGISwapChain_offset_w11 = 0xE0;

/*
 * AOB for function: COverlayContext_IsCandidateDirectFlipCompatbile_bytes_w11
 *
 * 40 55 53 56 57 41 54 41 55 41 56 41 57 48 8B EC 48 83 EC 68 48
 */
const unsigned char COverlayContext_IsCandidateDirectFlipCompatbile_bytes_w11[] = {
    0x40,
    0x55,
    0x53,
    0x56,
    0x57,
    0x41,
    0x54,
    0x41,
    0x55,
    0x41,
    0x56,
    0x41,
    0x57,
    0x48,
    0x8B,
    0xEC,
    0x48,
    0x83,
    0xEC,
    0x68,
    0x48,
};

/*
 * AOB for function: COverlayContext_OverlaysEnabled_bytes_w11
 *
 * 83 3D ?? ?? ?? ?? ?? 75 04
 */
const unsigned char COverlayContext_OverlaysEnabled_bytes_w11[] = {
    0x83, 0x3D, '?', '?', '?', '?', '?', 0x75, 0x04};

int COverlayContext_DeviceClipBox_offset_w11 = 0x466C;

const int IOverlaySwapChain_HardwareProtected_offset_w11 = -0x144;

bool isWindows11;

bool aob_match_inverse(const void *buf1, const void *mask, const int buf_len)
{
    for (int i = 0; i < buf_len; ++i)
    {
        if (((unsigned char *)buf1)[i] != ((unsigned char *)mask)[i] && ((unsigned char *)mask)[i] != '?')
        {
            return true;
        }
    }
    return false;
}

ID3D11Device *device;
ID3D11DeviceContext *deviceContext;
ID3D11VertexShader *vertexShader;
ID3D11PixelShader *pixelShader;
ID3D11InputLayout *inputLayout;

ID3D11Buffer *vertexBuffer;
UINT numVerts;
UINT stride;
UINT offset;

D3D11_TEXTURE2D_DESC backBufferDesc;
D3D11_TEXTURE2D_DESC textureDesc[2];

ID3D11SamplerState *samplerState;
ID3D11Texture2D *texture[2];
ID3D11ShaderResourceView *textureView[2];

ID3D11SamplerState *noiseSamplerState;
ID3D11ShaderResourceView *noiseTextureView;

ID3D11Buffer *constantBuffer;

struct filterData
{
    int left;
    int top;
    int size;
    bool isHdr;
    ID3D11ShaderResourceView *textureView;
};

void DrawRectangle(struct tagRECT *rect, int index)
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
        right, top, texRight, texTop};

    D3D11_MAPPED_SUBRESOURCE resource;
    EXECUTE_WITH_LOG(deviceContext->Map(vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource))
    memcpy(resource.pData, vertexData, stride * numVerts);
    deviceContext->Unmap(vertexBuffer, 0);

    deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);

    deviceContext->Draw(numVerts, 0);
}

int numFilterTargets;
void **filterTargets;

bool IsFilterActive(void *target)
{
    for (int i = 0; i < numFilterTargets; i++)
    {
        if (filterTargets[i] == target)
        {
            return true;
        }
    }
    return false;
}

void SetFilterActive(void *target)
{
    if (!IsFilterActive(target))
    {
        filterTargets = (void **)RESIZE(filterTargets, numFilterTargets + 1)
            filterTargets[numFilterTargets++] = target;
    }
}

void UnsetFilterActive(void *target)
{
    for (int i = 0; i < numFilterTargets; i++)
    {
        if (filterTargets[i] == target)
        {
            filterTargets[i] = filterTargets[--numFilterTargets];
            filterTargets = (void **)RESIZE(filterTargets, numFilterTargets) return;
        }
    }
}

typedef struct rectVec
{
    struct tagRECT *start;
    struct tagRECT *end;
    struct tagRECT *cap;
} rectVec;

typedef long(COverlayContext_Present_t)(void *, void *, unsigned int, rectVec *, unsigned int, bool);

COverlayContext_Present_t *COverlayContext_Present_orig;
COverlayContext_Present_t *COverlayContext_Present_real_orig;

long COverlayContext_Present_hook(void *self, void *overlaySwapChain, unsigned int a3, rectVec *rectVec,
                                  unsigned int a5, bool a6)
{
    if (_ReturnAddress() < (void *)COverlayContext_Present_real_orig)
    {
        LOG_ONLY_ONCE("I am inside COverlayContext::Present hook inside the main if condition")

        if (isWindows11 && *((bool *)overlaySwapChain + IOverlaySwapChain_HardwareProtected_offset_w11) ||
            !isWindows11 && *((bool *)overlaySwapChain + IOverlaySwapChain_HardwareProtected_offset))
        {
            std::stringstream hw_protection_message;
            hw_protection_message << "I'm inside the Hardware protection condition - 0x" << std::hex << (bool *)overlaySwapChain + IOverlaySwapChain_HardwareProtected_offset_w11 << " - value: 0x" << *((bool *)overlaySwapChain + IOverlaySwapChain_HardwareProtected_offset_w11);
            LOG_ONLY_ONCE(hw_protection_message.str().c_str())
            UnsetFilterActive(self);
        }
        else
        {
            std::stringstream hw_protection_message;
            hw_protection_message << "I'm outside the Hardware protection condition - 0x" << std::hex << (bool *)overlaySwapChain + IOverlaySwapChain_HardwareProtected_offset_w11 << " - value: 0x" << *((bool *)overlaySwapChain + IOverlaySwapChain_HardwareProtected_offset_w11);
            LOG_ONLY_ONCE(hw_protection_message.str().c_str())

            IDXGISwapChain *swapChain;
            if (isWindows11)
            {
                LOG_ONLY_ONCE("Gathering IDXGISwapChain pointer")
                int sub_from_legacy_swapchain = *(int *)((unsigned char *)overlaySwapChain - 4);
                void *real_overlay_swap_chain = (unsigned char *)overlaySwapChain - sub_from_legacy_swapchain -
                                                0x1b0;
                swapChain = *(IDXGISwapChain **)((unsigned char *)real_overlay_swap_chain +
                                                 IOverlaySwapChain_IDXGISwapChain_offset_w11);
            }
            else
            {
                swapChain = *(IDXGISwapChain **)((unsigned char *)overlaySwapChain +
                                                 IOverlaySwapChain_IDXGISwapChain_offset);
            }

            // if (ApplyFilter(self, swapChain, rectVec->start, rectVec->end - rectVec->start))
            // {
            //     LOG_ONLY_ONCE("Setting LUTactive")
            //     SetFilterActive(self);
            // }
            // else
            // {
            //     LOG_ONLY_ONCE("Un-setting LUTactive")
            //     UnsetFilterActive(self);
            // }
        }
    }

    return COverlayContext_Present_orig(self, overlaySwapChain, a3, rectVec, a5, a6);
}

typedef bool(COverlayContext_IsCandidateDirectFlipCompatbile_t)(void *, void *, void *, void *, int, unsigned int, bool,
                                                                bool);

COverlayContext_IsCandidateDirectFlipCompatbile_t *COverlayContext_IsCandidateDirectFlipCompatbile_orig;

bool COverlayContext_IsCandidateDirectFlipCompatbile_hook(void *self, void *a2, void *a3, void *a4, int a5,
                                                          unsigned int a6, bool a7, bool a8)
{
    if (IsFilterActive(self))
    {
        return false;
    }
    return COverlayContext_IsCandidateDirectFlipCompatbile_orig(self, a2, a3, a4, a5, a6, a7, a8);
}

typedef bool(COverlayContext_OverlaysEnabled_t)(void *);

COverlayContext_OverlaysEnabled_t *COverlayContext_OverlaysEnabled_orig;

bool COverlayContext_OverlaysEnabled_hook(void *self)
{
    if (IsFilterActive(self))
    {
        return false;
    }
    return COverlayContext_OverlaysEnabled_orig(self);
}

int main()
{
    HMODULE dwmcore = GetModuleHandleW(L"dwmcore.dll");
    MODULEINFO moduleInfo;
    GetModuleInformation(GetCurrentProcess(), dwmcore, &moduleInfo, sizeof moduleInfo);

    OSVERSIONINFOEX versionInfo;
    ZeroMemory(&versionInfo, sizeof(OSVERSIONINFOEX));
    versionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
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
            unsigned char *address = (unsigned char *)dwmcore + i;
            if (!COverlayContext_Present_orig && sizeof COverlayContext_Present_bytes_w11 <= moduleInfo.SizeOfImage - i && !aob_match_inverse(address, COverlayContext_Present_bytes_w11, sizeof COverlayContext_Present_bytes_w11))
            {
                COverlayContext_Present_orig = (COverlayContext_Present_t *)address;
                COverlayContext_Present_real_orig = COverlayContext_Present_orig;
            }
            else if (!COverlayContext_IsCandidateDirectFlipCompatbile_orig && sizeof COverlayContext_IsCandidateDirectFlipCompatbile_bytes_w11 <= moduleInfo.SizeOfImage - i && !aob_match_inverse(address, COverlayContext_IsCandidateDirectFlipCompatbile_bytes_w11, sizeof COverlayContext_IsCandidateDirectFlipCompatbile_bytes_w11))
            {
                COverlayContext_IsCandidateDirectFlipCompatbile_orig = (COverlayContext_IsCandidateDirectFlipCompatbile_t *)address;
            }
            else if (!COverlayContext_OverlaysEnabled_orig && sizeof COverlayContext_OverlaysEnabled_bytes_w11 <= moduleInfo.SizeOfImage - i && !aob_match_inverse(address, COverlayContext_OverlaysEnabled_bytes_w11, sizeof COverlayContext_OverlaysEnabled_bytes_w11))
            {
                COverlayContext_OverlaysEnabled_orig = (COverlayContext_OverlaysEnabled_t *)address;
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
            unsigned char *address = (unsigned char *)dwmcore + i;
            if (!COverlayContext_Present_orig && !memcmp(address, COverlayContext_Present_bytes,
                                                         sizeof(COverlayContext_Present_bytes)))
            {
                COverlayContext_Present_orig = (COverlayContext_Present_t *)address;
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
                    COverlayContext_IsCandidateDirectFlipCompatbile_orig = (COverlayContext_IsCandidateDirectFlipCompatbile_t *)(address - 0xa);
                }
            }
            else if (!COverlayContext_OverlaysEnabled_orig && !memcmp(
                                                                  address, COverlayContext_OverlaysEnabled_bytes, sizeof(COverlayContext_OverlaysEnabled_bytes)))
            {
                COverlayContext_OverlaysEnabled_orig = (COverlayContext_OverlaysEnabled_t *)(address - 0x7);
            }
            if (COverlayContext_Present_orig && COverlayContext_IsCandidateDirectFlipCompatbile_orig &&
                COverlayContext_OverlaysEnabled_orig)
            {
                break;
            }
        }
    }

    if (COverlayContext_Present_orig && COverlayContext_IsCandidateDirectFlipCompatbile_orig &&
        COverlayContext_OverlaysEnabled_orig)
    {
        MH_Initialize();
        MH_CreateHook((PVOID)COverlayContext_Present_orig, (PVOID)COverlayContext_Present_hook,
                      (PVOID *)&COverlayContext_Present_orig);
        MH_CreateHook((PVOID)COverlayContext_IsCandidateDirectFlipCompatbile_orig,
                      (PVOID)COverlayContext_IsCandidateDirectFlipCompatbile_hook,
                      (PVOID *)&COverlayContext_IsCandidateDirectFlipCompatbile_orig);
        MH_CreateHook((PVOID)COverlayContext_OverlaysEnabled_orig, (PVOID)COverlayContext_OverlaysEnabled_hook,
                      (PVOID *)&COverlayContext_OverlaysEnabled_orig);
        MH_EnableHook(MH_ALL_HOOKS);
        MESSAGE_BOX_DBG("DWM HOOK INITIALIZATION", MB_OK)
    }

    std::cout << "fin";
    return 0;
}
