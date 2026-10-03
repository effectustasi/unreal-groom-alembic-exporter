// Groom Alembic Exporter

#include "GroomExportLibrary.h"

#include "GroomAlembicWriter.h"
#include "GroomExportSettings.h"

#include "ToolMenu.h"
#include "ToolMenuSection.h"
#include "ToolMenus.h"

bool UGroomExportBlueprintLibrary::IsExportMenuEntryRegistered()
{
	UToolMenus* Menus = UToolMenus::Get();
	if (!Menus)
	{
		return false;
	}

	UToolMenu* Menu = Menus->FindMenu(TEXT("ContentBrowser.AssetContextMenu.GroomAsset"));
	if (!Menu)
	{
		return false;
	}

	FToolMenuSection* Section = Menu->FindSection(TEXT("GetAssetActions"));
	if (!Section)
	{
		return false;
	}

	// The name given to AddDynamicEntry in GroomExportEditor.cpp. The menu entry it builds
	// ("GroomAsset_ExportToAlembic") only materialises when the menu is generated with a
	// selection, so the dynamic block is what can be checked ahead of time.
	return Section->FindEntry(TEXT("GroomExport_Alembic")) != nullptr;
}

bool UGroomExportBlueprintLibrary::ExportGroomToAlembic(
	UGroomAsset* GroomAsset,
	const FString& Filename,
	FString& OutError,
	int32& OutNumGroups,
	int32& OutNumCurves,
	int32& OutNumPoints,
	bool bConvertToYUp,
	float UnitScale,
	bool bExportGuideCurves)
{
	OutError.Reset();
	OutNumGroups = 0;
	OutNumCurves = 0;
	OutNumPoints = 0;

	FGroomExportSettings Settings;
	Settings.CoordSystem = bConvertToYUp ? EGroomExportCoordSystem::YUpRightHanded : EGroomExportCoordSystem::UnrealZUp;
	Settings.UnitScale = UnitScale;
	Settings.bExportGuideCurves = bExportGuideCurves;

	FGroomExportResult Result;
	const bool bSuccess = FGroomAlembicWriter::Export(GroomAsset, Filename, Settings, Result);

	OutNumGroups = Result.NumGroups;
	OutNumCurves = Result.NumCurves;
	OutNumPoints = Result.NumPoints;
	if (!bSuccess)
	{
		OutError = Result.ErrorText.ToString();
	}

	return bSuccess;
}
