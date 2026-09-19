#pragma once

#include "ContentBrowserItemPath.h"

namespace BridgeEditorImpl
{
	inline FString GetContentBrowserInternalPath(const FContentBrowserItemPath& Path)
	{
		// Virtual-only folders and an absent browser have no internal package path.
		// GetInternalPathString asserts on those paths, rather than returning empty.
		return Path.HasInternalPath() ? Path.GetInternalPathString() : FString();
	}
}
