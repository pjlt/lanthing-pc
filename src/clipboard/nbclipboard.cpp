/*
 * Zhennan Tu all rights reserved
 */

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <future>
#include <mutex>
#include <optional>
#include <string>

#if defined(LT_WINDOWS)
#include <Windows.h>

#include <io.h>
#include <shellapi.h>
#include <shldisp.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <strsafe.h>
#include <wrl/client.h>
#endif // LT_WINDOWS

#include "nbclipboard.h"

namespace nbclip {

const WCHAR* const kLanthingClipboard = L"LanthingClipboard";
constexpr uint32_t kChunkSize = 8 * 1024;
constexpr uint32_t kMaxWindows = 8;
constexpr uint32_t kMsgPostTask = WM_USER + 200;
constexpr uint32_t kMsgStop = WM_USER + 201;

constexpr auto kDebug = NbClipLogLevel::Debug;
constexpr auto kInfo = NbClipLogLevel::Info;
constexpr auto kWarn = NbClipLogLevel::Warn;
constexpr auto kError = NbClipLogLevel::Error;

struct FileSendChunk {
    enum class State : uint32_t {
        Sent,
        Acked,
    };
    uint32_t seq;
    State state = State::Sent;
    int64_t timestamp;
    std::vector<uint8_t> data;
};

struct FileRecvChunk {
    uint32_t seq;
    std::vector<uint8_t> data;
};

class RemoteFileStream : public IStream {
public:
    RemoteFileStream(uint64_t file_size);

    virtual ~RemoteFileStream() {}

    void onRecvFileChunk(const uint8_t* data, uint16_t size);

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override;

    ULONG STDMETHODCALLTYPE AddRef() override { return ++refcount_; }

    ULONG STDMETHODCALLTYPE Release() override {
        int count = --refcount_;
        if (count == 0 && this != nullptr) {
            delete this;
        }
        return count;
    }

    virtual HRESULT STDMETHODCALLTYPE SetSize(ULARGE_INTEGER) { return E_NOTIMPL; }

    virtual HRESULT STDMETHODCALLTYPE CopyTo(IStream*, ULARGE_INTEGER, ULARGE_INTEGER*,
                                             ULARGE_INTEGER*) {
        return E_NOTIMPL;
    }

    virtual HRESULT STDMETHODCALLTYPE Commit(DWORD) { return E_NOTIMPL; }

    virtual HRESULT STDMETHODCALLTYPE Revert(void) { return E_NOTIMPL; }

    virtual HRESULT STDMETHODCALLTYPE LockRegion(ULARGE_INTEGER, ULARGE_INTEGER, DWORD) {
        return E_NOTIMPL;
    }

    virtual HRESULT STDMETHODCALLTYPE UnlockRegion(ULARGE_INTEGER, ULARGE_INTEGER, DWORD) {
        return E_NOTIMPL;
    }

    virtual HRESULT STDMETHODCALLTYPE Clone(IStream**) { return E_NOTIMPL; }

    virtual HRESULT STDMETHODCALLTYPE Read(void* pv, ULONG cb, ULONG* pcbRead);

    virtual HRESULT STDMETHODCALLTYPE Write(const void* pv, ULONG cb, ULONG* pcbWritten) {
        (void)pv;
        (void)cb;
        (void)pcbWritten;
        return S_OK;
    }

    virtual HRESULT STDMETHODCALLTYPE Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin,
                                           ULARGE_INTEGER* plibNewPosition);

    virtual HRESULT WINAPI Stat(STATSTG* pstatstg, DWORD grfStatFlag);

private:
    int32_t doRead(void* buff, uint32_t buff_size);

private:
    std::mutex mtx_;
    std::condition_variable cv_;
    std::deque<std::vector<uint8_t>> chunk_list_;
    std::atomic<int> refcount_{1};
    ULARGE_INTEGER file_size_{};
    ULARGE_INTEGER current_position_{};
};

RemoteFileStream::RemoteFileStream(uint64_t file_size) {
    file_size_.QuadPart = file_size;
    current_position_.QuadPart = 0;
}

void RemoteFileStream::onRecvFileChunk(const uint8_t* data, uint16_t size) {
    std::vector<uint8_t> tmp(data, data + size);
    {
        std::unique_lock<std::mutex> lock{mtx_};
        chunk_list_.push_back(std::move(tmp));
    }
    cv_.notify_one();
}

HRESULT STDMETHODCALLTYPE RemoteFileStream::QueryInterface(REFIID riid, void** ppvObject) {
    static const QITAB qit[] = {
        QITABENT(RemoteFileStream, ISequentialStream),
        QITABENT(RemoteFileStream, IStream),
        {0},
    };
    return QISearch(this, qit, riid, ppvObject);
}

