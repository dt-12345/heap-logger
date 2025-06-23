#include "debugdraw.h"
#include "callbacks.h"
#include "fileio.hpp"

#include "nn/fs.h"
#include "nn/hid.h"
#include "nn/os/os_tick.hpp"
#include "nn/os.h"

#include <container/seadPtrArray.h>
#include <heap/seadHeap.h>

sead::PtrArray<sead::Heap>* gRootHeaps = nullptr;
sead::Heap** gVirtualAddressHeapPtr = nullptr;
nn::os::MutexType* gHeapTreeMutex = nullptr;

void PrintInfo(const sead::Heap* heap, sead::TextWriter& writer, int indent = 0) {
    if (writer.getCursor().y < -writer.getHalfHeight() + 5.f) {
        // three columns
        if (writer.getCursor().x < writer.getWidth() / -6.f)
            writer.setCursor({writer.getWidth() / -6.f + 1.f, writer.getHalfHeight() - 1.f});
        else if (writer.getCursor().x < writer.getWidth() / 6)
            writer.setCursor({writer.getWidth() / 6.f + 1.f, writer.getHalfHeight() - 1.f});
        else
            return;
    }
    sead::Vector2f old_cursor = writer.getCursor();
    for (int i = 0; i < indent; ++i) {
        writer.printf("  ");
    }
    writer.printDropShadow(
        "%s (%.2f%) [Free: %lld B, Used: %lld B, Total: %lld B]\n",
        (float)(heap->getSize() - heap->getFreeSize()) / (float)heap->getSize() * 100.f,
        heap->getName(),
        heap->getFreeSize(),
        heap->getSize() - heap->getFreeSize(),
        heap->getSize()
    );
    sead::Vector2f new_cursor = writer.getCursor();
    writer.setCursor({old_cursor.x, new_cursor.y});
}

void VisitChildren(const sead::Heap* heap, sead::TextWriter& writer, int depth = 0) {
    // unsure if any heaps are even 5 levels deep
    if (!heap || depth > 5)
        return;

    // there's too many of these
    if (heap->getName() == "ConstraintPoolingHandlerHeap")
        return;
    
    PrintInfo(heap, writer, depth);

    // too many children
    if (heap->getName() == "ActorInstanceHeap" || heap->getName() == "ケミカル" || heap->getName() == "UIHeap")
        return;

    for (const auto& child : heap->mChildren) {
        VisitChildren(&child, writer, depth + 1);
    }
}

void DumpHeap(const sead::Heap* heap, FileWriter<0xa00000>& writer, int indent = 0, bool dump_children = true) {
    if (!heap)
        return;
    
    writer.writeIndent(indent);
    writer.writef("- Name: %s\n", heap->getName());
    writer.writeIndent(indent + 1);
    writer.writef("Free: %lld\n", heap->getFreeSize());
    writer.writeIndent(indent + 1);
    writer.writef("Used: %lld\n", heap->getSize() - heap->getFreeSize());
    writer.writeIndent(indent + 1);
    writer.writef("Total: %lld\n", heap->getSize());
    
    if (dump_children) {
        writer.writeIndent(indent + 1);
        if (heap->mChildren.isEmpty()) {
            writer.write("Children: []\n");
        } else {
            writer.write("Children:\n");
            for (const auto& child : heap->mChildren)
                DumpHeap(&child, writer, indent + 1);
        }
    }
}

bool gInitializedSDCard = false;
bool gIsDisplay = true;
bool gIsOutput = false;
u64 gFrameCounter = 0;
nn::hid::NpadBaseState gState{};
nn::hid::NpadBaseState gPreviousState{};

void updateInput() {
    const u32 port = 0;
    nn::hid::NpadStyleSet style = nn::hid::GetNpadStyleSet(port);

    gPreviousState = gState; 
    if (style.isBitSet(nn::hid::NpadStyleTag::NpadStyleFullKey))
        nn::hid::GetNpadStates((nn::hid::NpadFullKeyState*)(&gState), 1, port);
    else if (style.isBitSet(nn::hid::NpadStyleTag::NpadStyleHandheld))
        nn::hid::GetNpadStates((nn::hid::NpadHandheldState*)(&gState), 1, port);
    else if (style.isBitSet(nn::hid::NpadStyleTag::NpadStyleJoyDual))
        nn::hid::GetNpadStates((nn::hid::NpadJoyDualState*)(&gState), 1, port);
    else if (style.isBitSet(nn::hid::NpadStyleTag::NpadStyleJoyLeft))
        nn::hid::GetNpadStates((nn::hid::NpadJoyLeftState*)(&gState), 1, port);
    else if (style.isBitSet(nn::hid::NpadStyleTag::NpadStyleJoyRight))
        nn::hid::GetNpadStates((nn::hid::NpadJoyRightState*)(&gState), 1, port);

    // only register on button down, not on hold/release
    if (gState.mButtons.isBitSet(nn::hid::NpadButton::Down) && gState.mButtons.isBitSet(nn::hid::NpadButton::ZL)
        && gState.mButtons.isBitSet(nn::hid::NpadButton::ZR) && (!gPreviousState.mButtons.isBitSet(nn::hid::NpadButton::Down)
        || !gPreviousState.mButtons.isBitSet(nn::hid::NpadButton::ZL) || !gPreviousState.mButtons.isBitSet(nn::hid::NpadButton::ZR))) {
        gIsDisplay = !gIsDisplay;
    }

    if (gState.mButtons.isBitSet(nn::hid::NpadButton::Down) && gState.mButtons.isBitSet(nn::hid::NpadButton::L)
        && gState.mButtons.isBitSet(nn::hid::NpadButton::R) && (!gPreviousState.mButtons.isBitSet(nn::hid::NpadButton::Down)
        || !gPreviousState.mButtons.isBitSet(nn::hid::NpadButton::L) || !gPreviousState.mButtons.isBitSet(nn::hid::NpadButton::R))) {
        gIsOutput = !gIsOutput;
    }
}

