#pragma once

#define WINDOWS_LEAN_AND_MEAN
#include <windows.h>

#include "NvFBC/nvFBC.h"
#include <string>

#define NVFBC64_LIBRARY_NAME "NvFBC64.dll"
#define NVFBC_LIBRARY_NAME "NvFBC.dll"

// Wraps loading and using NvFBC
class NvFBCLibrary
{
    NvFBCLibrary(const NvFBCLibrary &);
    NvFBCLibrary &operator=(const NvFBCLibrary &);

public:
    NvFBCLibrary()
        : m_handle(NULL)
        , pfn_get_status(NULL)
        , pfn_create(NULL)
        , pfn_enable(NULL)
    {}

    ~NvFBCLibrary()
    {
        if(NULL != m_handle)
            close();
    }

    // Attempts to load NvFBC from system directory.
    // on 32-bit OS: looks for NvFBC.dll in system32
    // for 32-bit app on 64-bit OS: looks for NvFBC.dll in syswow64
    // for 64-bit app on 64-bit OS: looks for NvFBC64.dll in system32
    bool load(std::string fileName = std::string())
    {
        if(NULL != m_handle)
            return true;

        if(!fileName.empty())
            m_handle = ::LoadLibraryA(fileName.c_str());

        if(NULL == m_handle)
        {
            m_handle = ::LoadLibraryA(getDefaultPath().c_str());
        }

        if(NULL == m_handle)
        {
            fprintf(stderr, "Unable to load NvFBC.\n");
            return false;
        }

        // Load the three functions exported by NvFBC
        pfn_create = (NvFBC_CreateFunctionExType)::GetProcAddress(m_handle, "NvFBC_CreateEx");
        pfn_set_global_flags = (NvFBC_SetGlobalFlagsType)::GetProcAddress(m_handle, "NvFBC_SetGlobalFlags");
        pfn_get_status = (NvFBC_GetStatusExFunctionType)::GetProcAddress(m_handle, "NvFBC_GetStatusEx");
        pfn_enable = (NvFBC_EnableFunctionType)::GetProcAddress(m_handle,"NvFBC_Enable");

        if((NULL == pfn_create) || (NULL == pfn_set_global_flags) || (NULL == pfn_get_status) || (NULL == pfn_enable))
        {
            fprintf(stderr, "Unable to load the NvFBC function pointers.\n");
            close();

            return false;
        }

        return true;
    }

    // Close the NvFBC dll
    void close()
    {
        if(NULL != m_handle)
            FreeLibrary(m_handle);

        m_handle = NULL;
        pfn_create = NULL;
        pfn_get_status = NULL;
        pfn_enable  = NULL;
    }

    // Get the status for the provided adapter, if no adapter is 
    // provided the default adapter is used.
    NVFBCRESULT getStatus(NvFBCStatusEx *status)
    {
        return pfn_get_status((void*)status);
    }

    // Sets the global flags for the provided adapter, if 
    // no adapter is provided the default adapter is used
    void setGlobalFlags(DWORD flags, int adapter = 0)
    {
        setTargetAdapter(adapter);
        pfn_set_global_flags(flags);
    }

    // Creates an instance of the provided NvFBC type if possible
    NVFBCRESULT createEx(NvFBCCreateParams *pParams)
    {
        return pfn_create((void *)pParams);
    }
    // Creates an instance of the provided NvFBC type if possible.  
    void *create(DWORD type, DWORD *maxWidth, DWORD *maxHeight, int adapter = 0, void *devicePtr = NULL)
    {
        if(NULL == m_handle)
            return NULL;

        NVFBCRESULT res = NVFBC_SUCCESS;
        NvFBCStatusEx status = {0};
        status.dwVersion = NVFBC_STATUS_VER;
        status.dwAdapterIdx = adapter;
        res = getStatus(&status);

        if (res != NVFBC_SUCCESS)
        {
            fprintf(stderr, "NvFBC not supported on this device + driver.\r\n");
            return NULL;
        }

        // Check to see if the device and driver are supported
        if(!status.bIsCapturePossible)
        {
            fprintf(stderr, "Unsupported device or driver.\r\n");
            return NULL;
        }

        // Check to see if an instance can be created
        if(!status.bCanCreateNow)
        {
            fprintf(stderr, "Unable to create an instance of NvFBC.\r\n");
            return NULL;
        }

        NvFBCCreateParams createParams;
        memset(&createParams, 0, sizeof(createParams));
        createParams.dwVersion = NVFBC_CREATE_PARAMS_VER;
        createParams.dwInterfaceType = type;
        createParams.pDevice = devicePtr;
        createParams.dwAdapterIdx = adapter;

        res = pfn_create(&createParams);
        
        *maxWidth = createParams.dwMaxDisplayWidth;
        *maxHeight = createParams.dwMaxDisplayHeight;
        
        return createParams.pNvFBC;
    }

    // enable/disable NVFBC
    void enable(NVFBC_STATE nvFBCState)
    {
        NVFBCRESULT res = NVFBC_SUCCESS;
        res = pfn_enable(nvFBCState);

        if (res != NVFBC_SUCCESS)
        {
            fprintf(stderr, "Failed to %s. Insufficient privilege\n", nvFBCState == 0?"disable":"enable");
            return;
        }
        else
        {
            fprintf(stdout, "NvFBC is %s\n", nvFBCState == 0 ? "disabled" : "enabled");
            return;
        }
    }

protected:
    // Get the default NvFBC library path
    typedef BOOL (WINAPI *pfnIsWow64Process) (HANDLE, PBOOL);
    pfnIsWow64Process fnIsWow64Process;

    BOOL IsWow64()
    {
        BOOL bIsWow64 = FALSE;

        fnIsWow64Process = (pfnIsWow64Process) GetProcAddress(
            GetModuleHandle(TEXT("kernel32.dll")),"IsWow64Process");
      
        if (NULL != fnIsWow64Process)
        {
            if (!fnIsWow64Process(GetCurrentProcess(),&bIsWow64))
            {
                bIsWow64 = false;
            }
        }
        return bIsWow64;
    }

    std::string getDefaultPath()
    {
        std::string defaultPath;

        size_t pathSize;
        char *libPath;

        if(0 != _dupenv_s(&libPath, &pathSize, "SystemRoot"))
        {
            fprintf(stderr, "Unable to get the SystemRoot environment variable\n");
            return defaultPath;
        }

        if(0 == pathSize)
        {
            fprintf(stderr, "The SystemRoot environment variable is not set\n");
            return defaultPath;
        }
#ifdef _WIN64
        defaultPath = std::string(libPath) + "\\System32\\" + NVFBC64_LIBRARY_NAME;
#else
        if (IsWow64())
        {
            defaultPath = std::string(libPath) + "\\Syswow64\\" + NVFBC_LIBRARY_NAME;
        }
        else
        {
            defaultPath = std::string(libPath) + "\\System32\\" + NVFBC_LIBRARY_NAME;            
        }
#endif
        return defaultPath;
    }

    void setTargetAdapter(int adapter = 0)
    {
        char targetAdapter[10] = {0};
        _snprintf_s(targetAdapter, 10, 9, "%d", adapter);
        SetEnvironmentVariableA("NVFBC_TARGET_ADAPTER", targetAdapter);
    }


protected:
    HMODULE m_handle;
    NvFBC_GetStatusExFunctionType pfn_get_status;
    NvFBC_SetGlobalFlagsType      pfn_set_global_flags;
    NvFBC_CreateFunctionExType    pfn_create;
    NvFBC_EnableFunctionType      pfn_enable;
};
