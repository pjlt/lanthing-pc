/**
 * \file This file contains definitions for NVFBCCuda.
 *
 * Copyright 1993-2018 NVIDIA Corporation.  All rights reserved.
 * NOTICE TO LICENSEE: This source code and/or documentation ("Licensed Deliverables")
 * are subject to the applicable NVIDIA license agreement
 * that governs the use of the Licensed Deliverables.
 *
 */

#ifndef _NVFBC_CUDA_H_
#define _NVFBC_CUDA_H_

/**
 * \defgroup NVFBC_CUDA NVFBCCuda Interface.
 * \brief Interface for grabbing Desktop images and dumping to a CUDA buffer.
 */

/**
 * \defgroup NVFBC_CUDA_ENUMS Enums
 * \ingroup NVFBC_CUDA
 * \brief Enumerations to be used with the NVFBCCuda interface.
 */

/**
 * \defgroup NVFBC_CUDA_STRUCTS Structs
 * \ingroup NVFBC_CUDA
 * \brief Parameter Structures to be used with the NvFBCCuda interface.
 */

/**
 * \defgroup NVFBC_CUDA_INTERFACE Object Definition
 * \ingroup NVFBC_CUDA
 * \brief Interface class definition for the NVFBCCuda API.
 */

/** 
 * \ingroup NVFBC_CUDA
 * \brief Macro to define the interface ID to be passed as NvFBCCreateParams::dwInterfaceType for generating output in 24-bit RGB format.
 */
#define NVFBC_SHARED_CUDA (0x1007)

/**
 * \ingroup NVFBC_CUDA_ENUMS
 * \brief Enumerates special commands for grab\capture supported by the NVFBCCuda interface.
 */
typedef enum 
{
    NVFBC_TOCUDA_NOFLAGS           =    0x0,       /**< Default (no flags set). Grabbing will wait for a new frame or HW mouse move. */
    NVFBC_TOCUDA_NOWAIT            =    0x1,       /**< Grabbing will not wait for a new frame nor a HW cursor move. */
    NVFBC_TOCUDA_CPU_SYNC          =    0x2,       /**< Does a cpu event signal when grab is complete */
    NVFBC_TOCUDA_WITH_HWCURSOR     =    0x4,       /**< Grabs the HW cursor if any visible */
    NVFBC_TOCUDA_RESERVED_A        =    0x8,       /**< reserved */
    NVFBC_TOCUDA_WAIT_WITH_TIMEOUT =    0x10,      /**< Grabbing will wait for a new frame or HW mouse move with a maximum wait time of NVFBC_CUDA_GRAB_FRAME_PARAMS::dwWaitTime millisecond*/
} NVFBC_CUDA_FLAGS;

typedef enum
{
    NVFBC_TOCUDA_ARGB      = 0,              /**< Output in 32-bit packed ARGB format. */
    NVFBC_TOCUDA_ARGB10       ,              /**< Output in 32-bit packed ARGB10 format (A2B10G10R10). */
    NVFBC_TOCUDA_BUF_FMT_LAST ,              /**< Sentinel value. Do not use. */
} NVFBCToCUDABufferFormat;


/**
* \ingroup NVFBC_CUDA_STRUCTS
* \brief Defines parameters for a Grab\Capture call in the NVFBCCuda capture session.
*  Also holds information regarding the grabbed data.
*/
typedef struct
{
    NvU32 dwVersion;                           /**< [in]: Struct version. Set to NVFBC_CUDA_GRAB_FRAME_PARAMS_V1_VER. */
    NvU32 dwFlags;                             /**< [in]: Flags for grab frame.*/
    void *pCUDADeviceBuffer;                   /**< [in]: Output buffer.*/
    NvFBCFrameGrabInfo *pNvFBCFrameGrabInfo;   /**< [in/out]: Frame grab configuration and feedback from NvFBC driver.*/
    NvU32 dwWaitTime;                          /**< [in]: Time limit in millisecond to wait for a new frame or HW mouse move. Use with NVFBC_TOCUDA_WAIT_WITH_TIMEOUT  */
    NvU32 dwReserved[61];                      /**< [in]: Reserved. Set to 0.*/
    void *pReserved[30];                       /**< [in]: Reserved. Set to NULL.*/
} NVFBC_CUDA_GRAB_FRAME_PARAMS_V1;
#define NVFBC_CUDA_GRAB_FRAME_PARAMS_V1_VER NVFBC_STRUCT_VERSION(NVFBC_CUDA_GRAB_FRAME_PARAMS_V1, 1)
#define NVFBC_CUDA_GRAB_FRAME_PARAMS_VER NVFBC_CUDA_GRAB_FRAME_PARAMS_V1_VER
typedef NVFBC_CUDA_GRAB_FRAME_PARAMS_V1 NVFBC_CUDA_GRAB_FRAME_PARAMS;