void drawTool2DSuper(agl::lyr::Layer* layer, const agl::lyr::RenderInfo& info) {
    updateInput();

    if (gInitializedSDCard && gIsOutput && gVirtualAddressHeapPtr && *gVirtualAddressHeapPtr) {
        FileWriter<0xa00000> file_writer(*gVirtualAddressHeapPtr);
        
        nn::os::LockMutex(gHeapTreeMutex);
        file_writer.write("VirtualAddressHeaps:\n");
        // this heap is used for file resources and there's one heap per file loaded and that's simply way too many
        DumpHeap(*gVirtualAddressHeapPtr, file_writer, 0, false);
        
        if (gRootHeaps && !gRootHeaps->isEmpty()) {
            file_writer.write("RootHeaps:\n");
            for (int i = 0; i < 4; ++i) {
                const sead::Heap* heap = gRootHeaps->at(i);
                if (heap)
                    DumpHeap(heap, file_writer);
            }
        } else {
            file_writer.write("RootHeaps: []\n");
        }
        nn::os::UnlockMutex(gHeapTreeMutex);

        char path[0x80];
        s32 size = nn::util::SNPrintf(path, sizeof(path), "sd:/HeapLog_%lld_%016llx.yml", gFrameCounter, nn::os::GetSystemTick().GetInt64Value());
        path[size] = '\0';

        file_writer.saveToFile(path);

        gIsOutput = false;
    }

    sead::TextWriter writer = initializeTextWriter(info);
    writer.setCursorFromTopLeft({ 1.f, 1.f});
    writer.scaleBy(0.5f);

    writer.printDropShadow("Frame: %lld\n", gFrameCounter);

    ++gFrameCounter;

    if (!gIsDisplay)
        return;

    writer.printDropShadow("Heap Usage:\n");
    
    nn::os::LockMutex(gHeapTreeMutex);
    if (gVirtualAddressHeapPtr && *gVirtualAddressHeapPtr)
        PrintInfo(*gVirtualAddressHeapPtr, writer);

    if (gRootHeaps && !gRootHeaps->isEmpty()) {
        sead::Heap* root = gRootHeaps->at(0);
        if (root)
            VisitChildren(root, writer);
    }
    nn::os::UnlockMutex(gHeapTreeMutex);
}

HOOK_DEFINE_TRAMPOLINE(MountSD) {
    static void Callback() {
        gInitializedSDCard = nn::fs::MountSdCard("sd") == 0;

        Orig();
    }
};

extern "C" void nnMain();

static const u64 sRootHeapOffsets[6] = {
    0x0463be08, 0x04719328, 0x04721258, 0x04713738, 0x04707b90, 0x04716c08
};

static const u64 sVirtualAddressHeapOffsets[6] = {
    0x0463be68, 0x04719388, 0x047212b8, 0x04713798, 0x04707bf0, 0x04716c68
};

static const u64 sHeapTreeCSOffsets[6] = {
    0x0463be80, 0x047193a0, 0x047212d0, 0x047137b0, 0x04707c08, 0x04716c80
};

extern "C" void exl_main(void* x0, void* x1) {
    initDebugDrawer();

    gRootHeaps = reinterpret_cast<sead::PtrArray<sead::Heap>*>(exl::util::modules::GetTargetOffset(sRootHeapOffsets[gDrawMgr.version()]));
    gVirtualAddressHeapPtr = reinterpret_cast<sead::Heap**>(exl::util::modules::GetTargetOffset(sVirtualAddressHeapOffsets[gDrawMgr.version()]));
    gHeapTreeMutex = reinterpret_cast<nn::os::MutexType*>(exl::util::modules::GetTargetOffset(sHeapTreeCSOffsets[gDrawMgr.version()]));

    MountSD::InstallAtFuncPtr(nnMain);
}

extern "C" NORETURN void exl_exception_entry() {
    /* TODO: exception handling */
    EXL_ABORT(0x420);
}