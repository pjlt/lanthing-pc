#include <atomic>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include <initguid.h>

#include <d3d11.h>
#include <d3d11_1.h>
#include <dxgi1_5.h>

#include "ltlib/times.h"
#include "types.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext_d3d11va.h>
}

namespace lt {

class D3D11Pipeline {
public:
    D3D11Pipeline();

    ~D3D11Pipeline();

    // 在指定的adapter上初始化D3D
    bool init(uint64_t luid);

    bool setupRender(HWND hwnd, uint32_t width, uint32_t height);

    bool waitForPipeline(int64_t max_wait_ms);

    bool setupDecoder(Format format);

    int64_t decode(const uint8_t* data, uint32_t size);

    bool render(int64_t resouce);

private:
    bool setupIAAndVSStage();
    bool setupRSStage();
    bool setupPSStage();
    bool setupOMStage();

    bool checkDecoder();

private:
    struct Frame {
        int64_t id = 0;
        AVPacket* pkt = nullptr;
        AVFrame* frame = nullptr;

        size_t index = 0;
        ID3D11Texture2D* texture = nullptr;
    };

    bool initDecoderContext();
    bool initAVCodec(const AVCodec* decoder);
    bool initShaderResources(std::vector<ID3D11Texture2D*> textures);

    const Frame* get(int64_t frame_id);
    void erase(int64_t frame_id);

    void uninitDecoder();

private:
    static void d3d11LockContext(void* ctx);
    static void d3d11UnlockContext(void* ctx);
    static AVPixelFormat getFormat(AVCodecContext* context, const enum AVPixelFormat* pixFmts);

private:
    HWND hwnd_ = nullptr;
    int refresh_rate_ = 60;

    struct ShaderView {
        std::vector<ID3D11ShaderResourceView*> array;
    };

    ID3D11Device* d3d11_dev_ = nullptr;
    ID3D11DeviceContext* d3d11_ctx_ = nullptr;

    IDXGIFactory5* dxgi_factory_ = nullptr;
    IDXGISwapChain4* swap_chain_ = nullptr;
    HANDLE waitable_obj_ = NULL;
    bool pipeline_ready_ = false;
    ID3D11RenderTargetView* render_view_ = nullptr;
    std::vector<ShaderView> shader_views_;

    uint32_t display_width_ = 0;
    uint32_t display_height_ = 0;

    std::mutex pipeline_mtx_;

private:
    Format format_ = Format::UNSUPPORT;
    Codec codec_ = Codec::UNKNOWN;
    uint32_t video_width_ = 0;
    uint32_t video_height_ = 0;

    // for ffmpeg decoder
    AVCodecContext* avcodec_context_ = nullptr;
    const AVCodecHWConfig* avcodec_hwconfig_ = nullptr;

    AVBufferRef* hw_device_context_ = nullptr;
    AVBufferRef* hw_frames_context_ = nullptr;

    size_t av_pool_size_ = 10;

    std::mutex frames_mtx_;
    std::deque<Frame> frames_;

    int64_t id_counter_ = 0;

    std::map<int64_t, Frame> decoded_frames_;
};

} // namespace lt
