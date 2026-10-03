// Groom Alembic Exporter

namespace UnrealBuildTool.Rules
{
	public class GroomExportEditor : ModuleRules
	{
		public GroomExportEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					// Third party Alembic (headers + static lib). Mirrors
					// Engine/Plugins/Importers/AlembicHairImporter/.../AlembicHairTranslatorModule.Build.cs
					"Imath",
					"AlembicLib",

					"Core",
					"CoreUObject",
					"Engine",
					"InputCore",
					"Slate",
					"SlateCore",
					"EditorStyle",
					"UnrealEd",
					"ToolMenus",
					"ContentBrowser",
					"DesktopPlatform",
					"Projects",

					// Groom
					"HairStrandsCore",
					"MeshDescription"
				});
		}
	}
}