HRESULT STDMETHODCALLTYPE RemoteFileStream::Read(void* pv, ULONG buff_size, ULONG* pcbRead) {
    // 似乎没法异步
    int64_t bytes_to_read = std::min(file_size_.QuadPart - current_position_.QuadPart,
                                     static_cast<uint64_t>(buff_size));
    uint64_t total_read = 0;
    while (bytes_to_read > 0) {
        int32_t has_read = doRead((char*)pv + total_read, static_cast<uint32_t>(bytes_to_read));
        if (has_read < 0) {
            if (pcbRead) {
                *pcbRead = static_cast<ULONG>(total_read);
            }
            return E_PENDING;
        }
        else if (has_read == 0) {
            if (pcbRead != nullptr) {
                *pcbRead = static_cast<ULONG>(total_read);
            }
            return S_OK;
        }
        else {
            total_read += has_read;
            bytes_to_read -= has_read;
        }
    }
    current_position_.QuadPart += total_read;
    if (pcbRead != nullptr) {
        *pcbRead = static_cast<ULONG>(total_read);
    }
    return S_OK;
}

int32_t RemoteFileStream::doRead(void* buff, uint32_t buff_size) {
    std::vector<uint8_t> chunk;
    {
        std::unique_lock lock{mtx_};
        cv_.wait_for(lock, std::chrono::seconds{5}, [this]() { return !chunk_list_.empty(); });
        if (chunk_list_.empty()) {
            return -1;
        }

        if (chunk_list_.front().size() <= buff_size) {
            chunk = std::move(chunk_list_.front());
            chunk_list_.pop_front();
        }
        else {
            chunk = std::vector<uint8_t>(chunk_list_.front().data(),
                                         chunk_list_.front().data() + buff_size);
            std::vector<uint8_t> new_front(chunk_list_.front().data() + buff_size,
                                           chunk_list_.front().data() + chunk_list_.front().size());
            chunk_list_.front() = new_front;
        }
    }
    if (chunk.empty()) {
        return 0;
    }
    memcpy(buff, chunk.data(), chunk.size());
    return static_cast<int32_t>(chunk.size());
}

HRESULT STDMETHODCALLTYPE RemoteFileStream::Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin,
                                                 ULARGE_INTEGER* plibNewPosition) {
    ULARGE_INTEGER new_pos = {0};

    switch (dwOrigin) {
    case STREAM_SEEK_SET:
        break;
    case STREAM_SEEK_CUR:
        new_pos = current_position_;
        break;
    case STREAM_SEEK_END:
        new_pos = file_size_;
        break;
    default:
        return STG_E_INVALIDFUNCTION;
    }

    new_pos.QuadPart += dlibMove.QuadPart;
    if (new_pos.QuadPart < 0 || new_pos.QuadPart > file_size_.QuadPart) {
        return STG_E_INVALIDFUNCTION;
    }

    if (plibNewPosition) {
        *plibNewPosition = new_pos;
    }

    current_position_ = new_pos;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE RemoteFileStream::Stat(STATSTG* pstatstg, DWORD grfStatFlag) {
    (void)grfStatFlag;
    memset(pstatstg, 0, sizeof(STATSTG));
    pstatstg->pwcsName = NULL;
    pstatstg->type = STGTY_STREAM;
    pstatstg->cbSize = file_size_;
    return S_OK;
}

class RemoteFile : public IDataObject, public IDataObjectAsyncCapability {
public:
    struct Params {
        int64_t device_id;
        uint32_t file_seq;
        std::string filename;
        std::wstring wfilename;
        uint64_t size;
        NbLogPrint log_print;
        std::function<void()> on_paste_start;
        std::function<void()> on_paste_stop;
        std::function<void(int64_t, uint32_t)> on_pull_file;
        std::function<void(int64_t, uint32_t, uint32_t)> on_file_chunk_ack;
    };

public:
    RemoteFile(const Params& params);
    virtual ~RemoteFile();
    void onRecvFileChunk(int64_t device_id, uint32_t file_seq, uint32_t chunk_seq,
                         const std::vector<uint8_t>& data);

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;

    // IDataObject
    HRESULT STDMETHODCALLTYPE GetData(FORMATETC* pformatetcIn, STGMEDIUM* pmedium) override;
    HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC* pformatetc, STGMEDIUM* pmedium) override;
    HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC* pformatetc) override;
    HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC* pformatectIn,
                                                    FORMATETC* pformatetcOut) override;
    HRESULT STDMETHODCALLTYPE SetData(FORMATETC* pformatetc, STGMEDIUM* pmedium,
                                      BOOL fRelease) override;
    HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD dwDirection,
                                            IEnumFORMATETC** ppenumFormatEtc) override;
    HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC* pformatetc, DWORD advf, IAdviseSink* pAdvSink,
                                      DWORD* pdwConnection) override;
    HRESULT STDMETHODCALLTYPE DUnadvise(DWORD dwConnection) override;
    HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA** ppenumAdvise) override;

    // IDataObjectAsyncCapability
    HRESULT STDMETHODCALLTYPE SetAsyncMode(BOOL fDoOpAsync) override;
    HRESULT STDMETHODCALLTYPE GetAsyncMode(BOOL* pfIsOpAsync) override;
    HRESULT STDMETHODCALLTYPE StartOperation(IBindCtx* pbcReserved) override;
    HRESULT STDMETHODCALLTYPE InOperation(BOOL* pfInAsyncOp) override;
    HRESULT STDMETHODCALLTYPE EndOperation(HRESULT hResult, IBindCtx* pbcReserved,
                                           DWORD dwEffects) override;

