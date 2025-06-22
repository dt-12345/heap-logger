#pragma once

#include "nn/util.h"
#include "nn/fs.h"

#include <heap/seadHeap.h>

template <size_t BufferSize>
class FileWriter {
public:
    FileWriter() = default;

    explicit FileWriter(sead::Heap* heap) {
        allocBuffer(heap);
    }

    ~FileWriter() {
        if (mBuffer && mHeap) {
            mHeap->free(mBuffer);
            mBuffer = nullptr;
            mHeap = nullptr;
            mBufferOffset = 0;
        }
    }

    void writeIndent(s32 indent) {
        if (mBufferOffset + indent * 2 >= BufferSize)
            return;
        
        for (s32 i = 0; i < indent; ++i)
            write("  ");
    }

    bool writef(const char* fmt, auto&&... args) {
        char fmt_buf[0x1000];
        s32 size = nn::util::SNPrintf(fmt_buf, sizeof(fmt_buf), fmt, std::forward<decltype(args)>(args)...);

        if (mBufferOffset + size >= BufferSize)
            return false;
        
        return write(fmt_buf, size);
    }

    template <size_t N>
    bool write(const char(&data)[N]) {
        if (mBufferOffset + N - 1 >= BufferSize)
            return false;

        memcpy(mBuffer + mBufferOffset, data, N - 1);
        mBufferOffset += N - 1;
        return true;
    }

    bool write(const char* data, size_t size) {
        if (mBufferOffset + size >= BufferSize)
            return false;
        
        memcpy(mBuffer + mBufferOffset, data, size);
        mBufferOffset += size;
        return true;
    }

    bool saveToFile(const char* output_path) {
        nn::fs::FileHandle handle{};
        if (nn::fs::OpenFile(&handle, output_path, nn::fs::OpenMode_Write)) {
            if (nn::fs::CreateFile(output_path, 0))
                return false;

            if (nn::fs::OpenFile(&handle, output_path, nn::fs::OpenMode_Write))
                return false;
        }

        s64 file_size = 0;
        nn::fs::GetFileSize(&file_size, handle);

        if (file_size != mBufferOffset)
            nn::fs::SetFileSize(handle, mBufferOffset);
        
        nn::fs::WriteOption option = nn::fs::WriteOption(0);
        nn::fs::WriteFile(handle, 0, mBuffer, mBufferOffset, option);
        nn::fs::FlushFile(handle);

        nn::fs::CloseFile(handle);

        return true;
    }

private:
    void allocBuffer(sead::Heap* heap) {
        if (mBuffer && mHeap) {
            mHeap->free(mBuffer);
            mBuffer = nullptr;
            mHeap = nullptr;
            mBufferOffset = 0;
        }

        mHeap = heap;
        mBuffer = static_cast<char*>(mHeap->tryAlloc(BufferSize, 1));
    }

    sead::Heap* mHeap = nullptr;
    char* mBuffer = nullptr;
    s32 mBufferOffset = 0;
};