/**
 * \ingroup NVFBC_CUDA_STRUCTS
 * \brief Defines parameters for NvFBCCudaSetup() call
 */
typedef struct _NVFBC_CUDA_SETUP_PARAMS_V1
{
    NvU32 dwVersion;                            /**< [in]: Struct version. Set to NVFBC_CUDA_SETUP_PARMS_VER. */
    NvU32 bEnableSeparateCursorCapture : 1;     /**< [in]: The client should set this to 1 if it wants to enable mouse capture separately from Grab(). */
    NvU32 bHDRRequest : 1;                      /**< [in]: The client should set this to 1 if it wants to request HDR capture.*/
    NvU32 bReserved : 30;                       /**< [in]: Reserved. Seto to 0. */
    void *hCursorCaptureEvent;                  /**< [out]: Event handle to be signalled when there is an update to the HW cursor state. */
    NVFBCToCUDABufferFormat eFormat;            /**< [in]: Output image format.*/
    NvU32 dwReserved[61];                       /**< [in]: Reserved. Set to 0. */
    void *pReserved[31];                        /**< [in]: Reserved. Set to NULL. */
} NVFBC_CUDA_SETUP_PARAMS_V1;
#define NVFBC_CUDA_SETUP_PARAMS_V1_VER NVFBC_STRUCT_VERSION(NVFBC_CUDA_SETUP_PARAMS_V1, 1)
typedef NVFBC_CUDA_SETUP_PARAMS_V1 NVFBC_CUDA_SETUP_PARAMS;
#define NVFBC_CUDA_SETUP_PARAMS_VER NVFBC_CUDA_SETUP_PARAMS_V1_VER

/**
 * \ingroup NVFBC_CUDA_INTERFACE
 * Interface class definition for the NVFBCCuda API.
 */
class INvFBCCuda_v3
{
public:
    /**
     * \brief Returns the maximum buffer size, in bytes for allocating a CUDA buffer to hold output data generated by the NvFBCCuda interface.
     * \param [out] pdwMaxBufSize Pointer to a 32-bit unsigned integer.
     * \return An applicable ::NVFBCRESULT value.
     */
    virtual NVFBCRESULT NVFBCAPI NvFBCCudaGetMaxBufferSize (NvU32 *pdwMaxBufSize) = 0;
    
    /**
     * \brief Performs initial setup
     * \param [in] pParams Pointer to a struct of type ::NVFBC_CUDA_SETUP_PARAMS.
     * \return An applicable ::NVFBCRESULT value.
     */
    virtual NVFBCRESULT NVFBCAPI NvFBCCudaSetup (NVFBC_CUDA_SETUP_PARAMS *pParams) = 0;

    /**
     * \brief Captures the desktop and dumps captured data to a CUDA buffer provided by the client.
     *  If the API returns a failure, the client should check the return codes and ::NvFBCFrameGrabInfo output fields to determine if the session needs to be re-created.
     * \param [inout] pParams Pointer to a struct of type ::NVFBC_CUDA_GRAB_FRAME_PARAMS.
     * \return An applicable ::NVFBCRESULT value.
     */
    virtual NVFBCRESULT NVFBCAPI NvFBCCudaGrabFrame (NVFBC_CUDA_GRAB_FRAME_PARAMS *pParams) = 0;
      
    /**
     * \brief A high precision implementation of Sleep(). 
     *  Can provide sub quantum (usually 16ms) sleep that does not burn CPU cycles.
     * \param qwMicroSeconds The number of microseconds that the thread should sleep for.
     * \return An applicable ::NVFBCRESULT value.
     */
    virtual NVFBCRESULT NVFBCAPI NvFBCCudaGPUBasedCPUSleep (__int64 qwMicroSeconds) = 0;

    /**
    * \brief Captures HW cursor data whenever shape of mouse is changed
    * \param [inout] pParam Pointer to a struct of type ::NVFBC_TOSYS_GRAB_MOUSE_PARAMS.
    * \return An applicable ::NVFBCRESULT value.
    */
    virtual NVFBCRESULT NVFBCAPI NvFBCCudaCursorCapture (NVFBC_CURSOR_CAPTURE_PARAMS *pParam) = 0;

    /**
     * \brief Destroys the NvFBCCuda capture session.
     * \return An applicable ::NVFBCRESULT value.
     */
    virtual NVFBCRESULT NVFBCAPI NvFBCCudaRelease () = 0;
};

typedef INvFBCCuda_v3 NvFBCCuda;

#endif _NVFBC_CUDA_H_
