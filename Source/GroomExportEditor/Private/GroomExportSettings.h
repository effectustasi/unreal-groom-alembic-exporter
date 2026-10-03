// Groom Alembic Exporter

#pragma once

#include "CoreMinimal.h"

/** Target coordinate convention for the written Alembic file. */
enum class EGroomExportCoordSystem : uint8
{
	/**
	 * Write positions exactly as they are stored in the Groom Asset (Unreal space:
	 * Z-up, left handed, centimeters). This is the round-trip safe option: re-importing
	 * the file with the default groom import conversion settings (Rotation 0,0,0 /
	 * Scale 1,1,1) reproduces the original groom.
	 */
	UnrealZUp,

	/**
	 * Convert to the Y-up right handed convention used by Maya / Blender / Houdini:
	 * (X, Y, Z)_unreal -> (X, Z, Y)_alembic.
	 *
	 * The axis swap has determinant -1, which is exactly what is required when moving
	 * between a left handed and a right handed basis: the geometry keeps its shape and
	 * is NOT mirrored.
	 *
	 * Note: to re-import such a file into Unreal you must set the groom importer's
	 * conversion settings to Rotation (90, 0, 0) / Scale (1, 1, 1) - it will not
	 * round-trip with the default identity settings.
	 */
	YUpRightHanded
};

/** User facing options for a single groom -> .abc export. */
struct FGroomExportSettings
{
	/** Coordinate convention of the written file. */
	EGroomExportCoordSystem CoordSystem = EGroomExportCoordSystem::UnrealZUp;

	/**
	 * Uniform scale applied to positions (and widths) on the way out.
	 * Groom assets store centimeters, and UE's groom importer treats Alembic units as
	 * centimeters too (its UNIT_TO_CM is 1), so 1.0 keeps the file in centimeters.
	 * Use 0.01 to write meters.
	 */
	float UnitScale = 1.0f;

	/** Write one OCurves object per hair group instead of a single OCurves for the whole groom. */
	bool bSplitByGroup = true;

	/** Write the per-point (or per-curve) width parameter, when the groom has widths. */
	bool bExportWidths = true;

	/** Write the root UVs as the OCurves UV parameter (uniform scope), when present. */
	bool bExportRootUVs = true;

	/** Write groom_group_id / groom_id / groom_guide / groom_clumpid arbGeomParams. */
	bool bExportGroomAttributes = true;

	/** Also export the curves flagged as guides (groom_guide == 1). */
	bool bExportGuideCurves = true;
};
