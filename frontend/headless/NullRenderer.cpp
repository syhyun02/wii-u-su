#include "Common/precompiled.h"
#include "NullRenderer.h"
#include "Cafe/HW/Latte/Core/LatteTexture.h"
#include "Cafe/HW/Latte/Core/LatteCachedFBO.h"
#include "Cafe/HW/Latte/Core/LatteQueryObject.h"
#include "Cafe/HW/Latte/Renderer/RendererShader.h"

namespace
{
	class NullTextureView : public LatteTextureView
	{
	public:
		NullTextureView(LatteTexture* tex, Latte::E_DIM dim, Latte::E_GX2SURFFMT format, sint32 firstMip, sint32 mipCount, sint32 firstSlice, sint32 sliceCount)
			: LatteTextureView(tex, firstMip, mipCount, firstSlice, sliceCount, dim, format) {}
	};

	class NullTexture : public LatteTexture
	{
	public:
		using LatteTexture::LatteTexture;
		void AllocateOnHost() override {}
	protected:
		LatteTextureView* CreateView(Latte::E_DIM dim, Latte::E_GX2SURFFMT format, sint32 firstMip, sint32 mipCount, sint32 firstSlice, sint32 sliceCount) override
		{
			return new NullTextureView(this, dim, format, firstMip, mipCount, firstSlice, sliceCount);
		}
	};

	class NullCachedFBO : public LatteCachedFBO
	{
	public:
		NullCachedFBO(uint64 key) : LatteCachedFBO(key) {}
	};

	class NullShader : public RendererShader
	{
	public:
		NullShader(ShaderType type, uint64 baseHash, uint64 auxHash, bool isGameShader, bool isGfxPack)
			: RendererShader(type, baseHash, auxHash, isGameShader, isGfxPack) {}
		void PreponeCompilation(bool) override {}
		bool IsCompiled() override { return true; }
		bool WaitForCompiled() override { return true; }
	};

	class NullQuery : public LatteQueryObject
	{
	public:
		bool getResult(uint64& numSamplesPassed) override { numSamplesPassed = 1; return true; }
		void begin() override {}
		void end() override { queryEnded = true; }
	};
}

NullRenderer::NullRenderer() : Renderer(RendererAPI::Vulkan)
{
	m_selectedDeviceName = "Null renderer";
}

NullRenderer::~NullRenderer() = default;

void NullRenderer::SwapBuffers(bool swapTV, bool swapDRC)
{
	m_swapCount++;
}

LatteCachedFBO* NullRenderer::rendertarget_createCachedFBO(uint64 key)
{
	return new NullCachedFBO(key);
}

void NullRenderer::rendertarget_deleteCachedFBO(LatteCachedFBO* fbo)
{
	delete fbo;
}

void* NullRenderer::texture_acquireTextureUploadBuffer(uint32 size)
{
	if (m_uploadBuffer.size() < size)
		m_uploadBuffer.resize(size);
	return m_uploadBuffer.data();
}

LatteTexture* NullRenderer::texture_createTextureEx(Latte::E_DIM dim, MPTR physAddress, MPTR physMipAddress, Latte::E_GX2SURFFMT format, uint32 width, uint32 height, uint32 depth, uint32 pitch, uint32 mipLevels, uint32 swizzle, Latte::E_HWTILEMODE tileMode, bool isDepth)
{
	return new NullTexture(dim, physAddress, physMipAddress, format, width, height, depth, pitch, mipLevels, swizzle, tileMode, isDepth);
}

RendererShader* NullRenderer::shader_create(RendererShader::ShaderType type, uint64 baseHash, uint64 auxHash, const std::string& source, bool compileAsync, bool isGfxPackSource)
{
	return new NullShader(type, baseHash, auxHash, baseHash != 0, isGfxPackSource);
}

Renderer::IndexAllocation NullRenderer::indexData_reserveIndexMemory(uint32 size)
{
	IndexAllocation a;
	a.mem = malloc(size ? size : 4);
	a.rendererInternal = nullptr;
	return a;
}

void NullRenderer::indexData_releaseIndexMemory(IndexAllocation& allocation)
{
	free(allocation.mem);
	allocation.mem = nullptr;
}

LatteQueryObject* NullRenderer::occlusionQuery_create()
{
	return new NullQuery();
}

void NullRenderer::occlusionQuery_destroy(LatteQueryObject* queryObj)
{
	delete queryObj;
}
