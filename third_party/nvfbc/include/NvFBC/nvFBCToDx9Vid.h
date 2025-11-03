/**
 * \file
 * This file contains definitions for NVFBCToDX9Vid interface
 *
 *
 * Copyright 1993-2018 NVIDIA Corporation.  All rights reserved.
 * NOTICE TO LICENSEE: This source code and/or documentation ("Licensed Deliverables")
 * are subject to the applicable NVIDIA license agreement
 * that governs the use of the Licensed Deliverables.
 *
 */

#ifndef NVFBC_TO_DX9_VID_H_
#define NVFBC_TO_DX9_VID_H_
/**
 * \defgroup NVFBC_TODX9VID NVFBCToDx9Vid Interface.
 * \brief Interface for grabbing Desktop images and copying output to D3D9 buffers provided by the client.
 */

/**
 * \defgroup NVFBC_TODX9VID_ENUMS Enums
 * \ingroup NVFBC_TODX9VID
 * \brief Enumerations defined for use with NVFBCToDX9Vid interface.
 */

/**
 * \defgroup NVFBC_TODX9VID_STRUCTS Structs
 * \ingroup NVFBC_TODX9VID
 * \brief Parameter Structs defined for use with NVFBCToDX9Vid interface.
 */

/**
 * \defgroup NVFBC_TODX9VID_INTERFACE Object Interface
 * \ingroup NVFBC_TODX9VID
 * \brief Interface class definition for NVFBCToDX9Vid capture API
 */

/**
 * \ingroup NVFBC_TODX9VID
 * \brief Macro to define the interface ID to be passed as NvFBCCreateParams::dwInterfaceType
 * for creating an NVFBCToDX9Vid capture session object.
 */
#define NVFBC_TO_DX9_VID (0x2003)

/**
 * \ingroup NVFBC_TODX9VID
 * \brief Macro to define the maximum size, in bytes, of diffmap buffer
 */
#define NVFBC_TODX9VID_MAX_DIFF_MAP_SIZE                         0x00040000                             

/**
 * \ingroup NVFBC_TODX9VID
 * \brief Macro to define the maximum size, in bytes, of Image Classification Map buffer
 */
 #define NVFBC_TODX9VID_MAX_CLASSIFICATION_MAP_SIZE               0X00040000

/**
 * \ingroup NVFBC_TODX9VID
 * \brief Macro to define the maximum dimensions supported for 
 *  Stamp shape used to aggregate high frequency content detection results
 *  for generating Image Classification Map
 */
 #define NVFBC_TODX9VID_MAX_CLASSIFICATION_MAP_STAMP_DIM          256
 
 /**
 * \ingroup NVFBC_TODX9VID
 * \brief Macro to define the minimum dimensions supported for 
 *  Stamp shape used to aggregate high frequency content detection results
 *  for generating Image Classification Map
 */
 #define NVFBC_TODX9VID_MIN_CLASSIFICATION_MAP_STAMP_DIM          16

/**
 * \ingroup NVFBC_TODX9VID_ENUMS
 * Enumerates Output pixel data formats supported by NVFBCToDX9Vid interface.
 */
typedef enum
{
    NVFBC_TODX9VID_ARGB       = 0,              /**< Output in 32-bit packed ARGB format. */
    NVFBC_TODX9VID_NV12          ,              /**< Output in Nv12 format (YUV 4:2:0). */
    NVFBC_TODX9VID_ARGB10        ,              /**< Output in 32-bit packed ARGB10 format (A2B10G10R10). */
    NVFBC_TODX9VID_BUF_FMT_LAST  ,              /**< Sentinel value. Do not use. */
} NVFBCToDx9VidBufferFormat;

/**
 * \ingroup NVFBC_TODX9VID_ENUMS
 * Enumerates Capture\Grab modes supported by NVFBCToDX9Vid interface.
 */
