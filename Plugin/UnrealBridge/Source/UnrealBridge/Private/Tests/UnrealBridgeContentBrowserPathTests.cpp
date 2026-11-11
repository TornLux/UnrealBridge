#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "UnrealBridgeContentBrowserPath.h"
#include "UnrealBridgeEditorLibrary.h"

#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnrealBridgeContentBrowserInternalPathTest,
	"UnrealBridge.Editor.ContentBrowserPath.InternalPathOrEmpty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealBridgeContentBrowserInternalPathTest::RunTest(const FString& Parameters)
{
	using BridgeEditorImpl::GetContentBrowserInternalPath;

	// Exercise the public getter too, without opening or changing any browser.
	FContentBrowserModule& CBM = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	const FContentBrowserItemPath CurrentPath = CBM.Get().GetCurrentPath();
	TestEqual(TEXT("The public getter safely returns the current internal path or empty"),
		UUnrealBridgeEditorLibrary::GetContentBrowserPath(), GetContentBrowserInternalPath(CurrentPath));

	// GetCurrentPath returns this default value when there is no primary browser.
	const FContentBrowserItemPath EmptyPath;
	TestFalse(TEXT("An absent browser has no internal path"), EmptyPath.HasInternalPath());
	TestEqual(TEXT("An absent browser returns empty, not None"),
		GetContentBrowserInternalPath(EmptyPath), FString());

	// Use the engine's real virtual-path conversion without opening or navigating a browser.
	for (const TCHAR* VirtualPath : { TEXT("/"), TEXT("/All"), TEXT("/All/Plugins") })
	{
		const FContentBrowserItemPath Path(VirtualPath, EContentBrowserPathType::Virtual);
		if (TestFalse(FString::Printf(TEXT("%s is virtual-only"), VirtualPath), Path.HasInternalPath()))
		{
			TestEqual(FString::Printf(TEXT("%s returns empty without asserting"), VirtualPath),
				GetContentBrowserInternalPath(Path), FString());
		}
	}

	// Paired paths avoid depending on local mount points or Content Browser settings.
	const FContentBrowserItemPath InternalPaths[] = {
		{ FName(TEXT("/All/Game")), FName(TEXT("/Game")) },
		{ FName(TEXT("/All/Game/Maps")), FName(TEXT("/Game/Maps")) },
		{ FName(TEXT("/All/Engine")), FName(TEXT("/Engine")) },
		{ FName(TEXT("/All/Plugins/TestPlugin/Assets")), FName(TEXT("/TestPlugin/Assets")) },
	};
	for (const FContentBrowserItemPath& Path : InternalPaths)
	{
		TestEqual(TEXT("A real internal path is preserved, not replaced by its virtual path"),
			GetContentBrowserInternalPath(Path), Path.GetInternalPathString());
	}

	return !HasAnyErrors();
}

#endif // WITH_DEV_AUTOMATION_TESTS