private:
    Params params_;
    Microsoft::WRL::ComPtr<IDataObject> dataobj_;
    std::atomic<int> refcount_{1};
    HRESULT hr_ = S_OK;
    uint16_t format_filedesc_ = 0;
    uint16_t format_filecontent_ = 0;
    Microsoft::WRL::ComPtr<RemoteFileStream> file_stream_;
    BOOL in_operation_ = FALSE;
    std::deque<FileRecvChunk> recv_buffers_;
    std::atomic<uint32_t> last_seq_{std::numeric_limits<decltype(last_seq_)>::max()};
};

RemoteFile::RemoteFile(const Params& params)
    : params_{params} {
    hr_ = SHCreateDataObject(NULL, 0, NULL, NULL, IID_PPV_ARGS(&dataobj_));
    format_filedesc_ = static_cast<uint16_t>(RegisterClipboardFormatW(CFSTR_FILEDESCRIPTOR));
    format_filecontent_ = static_cast<uint16_t>(RegisterClipboardFormatW(CFSTR_FILECONTENTS));
}

RemoteFile::~RemoteFile() {}

void RemoteFile::onRecvFileChunk(int64_t device_id, uint32_t file_seq, uint32_t chunk_seq,
                                 const std::vector<uint8_t>& data) {
    (void)device_id;
    // recv_buffers_只在整个函数使用，这个函数由库的使用者保证单线程调用，不用对recv_buffers_加锁
    if (file_seq != params_.file_seq) {
        return;
    }
    // TODO: 校验from_device_id
    // if (device_id != params_.device_id) {
    //     return;
    // }
    for (uint32_t i = last_seq_ + 1; i <= chunk_seq; i++) {
        FileRecvChunk chunk;
        chunk.seq = i;
        if (i == chunk_seq) {
            chunk.data = data;
        }
        if (recv_buffers_.empty()) {
            recv_buffers_.push_back(std::move(chunk));
        }
        else {
            bool found = false;
            for (auto& item : recv_buffers_) {
                if (item.seq == chunk.seq) {
                    found = true;
                    if (item.data.empty()) {
                        item = std::move(chunk);
                    }
                }
            }
            if (!found) {
                recv_buffers_.push_back(std::move(chunk));
            }
        }
    }
    while (!recv_buffers_.empty() && !recv_buffers_.front().data.empty()) {
        auto& chunk = recv_buffers_.front();
        last_seq_ = chunk.seq;
        if (file_stream_) {
            file_stream_->onRecvFileChunk(chunk.data.data(),
                                          static_cast<uint16_t>(chunk.data.size()));
        }
        params_.on_file_chunk_ack(params_.device_id, file_seq, chunk.seq);
        recv_buffers_.pop_front();
    }
}

HRESULT STDMETHODCALLTYPE RemoteFile::QueryInterface(REFIID riid, void** ppvObject) {
    static const QITAB qit[] = {
        QITABENT(RemoteFile, IDataObject),
        QITABENT(RemoteFile, IDataObjectAsyncCapability),
        {0},
    };
    return QISearch(this, qit, riid, ppvObject);
}

ULONG STDMETHODCALLTYPE RemoteFile::AddRef() {
    return ++refcount_;
}

ULONG STDMETHODCALLTYPE RemoteFile::Release() {
    int count = --refcount_;
    if (count == 0 && this != nullptr) {
        delete this;
    }
    return count;
}

//*************************************************//