typedef enum 
{
    NVFBC_TODX9VID_SOURCEMODE_FULL  = 0,        /**< Grab full res. */
    NVFBC_TODX9VID_SOURCEMODE_SCALE    ,        /**< Scale the grabbed image according to NVFBC_TODX9VID_GRAB_FRAME_PARAMS::dwTargetWidth
                                                     and NVFBC_TODX9VID_GRAB_FRAME_PARAMS::dwTargetHeight. */
    NVFBC_TODX9VID_SOURCEMODE_CROP     ,        /**< Crop a subwindow without scaling, of NVFBC_TODX9VID_GRAB_FRAME_PARAMS::dwTargetWidth
                                                     and NVFBC_TODX9VID_GRAB_FRAME_PARAMS::dwTargetHeight sizes, 
                                                     starting at NVFBC_TODX9VID_GRAB_FRAME_PARAMS::dwStartX and NVFBC_TODX9VID_GRAB_FRAME_PARAMS::dwStartY. */
    NVFBC_TODX9VID_SOURCEMODE_LAST     ,        /**< Sentinel value. Do not use. */
}NVFBCToDx9VidGrabMode;

/**
 * \ingroup NVFBC_TODX9VID_ENUMS
 * Enumerates special commands for grab\capture supported by NVFBCToDX9Vid interface.
 */
typedef enum 
{
    NVFBC_TODX9VID_NOFLAGS              = 0x0,               /**< Default (no flags set). Grabbing will wait for a new frame or HW mouse move. */
    NVFBC_TODX9VID_NOWAIT               = 0x1,               /**< Grabbing will not wait for a new frame nor a HW cursor move. */
    NVFBC_TODX9VID_WAIT_WITH_TIMEOUT    = 0x10,              /**< Grabbing will wait for a new frame or HW mouse move with a maximum wait time of NVFBC_TODX9VID_GRAB_FRAME_PARAMS::dwWaitTime millisecond*/
} NVFBCToDx9VidGrabFlags;

/**
 * \ingroup NVFBC_TODX9VID_ENUMS
 * \typedef NVFBC_TODX9VID_GRAB_FLAGS
 * \brief \ref NVFBCToDx9VidGrabFlags.
 */
typedef NVFBCToDx9VidGrabFlags NVFBC_TODX9VID_GRAB_FLAGS;

/**
 * \ingroup NVFBC_TODX9VID_ENUMS
 * Enumerates stereo grab\capture formats. Currently unsupported.
 */
typedef enum
{
    NVFBC_TODX9VID_STEREOFMT_PACKED_LR = 0,       /**< Default, Stereo captured views will be packed side-by-side. */
    NVFBC_TODX9VID_STEREOFMT_PACKED_TB,           /**< Stereo captured captured views will be packed top-bottom. */
    NVFBC_TODX9VID_STEREOFMT_SEPARATE_VIEWS,      /**< Stereo captured views will be available in separate buffers. */
} NVFBCToDx9VidStereoFmt;

/**
 * \ingroup NVFBC_TODX9VID_STRUCTS
 * \brief Defines an output buffer for use with NVFBCToDX9Vid interface.
 */
typedef struct
{
    IDirect3DSurface9* pPrimary;                /**< [in]: Contains the grabbed desktop image. */
    IDirect3DSurface9* pSecondary;              /**< [in]: Reserved: Not to be used. */
} NVFBC_TODX9VID_OUT_BUF;

/**
 * \ingroup NVFBC_TODX9VID_ENUMS
 * \brief Defines list of block sizes (in pixels) supported for NVFBCToDX9Vid interface 
 */
