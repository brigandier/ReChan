#include "pc/autotest.h"

#if !defined(RC_PLATFORM_SWITCH) && !defined(RC_PLATFORM_NULL)
#include "p3d/context.h"
#include "pddi/gl/glrender.h"
#include "glad/gl.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <memory>

namespace {
int failures = 0;
void Check(bool condition, const char* name) {
    std::printf("%s OpenGL: %s\n", condition ? "PASS" : "FAIL", name);
    failures += !condition;
}
template<class T> struct Release {
    void operator()(T* value) const { if (value) value->Release(); }
};
using Target = std::unique_ptr<pddiRenderTarget, Release<pddiRenderTarget>>;
using Buffer = std::unique_ptr<glPrimBuffer, Release<glPrimBuffer>>;

PFNGLBUFFERDATAPROC originalBufferData = nullptr;
int vertexStores = 0, indexStores = 0;
void GLAD_API_PTR CountBufferData(GLenum target, GLsizeiptr size, const void* data, GLenum usage) {
    if (target == GL_ARRAY_BUFFER) ++vertexStores;
    if (target == GL_ELEMENT_ARRAY_BUFFER) ++indexStores;
    originalBufferData(target, size, data, usage);
}
struct CountStorage {
    CountStorage() {
        originalBufferData = glad_glBufferData;
        vertexStores = indexStores = 0;
        glad_glBufferData = CountBufferData;
    }
    ~CountStorage() { glad_glBufferData = originalBufferData; }
};

PFNGLTEXIMAGE2DPROC originalTexImage2D = nullptr;
int skippedAllocation = 0;
void GLAD_API_PTR SkipTextureAllocation(GLenum target, GLint level, GLint internalFormat,
    GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* data) {
    if (--skippedAllocation != 0)
        originalTexImage2D(target, level, internalFormat, width, height, border, format, type, data);
}
struct FailTextureAllocation {
    explicit FailTextureAllocation(int allocation) {
        originalTexImage2D = glad_glTexImage2D;
        skippedAllocation = allocation;
        glad_glTexImage2D = SkipTextureAllocation;
    }
    ~FailTextureAllocation() { glad_glTexImage2D = originalTexImage2D; }
};

PFNGLCHECKFRAMEBUFFERSTATUSPROC originalFramebufferStatus = nullptr;
GLenum GLAD_API_PTR IncompleteFramebuffer(GLenum) { return GL_FRAMEBUFFER_UNSUPPORTED; }
struct FailFramebuffer {
    FailFramebuffer() {
        originalFramebufferStatus = glad_glCheckFramebufferStatus;
        glad_glCheckFramebufferStatus = IncompleteFramebuffer;
    }
    ~FailFramebuffer() { glad_glCheckFramebufferStatus = originalFramebufferStatus; }
};

void TestRenderTargetRecovery() {
    GLint maximum = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximum);
    for (auto format : {PDDI_RENDER_TARGET_RGBA8, PDDI_RENDER_TARGET_RGBA16F,
        PDDI_RENDER_TARGET_DEPTH24, PDDI_RENDER_TARGET_DEPTH32F}) {
        Target target(p3d::context->CreateRenderTarget(16, 16, format, true));
        Check(target != nullptr, "recovery fixture creates a valid render target");
        if (!target) continue;
        const auto framebuffer = static_cast<glRenderTarget*>(target.get())->GetFramebuffer();
        auto* texture = target->GetTexture();
        auto* idTexture = target->GetIdTexture();
        const auto handle = texture->GetNativeHandle();
        const auto idHandle = idTexture ? idTexture->GetNativeHandle() : 0;
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        const GLfloat red[] = {1, 0, 0, 1}, depth = 0.75f;
        const GLuint id = 37;
        if (idTexture) {
            glClearBufferfv(GL_DEPTH, 0, &depth);
            glClearBufferuiv(GL_COLOR, 0, &id);
        }
        else glClearBufferfv(GL_COLOR, 0, red);
        const auto preserved = [&] {
            GLint read = 0, draw = 0;
            glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw);
            return target->IsValid() && target->GetWidth() == 16 && target->GetHeight() == 16
                && texture == target->GetTexture() && texture->GetNativeHandle() == handle
                && idTexture == target->GetIdTexture()
                && (!idTexture || idTexture->GetNativeHandle() == idHandle)
                && read == framebuffer && draw == framebuffer
                && glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
        };
        Check(!target->Resize(maximum + 1, 16), "dimensions above the driver limit are rejected");
        Check(preserved(), "unsupported resize preserves the complete target and active bindings");
        Check(glGetError() == GL_NO_ERROR, "unsupported dimensions are rejected before driver allocation");
        for (int allocation = 1; allocation <= (idTexture ? 2 : 1); ++allocation) {
            FailTextureAllocation failure(allocation);
            Check(!target->Resize(32, 24), "missing texture storage is detected");
            Check(preserved(), "allocation failure preserves both original attachments");
        }
        {
            FailFramebuffer failure;
            Check(!target->Resize(32, 24), "incomplete framebuffer is rejected");
        }
        Check(preserved(), "incomplete framebuffer restores both previous attachments");
        if (preserved()) {
            if (idTexture) {
                GLfloat actualDepth = 0;
                GLuint actualId = 0;
                glReadPixels(0, 0, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &actualDepth);
                glReadBuffer(GL_COLOR_ATTACHMENT0);
                glReadPixels(0, 0, 1, 1, GL_RED_INTEGER, GL_UNSIGNED_INT, &actualId);
                Check(actualDepth == depth && actualId == id, "failed resize retains depth and instance-ID pixels");
            }
            else {
                std::array<unsigned char, 4> pixel{};
                glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
                Check(pixel[0] == 255 && pixel[1] == 0, "failed resize retains color pixels");
            }
        }
        Check(target->Resize(32, 24) && target->IsValid(), "resize recovers after allocation failure");
        Check(texture == target->GetTexture() && idTexture == target->GetIdTexture(),
            "successful resize retains public texture object identities");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        {
            FailTextureAllocation failure(1);
            Target missing(p3d::context->CreateRenderTarget(16, 16, format, true));
            Check(!missing, "initial allocation failure returns no render target");
        }
    }
}

