#pragma once
// A renderer that accepts every Latte command and draws nothing.
// Lets the whole Cafe system (CPU, OS HLE, GX2, Latte command processor) run without a GPU.
// It reports itself as Vulkan so the shader decompiler produces Vulkan-style GLSL (never compiled).
#include "Cafe/HW/Latte/Renderer/Renderer.h"

class NullRenderer : public Renderer
{
public:
	NullRenderer();
	~NullRenderer() override;

	bool IsPadWindowActive() override { return false; }
	void ClearColorbuffer(bool) override {}
	void DrawEmptyFrame(bool) override {}
	void SwapBuffers(bool swapTV, bool swapDRC) override;
	void DrawBackbufferQuad(LatteTextureView*, RendererOutputShader*, bool, sint32, sint32, sint32, sint32, bool, bool) override {}
	bool BeginFrame(bool) override { return true; }
	void Flush(bool) override {}
	void NotifyLatteCommandProcessorIdle() override {}

	bool ImguiBegin(bool) override { return false; }
	void ImguiEnd() override {}
	ImTextureID GenerateTexture(const std::vector<uint8>&, const Vector2i&) override { return nullptr; }
	void DeleteTexture(ImTextureID) override {}
	void DeleteFontTextures() override {}
	void AppendOverlayDebugInfo() override {}

	void renderTarget_setViewport(float, float, float, float, float, float, bool) override {}
	void renderTarget_setScissor(sint32, sint32, sint32, sint32) override {}
	LatteCachedFBO* rendertarget_createCachedFBO(uint64 key) override;
	void rendertarget_deleteCachedFBO(LatteCachedFBO* fbo) override;
	void rendertarget_bindFramebufferObject(LatteCachedFBO*) override {}

	void* texture_acquireTextureUploadBuffer(uint32 size) override;
	void texture_releaseTextureUploadBuffer(uint8*) override {}
	TextureDecoder* texture_chooseDecodedFormat(Latte::E_GX2SURFFMT, bool, Latte::E_DIM, uint32, uint32) override { return nullptr; }
	void texture_clearSlice(LatteTexture*, sint32, sint32) override {}
	void texture_loadSlice(LatteTexture*, sint32, sint32, sint32, void*, sint32, sint32, uint32) override {}
	void texture_clearColorSlice(LatteTexture*, sint32, sint32, float, float, float, float) override {}
	void texture_clearDepthSlice(LatteTexture*, uint32, sint32, bool, bool, float, uint32) override {}
	LatteTexture* texture_createTextureEx(Latte::E_DIM dim, MPTR physAddress, MPTR physMipAddress, Latte::E_GX2SURFFMT format, uint32 width, uint32 height, uint32 depth, uint32 pitch, uint32 mipLevels, uint32 swizzle, Latte::E_HWTILEMODE tileMode, bool isDepth) override;
	void texture_setLatteTexture(LatteTextureView*, uint32) override {}
	void texture_copyImageSubData(LatteTexture*, sint32, sint32, sint32, sint32, LatteTexture*, sint32, sint32, sint32, sint32, sint32, sint32, sint32) override {}
	LatteTextureReadbackInfo* texture_createReadback(LatteTextureView*) override { return nullptr; }

	void surfaceCopy_copySurfaceWithFormatConversion(LatteTexture*, sint32, sint32, LatteTexture*, sint32, sint32, sint32, sint32) override {}

	void bufferCache_init(const sint32) override {}
	void bufferCache_upload(uint8*, sint32, uint32) override {}
	void bufferCache_copy(uint32, uint32, uint32) override {}
	void bufferCache_copyStreamoutToMainBuffer(uint32, uint32, uint32) override {}
	void buffer_bindVertexBuffers(std::span<BindBufferParam>) override {}
	void buffer_bindUniformBuffer(LatteConst::ShaderType, uint32, uint32, uint32) override {}

	RendererShader* shader_create(RendererShader::ShaderType type, uint64 baseHash, uint64 auxHash, const std::string& source, bool compileAsync, bool isGfxPackSource) override;

	void streamout_setupXfbBuffer(uint32, sint32, uint32, uint32) override {}
	void streamout_begin() override {}
	void streamout_rendererFinishDrawcall() override {}

	void draw_beginSequence() override {}
	void draw_execute(uint32, uint32, uint32, uint32, MPTR, Latte::LATTE_VGT_DMA_INDEX_TYPE::E_INDEX_TYPE, const LatteDrawcallContext&) override { m_drawCount++; }
	void draw_endSequence() override {}

	IndexAllocation indexData_reserveIndexMemory(uint32 size) override;
	void indexData_releaseIndexMemory(IndexAllocation& allocation) override;
	void indexData_uploadIndexMemory(IndexAllocation&) override {}

	LatteQueryObject* occlusionQuery_create() override;
	void occlusionQuery_destroy(LatteQueryObject* queryObj) override;
	void occlusionQuery_flush() override {}
	void occlusionQuery_updateState() override {}

	uint64 GetSwapCount() const { return m_swapCount; }
	uint64 GetDrawCount() const { return m_drawCount; }

private:
	std::vector<uint8> m_uploadBuffer;
	std::atomic<uint64> m_swapCount{0};
	std::atomic<uint64> m_drawCount{0};
};