typedef enum
{
    NVFBC_DX9VID_DIFFMAP_BLOCKSIZE_128X128 = 0,    /**< Default 128 pixel x 128 pixel blocksize, same as legacy blocksize for backward compatibility */
    NVFBC_DX9VID_DIFFMAP_BLOCKSIZE_16X16,          /**< 16 pixel x 16 pixel blocksize */ 
    NVFBC_DX9VID_DIFFMAP_BLOCKSIZE_32X32,          /**< 32 pixel x 32 pixel blocksize */
    NVFBC_DX9VID_DIFFMAP_BLOCKSIZE_64X64,          /**< 64 pixel x 64 pixel blocksize */
    NVFBC_TODX9VID_DIFFMAP_BLOCKSIZE_128X128 = NVFBC_DX9VID_DIFFMAP_BLOCKSIZE_128X128,  /**< Alias for consistant naming convention */
    NVFBC_TODX9VID_DIFFMAP_BLOCKSIZE_16X16 = NVFBC_DX9VID_DIFFMAP_BLOCKSIZE_16X16,      /**< Alias for consistant naming convention */
    NVFBC_TODX9VID_DIFFMAP_BLOCKSIZE_32X32 = NVFBC_DX9VID_DIFFMAP_BLOCKSIZE_32X32,      /**< Alias for consistant naming convention */
    NVFBC_TODX9VID_DIFFMAP_BLOCKSIZE_64X64 = NVFBC_DX9VID_DIFFMAP_BLOCKSIZE_64X64       /**< Alias for consistant naming convention */
} NVFBC_DX9VID_DIFFMAP_BLOCKSIZE;
typedef NVFBC_DX9VID_DIFFMAP_BLOCKSIZE NVFBC_TODX9VID_DIFFMAP_BLOCKSIZE;

/**
 * \ingroup NVFBC_TODX9VID_STRUCTS
 * \brief Defines parameters used to configure NVFBCToDX9Vid capture session.
 */
typedef struct
{
    NvU32 dwVersion;                                    /**< [in]: Struct version. Set to NVFBC_TODX9VID_SETUP_PARAMS_VER.*/
    NvU32 bWithHWCursor         :1;                     /**< [in]: The client should set this to 1 if it requires the HW cursor to be composited on the captured image.*/
    NvU32 bStereoGrab           :1;                     /**< [in]: Reserved. Not to be used. */
    NvU32 bDiffMap              :1;                     /**< [in]: The client should set this to use the DiffMap feature.*/
    NvU32 bEnableSeparateCursorCapture : 1;             /**< [in]: The client should set this to 1 if it wants to enable mouse capture separately from Grab().*/
    NvU32 bHDRRequest           :1;                     /**< [in]: The client should set this to 1 if it wants to request HDR capture.*/
    NvU32 bClassificationMap    :1;                     /**< [in]: The client should set this to use the High Frequency Content Detection feature.*/
    NvU32 bReservedBits         :26;                    /**< [in]: Reserved. Set to 0.*/
    NVFBCToDx9VidBufferFormat eMode;                    /**< [in]: Output image format.*/
    NvU32 dwNumBuffers;                                 /**< [in]: Number of NVFBC_TODX9VID_OUT_BUF allocated by the client*/
    NVFBC_TODX9VID_DIFFMAP_BLOCKSIZE eDiffMapBlockSize; /**< [in]: Valid only if bDiffMap is set. 
                                                                   Set this bit field using NVFBC_TODX9VID_DIFFMAP_BLOCKSIZE enum. 
                                                                   Default is 128x128 Diffmap.*/
    NVFBCToDx9VidStereoFmt eStereoFmt;                  /**< [in]: Reserved: Not to be used.*/
    NvU32 dwDiffMapBuffSize;                            /**< [in]: Size of diffMap buffer allocated by client. 
                                                                   Use NVFBC_TODX9VID_MAX_DIFF_MAP_SIZE for allocating the diffmap buffer for best results.*/
    NvU32 dwClassificationMapBuffSize;                  /**< [in]: Size of High Frequency Content Detection buffer allocated by client.*/
    NvU32 dwClassificationMapStampWidth;                /**< [in]: Stamp width for aggregating Image Classification results. 
                                                                   Must be multiple of NVFBC_TODX9VID_MIN_CLASSIFICATION_MAP_BLOCK_DIM 
                                                                   and less than NVFBC_TODX9VID_MAX_CLASSIFICATION_MAP_BLOCK_DIM */
    NvU32 dwClassificationMapStampHeight;               /**< [in]: Stamp height for aggregating Image Classification results.
                                                                   Must be multiple of NVFBC_TODX9VID_MIN_CLASSIFICATION_MAP_BLOCK_DIM 
                                                                   and less than NVFBC_TODX9VID_MAX_CLASSIFICATION_MAP_BLOCK_DIM */
    void **ppDiffMap;                                   /**< [in]: Array of diffmap buffers allocated by client. 
                                                                   Must be allocated using VirtualAlloc.
                                                                   The no. of buffers is same as specified by dwNumBuffers.*/
    void **ppClassificationMap;                         /**< [in]: Array of High Frequency Content Detection buffers allocated by client.
                                                                   Must be allocated using VirtualAlloc. 
                                                                   The no. of buffers is same as specified by dwNumBuffers.*/
    NVFBC_TODX9VID_OUT_BUF *ppBuffer;                   /**< [in/out]: Container to hold NvFBC output buffers.*/
    void *hCursorCaptureEvent;                          /**< [out]: Event handle to be signalled when there is an update to the HW cursor state. */
    NvU32 dwReserved[22];                               /**< [in]: Reserved. Set to 0.*/
    void *pReserved[12];                                /**< [in]: Reserved. Set to 0.*/
} NVFBC_TODX9VID_SETUP_PARAMS_V3;
#define NVFBC_TODX9VID_SETUP_PARAMS_V3_VER NVFBC_STRUCT_VERSION(NVFBC_TODX9VID_SETUP_PARAMS_V3, 3)
#define NVFBC_TODX9VID_SETUP_PARAMS_VER NVFBC_TODX9VID_SETUP_PARAMS_V3_VER

