// Groom Alembic Exporter

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GroomExportLibrary.generated.h"

class UGroomAsset;

/**
 * Scripting entry point for the groom Alembic exporter, so grooms can be exported from
 * Blueprint, Python or a commandlet without going through the Content Browser menu.
 */
UCLASS()
class GROOMEXPORTEDITOR_API UGroomExportBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Writes a groom's strand curves to an Alembic file.
	 *
	 * @param GroomAsset          The groom to export.
	 * @param Filename            Absolute path of the .abc file to write.
	 * @param OutError            Empty on success, otherwise why the export failed.
	 * @param OutNumGroups        Number of OCurves objects written.
	 * @param OutNumCurves        Total curves written.
	 * @param OutNumPoints        Total points written.
	 * @param bConvertToYUp       Convert to the Y-up right-handed convention used by Maya
	 *                            and Blender. Leave false to keep Unreal space, which is
	 *                            what round-trips through the groom importer's defaults.
	 * @param UnitScale           Uniform scale on positions and widths. 1.0 keeps centimeters.
	 * @param bExportGuideCurves  Include curves flagged as simulation guides.
	 */
	/**
	 * True when this plugin's "Export Groom to Alembic" entry is present in the Groom Asset
	 * context menu. Only meaningful in a GUI editor session - a commandlet never builds the
	 * Content Browser menus.
	 *
	 * Exists because the menu's sections are not readable from Python, so this is the only
	 * way to check the registration without clicking through the UI by hand.
	 */
	UFUNCTION(BlueprintCallable, Category = "Groom Export")
	static bool IsExportMenuEntryRegistered();

	UFUNCTION(BlueprintCallable, Category = "Groom Export")
	static bool ExportGroomToAlembic(
		UGroomAsset* GroomAsset,
		const FString& Filename,
		FString& OutError,
		int32& OutNumGroups,
		int32& OutNumCurves,
		int32& OutNumPoints,
		bool bConvertToYUp = false,
		float UnitScale = 1.0f,
		bool bExportGuideCurves = true);
};
