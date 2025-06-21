#include "debugdraw.h"
#include "callbacks.h"

#include <container/seadPtrArray.h>
#include <heap/seadHeap.h>

sead::PtrArray<sead::Heap>* gRootHeaps = nullptr;
sead::Heap** gVirtualAddressHeapPtr = nullptr;

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

void drawTool2DSuper(agl::lyr::Layer* layer, const agl::lyr::RenderInfo& info) {
    sead::TextWriter writer = initializeTextWriter(info);
    writer.setCursorFromTopLeft({ 1.f, 1.f});
    writer.scaleBy(0.5f);

    writer.printDropShadow("Heap Usage:\n");
    
    if (gVirtualAddressHeapPtr && *gVirtualAddressHeapPtr)
        PrintInfo(*gVirtualAddressHeapPtr, writer);

    if (gRootHeaps && !gRootHeaps->isEmpty()) {
        sead::Heap* root = gRootHeaps->at(0);
        if (root)
            VisitChildren(root, writer);
    }
}

extern "C" void exl_main(void* x0, void* x1) {
    initDebugDrawer();
    
    gRootHeaps = reinterpret_cast<sead::PtrArray<sead::Heap>*>(exl::util::modules::GetTargetOffset(0x04716c08));
    gVirtualAddressHeapPtr = reinterpret_cast<sead::Heap**>(exl::util::modules::GetTargetOffset(0x04716c68));
}

extern "C" NORETURN void exl_exception_entry() {
    /* TODO: exception handling */
    EXL_ABORT(0x420);
}