void TestRenderTargetBindings() {
    Target read(p3d::context->CreateRenderTarget(16, 16, PDDI_RENDER_TARGET_RGBA8));
    Target draw(p3d::context->CreateRenderTarget(16, 16, PDDI_RENDER_TARGET_RGBA8));
    Target resized(p3d::context->CreateRenderTarget(16, 16, PDDI_RENDER_TARGET_RGBA16F));
    Check(read && draw && resized, "render targets created on the real graphics context");
    if (!read || !draw || !resized) return;
    const auto readId = static_cast<glRenderTarget*>(read.get())->GetFramebuffer();
    const auto drawId = static_cast<glRenderTarget*>(draw.get())->GetFramebuffer();
    const GLfloat red[] = {1, 0, 0, 1}, green[] = {0, 1, 0, 1};
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, readId);
    glClearBufferfv(GL_COLOR, 0, red);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawId);
    glClearBufferfv(GL_COLOR, 0, green);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, readId);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawId);
    Check(resized->Resize(32, 24), "render target resize succeeds");
    GLint actualRead = 0, actualDraw = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &actualRead);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &actualDraw);
    Check(actualRead == readId && actualDraw == drawId,
        "resize preserves independent read and draw framebuffer bindings");
    std::array<unsigned char, 4> pixel{};
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
    Check(pixel[0] == 255 && pixel[1] == 0 && pixel[2] == 0,
        "readback after resize reads the original red image, not the green draw target");
    Check(p3d::context->SetRenderTarget(resized.get()) && p3d::context->SetRenderTarget(nullptr),
        "temporary render target can be entered and left");
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &actualRead);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &actualDraw);
    Check(actualRead == readId && actualDraw == drawId,
        "leaving a render target restores independent read and draw framebuffer bindings");
    Check(!resized->Resize(0, 24) && resized->IsValid()
        && resized->GetWidth() == 32 && resized->GetHeight() == 24,
        "invalid dimensions preserve the previous valid render target");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void TestRecycledBuffers() {
    const pddiPrimBufferDesc desc(PDDI_PRIM_TRIANGLES, PDDI_V_POSITION, 3, 3);
    const std::array<float, 9> vertices = {-1,-1,0, 1,-1,0, 0,1,0};
    const std::array<u16, 3> indices = {0,1,2};
    Buffer first(new glPrimBuffer(desc));
    first->SetVertexData(vertices.data(), 3);
    first->SetIndices(indices.data(), 3);
    const auto vao = first->GetVAO();
    first.reset();
    Buffer recycled(new glPrimBuffer(desc));
    Check(recycled->GetVAO() == vao, "fixture uses a recycled VAO and its buffers");
    {
        CountStorage counts;
        for (int i = 0; i < 32; ++i) recycled->SetVertexData(vertices.data(), 3);
        Check(vertexStores == 1, "32 vertex-only updates orphan recycled storage exactly once");
        for (int i = 0; i < 32; ++i) recycled->SetIndices(indices.data(), 3);
        Check(indexStores == 1, "32 index updates orphan recycled storage exactly once");
        std::printf("METRIC OpenGL storage allocations: vertices=%d indices=%d for 32 updates each\n", vertexStores, indexStores);
    }
    glBindVertexArray(recycled->GetVAO());
    GLint vbo = 0, ebo = 0;
    glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &vbo);
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &ebo);
    std::array<float, 9> actualVertices{};
    std::array<u16, 3> actualIndices{};
    glBindBuffer(GL_COPY_READ_BUFFER, vbo);
    glGetBufferSubData(GL_COPY_READ_BUFFER, 0, sizeof(actualVertices), actualVertices.data());
    glBindBuffer(GL_COPY_READ_BUFFER, ebo);
    glGetBufferSubData(GL_COPY_READ_BUFFER, 0, sizeof(actualIndices), actualIndices.data());
    Check(actualVertices == vertices && actualIndices == indices, "GPU vertex and index contents are preserved");
    glBindBuffer(GL_COPY_READ_BUFFER, 0);
    glBindVertexArray(0);
    recycled.reset();
    Buffer reversed(new glPrimBuffer(desc));
    {
        CountStorage counts;
        reversed->SetIndices(indices.data(), 3);
        reversed->SetVertexData(vertices.data(), 3);
        reversed->SetVertexData(vertices.data(), 3);
        Check(indexStores == 1 && vertexStores == 1,
            "index-first updates independently orphan both recycled storage objects");
    }
}

void TestMSAA() {
    GLint maximum = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &maximum);
    for (int requested : {0, 2, 4, 8, 16, 0}) {
        p3d::display->SetMSAA(requested);
        const int effective = p3d::display->GetMSAA();
        Check(effective >= 0 && effective <= maximum, "MSAA respects the driver sample limit");
        p3d::context->BeginFrame();
        p3d::context->Clear(PDDI_BUFFER_COLOUR | PDDI_BUFFER_DEPTH);
        p3d::context->EndFrame();
        Check(glGetError() == GL_NO_ERROR, "MSAA framebuffer allocation and resolve complete without errors");
    }
}
}

int AutoTest_VerifyOpenGL() {
    failures = 0;
    TestRenderTargetBindings();
    TestRenderTargetRecovery();
    TestRecycledBuffers();
    TestMSAA();
    Check(glGetError() == GL_NO_ERROR, "driver reports no OpenGL error");
    return failures ? 41 : 0;
}
#else
int AutoTest_VerifyOpenGL() { return 41; }
#endif