typedef NVFBC_TODX9VID_SETUP_PARAMS_V3 NVFBC_TODX9VID_SETUP_PARAMS;

/**
 * \ingroup NVFBC_TODX9VID_STRUCTS
 * \brief Defines parameters for a Grab\Capture call in the NVFBCToDX9Vid capture session.
 * Also holds information regarding the grabbed data.
 */
typedef struct
{
    NvU32 dwVersion;                            /**< [in]: Struct version. Set to NVFBC_TODX9VID_GRAB_FRAME_PARAMS_VER.*/
    NvU32 dwFlags;                              /**< [in]: Special grabbing requests. This should be a bit-mask of NVFBCToDx9VidGrabFlags values.*/
    NvU32 dwTargetWidth;                        /**< [in]: Target image width. For NVFBC_TODX9VID_SOURCEMODE_SCALE, NvFBC will scale the captured image to fit target width and height.
                                                           For NVFBC_TODX9VID_SOURCEMODE_CROP, NVFBC will use this to determine cropping rect. */
    NvU32 dwTargetHeight;                       /**< [in]: Target image height. For NVFBC_TODX9VID_SOURCEMODE_SCALE, NvFBC will scale the captured image to fit target width and height.
                                                           For NVFBC_TODX9VID_SOURCEMODE_CROP, NVFBC will use this to determine cropping rect. */
    NvU32 dwStartX;                             /**< [in]: x-coordinate of starting pixel for cropping. Used with NVFBC_TODX9VID_SOURCEMODE_CROP. */
    NvU32 dwStartY;                             /**< [in]: y-coordinate of starting pixel for cropping. Used with NVFBC_TODX9VID_SOURCEMODE_CROP. .*/
    NVFBCToDx9VidGrabMode eGMode;               /**< [in]: Frame grab mode.*/
    NvU32 dwBufferIdx;                          /**< [in]: Array index of the NVFBC_TODX9VID_OUT_BUF which should contain the grabbed image.*/
    NvFBCFrameGrabInfo *pNvFBCFrameGrabInfo;    /**< [in/out]: Frame grab information and feedback from NvFBC driver.*/
    NvU32 dwWaitTime;                           /**< [in]: Timeout limit  in millisecond to wait for  a new frame or HW cursor move. Used with NVFBC_TODX9VID_WAIT_WITH_TIMEOUT */
    NvU32 dwReserved[23];                       /**< [in]: Reserved. Set to 0.*/
    void *pReserved[15];                        /**< [in]: Reserved. Set to NULL.*/
} NVFBC_TODX9VID_GRAB_FRAME_PARAMS_V1;
#define NVFBC_TODX9VID_GRAB_FRAME_PARAMS_V1_VER NVFBC_STRUCT_VERSION(NVFBC_TODX9VID_GRAB_FRAME_PARAMS_V1, 1)
#define NVFBC_TODX9VID_GRAB_FRAME_PARAMS_VER NVFBC_TODX9VID_GRAB_FRAME_PARAMS_V1_VER

