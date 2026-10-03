// Groom Alembic Exporter

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

/**
 * Editor module that adds "Export Groom to Alembic (.abc)..." to the Content Browser
 * context menu of Groom Assets.
 */
class FGroomExportEditorModule : public IModuleInterface
{
public:
	//~ Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface
};
