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
    TestRecycledBuffers();
    TestMSAA();
    Check(glGetError() == GL_NO_ERROR, "driver reports no OpenGL error");
    return failures ? 41 : 0;
}
#else
int AutoTest_VerifyOpenGL() { return 41; }
#endif