HRESULT STDMETHODCALLTYPE RemoteFile::GetData(FORMATETC* pformatetcIn, STGMEDIUM* pmedium) {
    ZeroMemory(pmedium, sizeof(*pmedium));

    HRESULT hr = DATA_E_FORMATETC;
    if (pformatetcIn->cfFormat == format_filedesc_) {
        if (pformatetcIn->tymed & TYMED_HGLOBAL) {
            uint32_t cb = sizeof(FILEGROUPDESCRIPTORW);
            HGLOBAL h = GlobalAlloc(GHND | GMEM_SHARE, cb);
            if (!h) {
                hr = E_OUTOFMEMORY;
            }
            else {
                FILEGROUPDESCRIPTORW* pGroupDescriptor =
                    reinterpret_cast<FILEGROUPDESCRIPTORW*>(::GlobalLock(h));
                if (pGroupDescriptor) {
                    pGroupDescriptor->cItems = 1;
                    DWORD flags = FD_FILESIZE | FD_PROGRESSUI | FD_CREATETIME | FD_WRITESTIME;
                    wcsncpy_s(pGroupDescriptor->fgd[0].cFileName,
                              _countof(pGroupDescriptor->fgd[0].cFileName),
                              params_.wfilename.c_str(), _TRUNCATE);
                    pGroupDescriptor->fgd[0].dwFlags = flags;
                    pGroupDescriptor->fgd[0].nFileSizeLow =
                        static_cast<uint32_t>(params_.size & 0x0000'0000'ffff'ffff);
                    pGroupDescriptor->fgd[0].nFileSizeHigh =
                        static_cast<uint32_t>((params_.size & 0xffff'ffff'0000'0000) >> 32);
                    // pGroupDescriptor->fgd[0].dwFileAttributes; TODO: FILE ATTRIBUTES
                    SYSTEMTIME st;
                    FILETIME ft;
                    GetSystemTime(&st);
                    SystemTimeToFileTime(&st, &ft);
                    pGroupDescriptor->fgd[0].ftLastAccessTime = ft;
                    pGroupDescriptor->fgd[0].ftCreationTime = ft;
                    pGroupDescriptor->fgd[0].ftLastWriteTime = ft;
                    ::GlobalUnlock(h);
                    pmedium->hGlobal = h;
                    pmedium->tymed = TYMED_HGLOBAL;
                    hr = S_OK;
                }
            }
        }
    }
    else if (pformatetcIn->cfFormat == format_filecontent_) {
        if (kLanthingClipboard[0] != L'L' || kLanthingClipboard[1] != L'a' ||
            kLanthingClipboard[2] != L'n' || kLanthingClipboard[3] != L't' ||
            kLanthingClipboard[4] != L'h' || kLanthingClipboard[5] != L'i' ||
            kLanthingClipboard[6] != L'n' || kLanthingClipboard[7] != L'g') {
            return hr;
        }
        if ((pformatetcIn->tymed & TYMED_ISTREAM)) {
            if (pformatetcIn->lindex == 0 && file_stream_ == nullptr) {
                file_stream_ = new RemoteFileStream{params_.size};
                pmedium->pstm = file_stream_.Get();
                pmedium->pstm->AddRef();
                pmedium->tymed = TYMED_ISTREAM;
                last_seq_ = std::numeric_limits<uint32_t>::max();
                hr = S_OK;
                params_.on_pull_file(params_.device_id, params_.file_seq);
            }
        }
    }
    else if (dataobj_ != nullptr) {
        hr = dataobj_->GetData(pformatetcIn, pmedium);
    }
    return hr;
}

HRESULT STDMETHODCALLTYPE RemoteFile::GetDataHere(FORMATETC* pformatetc, STGMEDIUM* pmedium) {
    (void)pformatetc;
    (void)pmedium;
    return DATA_E_FORMATETC;
}

HRESULT STDMETHODCALLTYPE RemoteFile::QueryGetData(FORMATETC* pformatetc) {
    HRESULT hr = S_FALSE;
    if (pformatetc->cfFormat == format_filedesc_ || pformatetc->cfFormat == format_filecontent_) {
        hr = S_OK;
    }
    else if (dataobj_ != nullptr) {
        hr = dataobj_->QueryGetData(pformatetc);
    }
    return hr;
}

HRESULT STDMETHODCALLTYPE RemoteFile::GetCanonicalFormatEtc(FORMATETC* pformatectIn,
                                                            FORMATETC* pformatetcOut) {
    (void)pformatetcOut;
    (void)pformatectIn;
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE RemoteFile::SetData(FORMATETC* pformatetc, STGMEDIUM* pmedium,
                                              BOOL fRelease) {
    if (dataobj_ != nullptr) {
        return dataobj_->SetData(pformatetc, pmedium, fRelease);
    }
    else {
        return hr_;
    }
}

HRESULT STDMETHODCALLTYPE RemoteFile::EnumFormatEtc(DWORD dwDirection,
                                                    IEnumFORMATETC** ppenumFormatEtc) {
    *ppenumFormatEtc = NULL;
    HRESULT hr = E_NOTIMPL;
    if (dwDirection == DATADIR_GET) {
        FORMATETC rgfmtetc[] = {{format_filedesc_, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL},
                                {format_filecontent_, NULL, DVASPECT_CONTENT, -1, TYMED_ISTREAM}};
        hr = SHCreateStdEnumFmtEtc(ARRAYSIZE(rgfmtetc), rgfmtetc, ppenumFormatEtc);
    }
    CF_UNICODETEXT;
    return hr;
}

HRESULT STDMETHODCALLTYPE RemoteFile::DAdvise(FORMATETC* pformatetc, DWORD advf,
                                              IAdviseSink* pAdvSink, DWORD* pdwConnection) {
    (void)pformatetc;
    (void)advf;
    (void)pAdvSink;
    (void)pdwConnection;
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE RemoteFile::DUnadvise(DWORD dwConnection) {
    (void)dwConnection;
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE RemoteFile::EnumDAdvise(IEnumSTATDATA** ppenumAdvise) {
    (void)ppenumAdvise;
    return E_NOTIMPL;
}

//*************************************//

HRESULT STDMETHODCALLTYPE RemoteFile::SetAsyncMode(BOOL fDoOpAsync) {
    (void)fDoOpAsync;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE RemoteFile::GetAsyncMode(BOOL* pfIsOpAsync) {
    *pfIsOpAsync = TRUE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE RemoteFile::StartOperation(IBindCtx* pbcReserved) {
    (void)pbcReserved;
    // 总的粘贴开始了
    in_operation_ = TRUE;
    params_.on_paste_start();
    params_.log_print(kInfo, "Start copy '%s', size %u", params_.filename.c_str(), params_.size);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE RemoteFile::InOperation(BOOL* pfInAsyncOp) {
    *pfInAsyncOp = in_operation_;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE RemoteFile::EndOperation(HRESULT hResult, IBindCtx* pbcReserved,
                                                   DWORD dwEffects) {
    (void)hResult;
    (void)pbcReserved;
    (void)dwEffects;
    // 总的粘贴结束了
    in_operation_ = FALSE;
    params_.on_paste_stop();
    file_stream_ = nullptr;
    params_.log_print(kInfo, "Finished copy '%s'", params_.filename.c_str());
    return S_OK;
}

class LocalFile {
public:
    struct Params {
        NbLogPrint log_print;
        uint32_t file_seq;
        std::string fullpath;
        std::wstring wfullpath;
        uint64_t size;
        std::function<void(int64_t device_id, uint32_t file_seq, uint32_t chunk_seq,
                           const uint8_t* data, uint16_t size)>
            send_file_chunk;
        std::function<void()> on_copy_stop;
    };

public:
    LocalFile(const Params& params);
    ~LocalFile();
    bool pullRequest(int64_t device_id);
    void onFileChunkAck(uint32_t seq);

private:
    uint32_t sendFileChunk(uint32_t seq, uint16_t size);

private:
    Params params_;
    std::FILE* file_ = nullptr;
    std::deque<FileSendChunk> send_buffer_;
    uint32_t chunk_seq_ = 0;
    bool copying_ = false;
    int64_t peer_device_id_ = 0;
};

LocalFile::LocalFile(const Params& params)
    : params_{params} {}

LocalFile::~LocalFile() {
    if (file_ != nullptr) {
        std::fclose(file_);
    }
}

bool LocalFile::pullRequest(int64_t device_id) {
    // 这个copying标志位是为了保证我们同一时间只向一个remote发送文件
    if (copying_) {
        return false;
    }
    copying_ = true;
    peer_device_id_ = device_id;
    file_ = _wfopen(params_.wfullpath.c_str(), L"rb");
    if (file_ == nullptr) {
        copying_ = false;
        params_.log_print(kError, "Open local file '%s' failed", params_.fullpath.c_str());
        return false;
    }
    chunk_seq_ = 0;
    send_buffer_.clear();
    for (uint32_t i = 0; i < kMaxWindows; i++) {
        uint32_t chunk_seq = chunk_seq_++;
        uint32_t count = sendFileChunk(chunk_seq, kChunkSize);
        if (count == 0) {
            // 读完了，或者出错了
            std::fclose(file_);
            file_ = nullptr;
            params_.on_copy_stop();
            break;
        }
    }
    return true;
}

void LocalFile::onFileChunkAck(uint32_t seq) {
    // 将发送buffer中的数据块标记成Acked
    for (size_t i = 0; i < send_buffer_.size(); i++) {
        if (send_buffer_[i].seq == seq) {
            send_buffer_[i].state = FileSendChunk::State::Acked;
            break;
        }
    }
    // 弹出队列【头部】所有标记为Acked的数据块
    while (!send_buffer_.empty() && send_buffer_.front().state == FileSendChunk::State::Acked) {
        send_buffer_.pop_front();
    }
    // 如果发完所有文件块 停止发送，等ack
    if (file_ == nullptr) {
        return;
    }
    // 发送“可用窗口”个数据块
    for (size_t i = 0; i < kMaxWindows - send_buffer_.size(); i++) {
        uint32_t chunk_seq = chunk_seq_++;
        uint32_t count = sendFileChunk(chunk_seq, kChunkSize);
        if (count == 0) {
            // 读完了，或者出错了
            std::fclose(file_);
            file_ = nullptr;
            params_.on_copy_stop();
            return;
        }
    }
}

uint32_t LocalFile::sendFileChunk(uint32_t chunk_seq, uint16_t size) {
    std::vector<uint8_t> buffer(size);
    size_t count = std::fread(buffer.data(), sizeof(uint8_t), buffer.size(), file_);
    params_.send_file_chunk(peer_device_id_, params_.file_seq, chunk_seq, buffer.data(),
                            static_cast<uint16_t>(count));
    FileSendChunk chunk{};
    chunk.seq = chunk_seq;
    chunk.state = FileSendChunk::State::Sent;
    chunk.timestamp =
        std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now())
            .time_since_epoch()
            .count();
    chunk.data = std::vector<uint8_t>(buffer.data(), buffer.data() + count);
    send_buffer_.push_back(std::move(chunk));
    return static_cast<uint32_t>(count);
}

class NbClipboardImpl : public NbClipboard {
public:
    NbClipboardImpl(const NbClipboard::Params* params);
    virtual ~NbClipboardImpl();
    bool init();
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

private:
    static uint32_t updateLocalFileInfo(NbClipboard* ctx, const char* fullpath,
                                        const wchar_t* wfullpath, uint64_t size);
    static int32_t updateRemoteFileInfo(NbClipboard* ctx, int64_t device_id, uint32_t file_seq,
                                        const char* filename, const wchar_t* wfilename,
                                        uint64_t size);
    void prepareFunctionPointers();
    void consumeTasks();
    void postTask(const std::function<void()>& task);
    //**********作为接收端**************//
    void onPasteStart();
    void onPasteStop();
    void onPullRemoteFile(int64_t device_id, uint32_t file_seq);
    void sendFileChunkAck(int64_t device_id, uint32_t file_seq, uint32_t chunk_seq);
    static void onFileChunk(NbClipboard* ctx, int64_t device_id, uint32_t file_seq,
                            uint32_t chunk_seq, const uint8_t* data, uint16_t size);
    //**********作为发送端**************//
    static void onRemoteFilePullRequest(NbClipboard* ctx, int64_t device_id, uint32_t file_seq);
    static void onFileChunkAck(NbClipboard* ctx, uint32_t file_seq, uint32_t chunk_seq);
    void sendFileChunk(int64_t device_id, uint32_t file_seq, uint32_t chunk_seq,
                       const uint8_t* data, uint16_t size);
    void onCopyStop();

private:
    Params params_;
    // copying_有两种情况为true，一种是正在将远端的文件复制到本地，另一种是正在将本地的文件复制到远端
    std::atomic<bool> copying_{false};
    std::atomic<uint32_t> file_seq_{1};
    std::atomic<bool> stoped_{false};
    std::mutex mutex_;
    Microsoft::WRL::ComPtr<RemoteFile> remote_file_;
    std::optional<LocalFile> local_file_;
    HWND window_ = nullptr;
    std::thread thread_;
    std::vector<std::function<void()>> tasks_;
};

NbClipboardImpl::NbClipboardImpl(const NbClipboard::Params* params)
    : params_{*params} {
    prepareFunctionPointers();
}

NbClipboardImpl::~NbClipboardImpl() {
    stoped_ = true;
    if (window_) {
        PostMessageW(window_, kMsgStop, NULL, NULL);
    }
    remote_file_ = nullptr;
    if (thread_.joinable()) {
        thread_.join();
    }
}

void NbClipboardImpl::prepareFunctionPointers() {
    if (params_.log_print == nullptr) {
        params_.log_print = [](NbClipLogLevel, const char*, ...) {};
    }
    update_local_file_info = &NbClipboardImpl::updateLocalFileInfo;
    update_remote_file_info = &NbClipboardImpl::updateRemoteFileInfo;
    on_file_chunk = &NbClipboardImpl::onFileChunk;
    on_file_pull_request = &NbClipboardImpl::onRemoteFilePullRequest;
    on_file_chunk_ack = &NbClipboardImpl::onFileChunkAck;
}

void NbClipboardImpl::onPasteStart() {
    copying_ = true;
}

void NbClipboardImpl::onPasteStop() {
    copying_ = false;
    if (remote_file_) {
        remote_file_ = nullptr;
    }
}

void NbClipboardImpl::onCopyStop() {
    copying_ = false;
}

// 向对端请求拉取一个序列号为seq的文件
void NbClipboardImpl::onPullRemoteFile(int64_t device_id, uint32_t file_seq) {
    params_.send_file_pull_request(params_.userdata, device_id, file_seq);
}

void NbClipboardImpl::sendFileChunkAck(int64_t device_id, uint32_t file_seq, uint32_t chunk_seq) {
    params_.send_file_chunk_ack(params_.userdata, device_id, file_seq, chunk_seq);
}

void NbClipboardImpl::onFileChunk(NbClipboard* ctx, int64_t device_id, uint32_t file_seq,
                                  uint32_t chunk_seq, const uint8_t* data, uint16_t size) {
    auto that = static_cast<NbClipboardImpl*>(ctx);
    std::vector<uint8_t> chunk(data, data + size);
    auto remote_file = that->remote_file_;
    if (remote_file != nullptr) {
        remote_file->onRecvFileChunk(device_id, file_seq, chunk_seq, chunk);
    }
    else {
        that->params_.log_print(kWarn, "Received FileSendChunk but remote_file is nullptr");
    }
}

// 远端向我们请求拉取序号为file_seq的文件
void NbClipboardImpl::onRemoteFilePullRequest(NbClipboard* ctx, int64_t device_id,
                                              uint32_t file_seq) {
    auto that = static_cast<NbClipboardImpl*>(ctx);
    that->postTask([device_id, file_seq, that]() {
        if (that->copying_ || file_seq != that->file_seq_ || that->local_file_ == std::nullopt) {
            that->params_.log_print(
                kError,
                "onRemoteFilePullRequest copying %u, file_seq %u, file_seq_ %u, local_file_ %d",
                that->copying_.load(), file_seq, that->file_seq_.load(),
                (that->local_file_ == std::nullopt));
            return;
        }
        that->copying_ = true;
        if (!that->local_file_->pullRequest(device_id)) {
            that->copying_ = false;
        }
    });
}

void NbClipboardImpl::onFileChunkAck(NbClipboard* ctx, uint32_t file_seq, uint32_t chunk_seq) {
    auto that = static_cast<NbClipboardImpl*>(ctx);
    that->postTask([file_seq, chunk_seq, that]() {
        if (file_seq != that->file_seq_ || that->local_file_ == std::nullopt) {
            return;
        }
        that->local_file_->onFileChunkAck(chunk_seq);
    });
}

void NbClipboardImpl::sendFileChunk(int64_t device_id, uint32_t file_seq, uint32_t chunk_seq,
                                    const uint8_t* data, uint16_t size) {
    params_.send_file_chunk(params_.userdata, device_id, file_seq, chunk_seq, data, size);
}

// 检测到本地clipboard复制了文件，给该文件分配一个file_seq，后面对端可能会用这个file_seq请求拉取文件
uint32_t NbClipboardImpl::updateLocalFileInfo(NbClipboard* ctx, const char* fullpath,
                                              const wchar_t* wfullpath, uint64_t size) {
    std::promise<uint32_t> promise;
    std::string path = fullpath;
    std::wstring wpath = wfullpath;
    auto that = static_cast<NbClipboardImpl*>(ctx);
    that->postTask([path, wpath, size, that, &promise]() {
        if (that->copying_) {
            promise.set_value(0);
            return;
        }
        if (size == 0) {
            promise.set_value(0);
            return;
        }
        uint32_t file_seq = ++(that->file_seq_);
        // local_file和remote_file同时只能存在一个
        LocalFile::Params params{};
        params.log_print = that->params_.log_print;
        params.file_seq = file_seq;
        params.fullpath = path;
        params.wfullpath = wpath;
        params.size = size;
        params.send_file_chunk = std::bind(
            &NbClipboardImpl::sendFileChunk, that, std::placeholders::_1, std::placeholders::_2,
            std::placeholders::_3, std::placeholders::_4, std::placeholders::_5);
        params.on_copy_stop = std::bind(&NbClipboardImpl::onCopyStop, that);
        that->local_file_ = LocalFile{params};
        that->remote_file_ = nullptr;
        promise.set_value(file_seq);
    });
    return promise.get_future().get();
}

// 对端操作系统对某文件执行了'复制'操作，并把对应文件相关信息发过来了，我们后续会用这个file_seq请求拉取该文件
int32_t NbClipboardImpl::updateRemoteFileInfo(NbClipboard* ctx, int64_t device_id,
                                              uint32_t file_seq, const char* filename,
                                              const wchar_t* wfilename, uint64_t size) {
    auto that = static_cast<NbClipboardImpl*>(ctx);
    std::promise<int32_t> promise;
    std::wstring wname = wfilename;
    std::string name = filename;
    that->postTask([device_id, file_seq, wname, name, size, that, &promise]() {
        if (that->copying_) {
            promise.set_value(0);
            return;
        }
        if (size == 0) {
            promise.set_value(0);
            return;
        }
        // local_file和remote_file同时只能存在一个
        that->local_file_ = std::nullopt;
        RemoteFile::Params params{};
        params.device_id = device_id;
        params.file_seq = file_seq;
        params.filename = name;
        params.wfilename = wname;
        params.size = size;
        params.log_print = that->params_.log_print;
        params.on_paste_start = std::bind(&NbClipboardImpl::onPasteStart, that);
        params.on_paste_stop = std::bind(&NbClipboardImpl::onPasteStop, that);
        params.on_pull_file = std::bind(&NbClipboardImpl::onPullRemoteFile, that,
                                        std::placeholders::_1, std::placeholders::_2);
        params.on_file_chunk_ack =
            std::bind(&NbClipboardImpl::sendFileChunkAck, that, std::placeholders::_1,
                      std::placeholders::_2, std::placeholders::_3);
        that->remote_file_ = new RemoteFile{params};
        HRESULT hr = ::OleSetClipboard(that->remote_file_.Get()); // OleSetClipboard会AddRef
        if (FAILED(hr)) {
            promise.set_value(0);
            that->params_.log_print(kError, "OleSetClipboard failed %#x %#x", hr, GetLastError());
        }
        else {
            promise.set_value(1);
        }
    });
    return promise.get_future().get();
}

HINSTANCE getInstance() {
    HMODULE instance = NULL;
    if (!::GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                  GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                              reinterpret_cast<char*>(&NbClipboardImpl::WindowProc), &instance)) {
        return nullptr;
    }
    return instance;
}

ATOM registerClass(HINSTANCE instance) {
    WNDCLASSEX wndclass{};
    wndclass.cbSize = sizeof(wndclass);
    wndclass.style = 0;
    wndclass.lpfnWndProc = &NbClipboardImpl::WindowProc;
    wndclass.cbClsExtra = 0;
    wndclass.cbWndExtra = 0;
    wndclass.hInstance = instance;
    wndclass.hIcon = NULL;
    wndclass.hCursor = NULL;
    wndclass.hbrBackground = NULL;
    wndclass.lpszMenuName = NULL;
    wndclass.lpszClassName = L"nbclipClass";
    wndclass.hIconSm = NULL;
    return RegisterClassExW(&wndclass);
}

bool NbClipboardImpl::init() {
    std::promise<bool> promise;
    std::thread thread{[&promise, this]() {
        HRESULT hr = ::OleInitialize(nullptr);
        params_.log_print(kInfo, "OleInitialize %#x", hr);
        HINSTANCE instance = nbclip::getInstance();
        if (instance == nullptr) {
            ::OleUninitialize();
            promise.set_value(false);
            return;
        }
        ATOM atom = nbclip::registerClass(instance);
        if (atom == 0) {
            ::OleUninitialize();
            promise.set_value(false);
            return;
        }
        // 回收？？
        window_ =
            CreateWindowW(MAKEINTATOM(atom), NULL, 0, 0, 0, 0, 0, HWND_MESSAGE, 0, instance, this);
        if (window_ == nullptr) {
            ::OleUninitialize();
            promise.set_value(false);
            return;
        }
        (void)SetWindowLongPtrW(window_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        promise.set_value(true);
        MSG msg{};
        while (::GetMessageW(&msg, nullptr, 0, 0)) {
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
            if (window_ == nullptr || stoped_) {
                break;
            }
        }
        ::OleUninitialize();
    }};
    bool success = promise.get_future().get();
    if (success) {
        thread_ = std::move(thread);
        return true;
    }
    else {
        thread.join();
        return false;
    }
}

LRESULT CALLBACK NbClipboardImpl::WindowProc(HWND hwnd, UINT message, WPARAM wparam,
                                             LPARAM lparam) {
    NbClipboardImpl* that =
        reinterpret_cast<NbClipboardImpl*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    switch (message) {
    case kMsgPostTask:
        that->consumeTasks();
        return 0;
    case kMsgStop:
        return 0;
    default:
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
}

void NbClipboardImpl::consumeTasks() {
    std::vector<std::function<void()>> tasks;
    {
        std::lock_guard lock{mutex_};
        tasks = std::move(tasks_);
    }
    for (auto& task : tasks) {
        task();
    }
}

void NbClipboardImpl::postTask(const std::function<void()>& task) {
    {
        std::lock_guard lock{mutex_};
        tasks_.push_back(task);
    }
    if (window_) {
        PostMessageW(window_, kMsgPostTask, NULL, NULL);
    }
}

} // namespace nbclip

NbClipboard* createNbClipboard(const NbClipboard::Params* params) {
    auto cb = new nbclip::NbClipboardImpl{params};
    if (cb->init()) {
        return cb;
    }
    else {
        delete cb;
        return nullptr;
    }
}

void destroyNbClipboard(NbClipboard* ptr) {
    if (ptr == nullptr) {
        return;
    }
    auto ptr2 = static_cast<nbclip::NbClipboardImpl*>(ptr);
    delete ptr2;
}