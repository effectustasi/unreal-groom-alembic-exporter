// Groom Alembic Exporter

#pragma once

#include "CoreMinimal.h"
#include "GroomExportSettings.h"

namespace GroomExportOptionsDialog
{
	enum class EResult : uint8
	{
		Export,
		Cancel
	};

	/**
	 * Shows the modal export options dialog.
	 *
	 * @param InOutSettings   Seeded with the defaults to show; filled with the user's choices.
	 * @param bOutApplyToAll  Set when the user ticked "apply to all selected grooms".
	 * @param InNumAssets     Number of grooms in the current selection (drives the "apply to all" option).
	 */
	EResult Show(FGroomExportSettings& InOutSettings, bool& bOutApplyToAll, int32 InNumAssets);
}
