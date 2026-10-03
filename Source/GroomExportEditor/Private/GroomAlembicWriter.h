// Groom Alembic Exporter

#pragma once

#include "CoreMinimal.h"
#include "GroomExportSettings.h"

class UGroomAsset;

/** Summary of what a single export produced, for logging / user feedback. */
struct FGroomExportResult
{
	int32 NumGroups = 0;
	int32 NumCurves = 0;
	int32 NumPoints = 0;
	int32 NumGuideCurvesSkipped = 0;
	bool  bWroteWidths = false;
	bool  bWroteRootUVs = false;

	/** Populated when the export failed. */
	FText ErrorText;
};

/**
 * Writes the strand (curve) data of a UGroomAsset to an Alembic file that matches the
 * schema UE's own groom Alembic importer expects (see Engine/Plugins/Importers/
 * AlembicHairImporter/.../AlembicHairTranslator.cpp).
 *
 * Layout produced:
 *
 *   /groom                     OXform, identity, carries the groom-scope user properties
 *     /group_0                 OCurves, linear / non-periodic / no basis
 *     /group_1                 OCurves
 *     ...
 *
 * Each OCurves carries:
 *   - P            : point positions
 *   - nVertices    : point count per curve
 *   - width        : per-point (vertex scope) or per-curve (uniform scope), when available
 *   - uv           : root UV, uniform scope, when available
 *   - arbGeomParams: every "groom_*" strand/vertex attribute of the source groom
 *                    (groom_group_id, groom_id, groom_guide, groom_clumpid, groom_color, ...)
 */
class FGroomAlembicWriter
{
public:
	/** Editor-only: requires the groom's source FHairDescription. Returns false on failure. */
	static bool Export(
		const UGroomAsset* InGroomAsset,
		const FString& InFilename,
		const FGroomExportSettings& InSettings,
		FGroomExportResult& OutResult);
};