typedef NVFBC_TODX9VID_GRAB_FRAME_PARAMS_V1 NVFBC_TODX9VID_GRAB_FRAME_PARAMS;

/**
 * \ingroup NVFBC_TODX9VID_INTERFACE
 * \brief Interface class definition for NVFBCToDX9Vid capture API
 */
class INvFBCToDx9Vid_v3
{
public:
    /**
     * \brief Set up NVFBC D3D9 capture according to the provided parameters.
     *  Registers the output buffers created and passed by the client for use with NVFBCToDx9Vid.
     * \param [in] pParam Pointer to a struct of type NVFBC_TODX9VID_SETUP_PARAMS
     * \return An applicable ::NVFBCRESULT value.
     */
    virtual NVFBCRESULT NVFBCAPI NvFBCToDx9VidSetUp              (NVFBC_TODX9VID_SETUP_PARAMS *pParam) = 0;

     /**
     * \brief Captures the desktop and copies the captured data to a D3D9 buffer that is managed by the client.
     *  If the API returns a failure, the client should check the return codes and ::NvFBCFrameGrabInfo output fields to determine if the session needs to be re-created.
     * \param [in] pParam Pointer to a struct of type NVFBC_TODX9VID_GRAB_FRAME_PARAMS
     * \return An applicable ::NVFBCRESULT value.
     */
    virtual NVFBCRESULT NVFBCAPI NvFBCToDx9VidGrabFrame          (NVFBC_TODX9VID_GRAB_FRAME_PARAMS *pParam) = 0;

    /**
     * \brief A high precision implementation of Sleep().
     * Can provide sub quantum (usually 16ms) sleep that does not burn CPU cycles.
     * param [in] qwMicroSeconds The number of microseconds that the thread should sleep for.
     * \return An applicable ::NVFBCRESULT value.
     */
    virtual NVFBCRESULT NVFBCAPI NvFBCToDx9VidGPUBasedCPUSleep   (__int64 qwMicroSeconds) = 0;

    /**
     * \brief Destroys the NVFBCToDX9Vid capture session.
     * \return An applicable ::NVFBCRESULT value.
     */
    virtual NVFBCRESULT NVFBCAPI NvFBCToDx9VidRelease() = 0;

    /**
    * \brief Captures HW cursor data whenever shape of mouse is changed
    * \param [inout] pParam Pointer to a struct of type ::NVFBC_TOSYS_GRAB_MOUSE_PARAMS.
    * \return An applicable ::NVFBCRESULT value.
    */
    virtual NVFBCRESULT NVFBCAPI NvFBCToDx9VidCursorCapture(NVFBC_CURSOR_CAPTURE_PARAMS *pParam) = 0;
};

typedef INvFBCToDx9Vid_v3 NvFBCToDx9Vid;

#endif // NVFBC_TO_DX9_VID_H_
