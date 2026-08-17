#pragma once

#include "UnrealBridgeMaterialLibrary.h"

#include "Engine/Texture.h"

/**
 * 贴图刷新所需的最小引擎操作接口；生产适配器调用真实 UTexture API，测试适配器仅记录确定性的调用序列。
 * Minimal engine-operation interface for texture refresh; production calls real UTexture APIs while tests record deterministic call sequences.
 */
class IUnrealBridgeTextureRefreshOperations
{
public:
	virtual ~IUnrealBridgeTextureRefreshOperations() = default;

	virtual bool IsCompilingTexture(UTexture& Texture) const = 0;
	virtual void BlockOnAnyAsyncBuild(UTexture& Texture) const = 0;
	virtual void UpdateResource(UTexture& Texture) const = 0;
	virtual void UpdateResourceWithParams(UTexture& Texture, UTexture::EUpdateResourceFlags Flags) const = 0;
};

namespace UnrealBridgeTextureRefresh
{
	/**
	 * 提交普通或强制贴图刷新，并记录提交前的编译状态；调用成功仅表示请求已提交，不表示异步工作已完成。
	 * Submits an ordinary or forced texture refresh and captures pre-submit compilation state; success means submitted, not asynchronously completed.
	 */
	void Submit(
		UTexture& Texture,
		bool bForceDerivedDataRebuild,
		const IUnrealBridgeTextureRefreshOperations& Operations,
		FBridgeTextureRefreshResult& OutResult);
}
