// Groom Alembic Exporter

#include "SGroomExportOptionsDialog.h"

#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "GroomExport"

namespace GroomExportOptionsDialog
{
	namespace Private
	{
		/** Slate contents of the modal options window. */
		class SOptionsPanel : public SCompoundWidget
		{
		public:
			SLATE_BEGIN_ARGS(SOptionsPanel) {}
				SLATE_ARGUMENT(TSharedPtr<SWindow>, ParentWindow)
				SLATE_ARGUMENT(FGroomExportSettings, InitialSettings)
				SLATE_ARGUMENT(int32, NumAssets)
			SLATE_END_ARGS()

			void Construct(const FArguments& InArgs)
			{
				ParentWindow = InArgs._ParentWindow;
				Settings = InArgs._InitialSettings;
				NumAssets = InArgs._NumAssets;
				bApplyToAll = NumAssets > 1;
				bConfirmed = false;

				TSharedRef<SVerticalBox> Options = SNew(SVerticalBox);

				Options->AddSlot().AutoHeight().Padding(0.f, 2.f)
				[
					MakeCheckBox(
						LOCTEXT("YUp", "Convert to Y-up (Maya / Blender)"),
						LOCTEXT("YUpTip",
							"Off (default): positions are written exactly as Unreal stores them (Z-up, centimeters). "
							"This is the round-trip safe option - re-importing the file with the groom importer's "
							"default conversion settings reproduces the original groom.\n\n"
							"On: swaps Y and Z to the Y-up right-handed convention Maya, Blender and Houdini use. "
							"The groom keeps its shape and is not mirrored, but re-importing it into Unreal then "
							"needs the importer's conversion rotation set to (90, 0, 0)."),
						TAttribute<ECheckBoxState>::CreateLambda([this]()
						{
							return Settings.CoordSystem == EGroomExportCoordSystem::YUpRightHanded
								? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
						}),
						FOnCheckStateChanged::CreateLambda([this](ECheckBoxState NewState)
						{
							Settings.CoordSystem = (NewState == ECheckBoxState::Checked)
								? EGroomExportCoordSystem::YUpRightHanded
								: EGroomExportCoordSystem::UnrealZUp;
						}))
				];

				Options->AddSlot().AutoHeight().Padding(0.f, 6.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Scale", "Unit scale"))
						.ToolTipText(LOCTEXT("ScaleTip",
							"Uniform scale applied to positions and widths. Groom assets store centimeters and "
							"Unreal's groom importer reads Alembic units as centimeters, so leave this at 1.0 for a "
							"round trip. Use 0.01 to write the file in meters."))
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[
						SNew(SSpinBox<float>)
						.MinValue(0.000001f)
						.MaxValue(10000.f)
						.MinSliderValue(0.01f)
						.MaxSliderValue(100.f)
						.Value_Lambda([this]() { return Settings.UnitScale; })
						.OnValueChanged_Lambda([this](float NewValue) { Settings.UnitScale = NewValue; })
					]
				];

				AddSimpleCheckBox(Options,
					LOCTEXT("SplitByGroup", "One curves object per hair group"),
					LOCTEXT("SplitByGroupTip",
						"Writes a separate OCurves object per groom group (group_0, group_1, ...). "
						"Turn off to write every strand into a single OCurves object."),
					Settings.bSplitByGroup);

				AddSimpleCheckBox(Options,
					LOCTEXT("Widths", "Export widths"),
					LOCTEXT("WidthsTip", "Writes the groom's strand widths as the Alembic width parameter, when the groom has them."),
					Settings.bExportWidths);

				AddSimpleCheckBox(Options,
					LOCTEXT("RootUVs", "Export root UVs"),
					LOCTEXT("RootUVsTip", "Writes the per-strand root UVs as the Alembic uv parameter, when the groom has them."),
					Settings.bExportRootUVs);

				AddSimpleCheckBox(Options,
					LOCTEXT("GroomAttrs", "Export groom_* attributes"),
					LOCTEXT("GroomAttrsTip",
						"Writes the groom's remaining attributes (groom_group_id, groom_id, groom_guide, "
						"groom_clumpid, groom_color, ...) as arbitrary geometry parameters. Needed for a faithful "
						"round trip back into Unreal."),
					Settings.bExportGroomAttributes);

				AddSimpleCheckBox(Options,
					LOCTEXT("Guides", "Export guide curves"),
					LOCTEXT("GuidesTip",
						"Includes the curves flagged as simulation guides (groom_guide). Turn off to export only "
						"the render strands."),
					Settings.bExportGuideCurves);

				if (NumAssets > 1)
				{
					Options->AddSlot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)
					[
						MakeCheckBox(
							FText::Format(LOCTEXT("ApplyAllFmt", "Use these settings for all {0} selected grooms"), FText::AsNumber(NumAssets)),
							LOCTEXT("ApplyAllTip", "Skips this dialog for the remaining grooms in the selection."),
							TAttribute<ECheckBoxState>::CreateLambda([this]()
							{
								return bApplyToAll ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
							}),
							FOnCheckStateChanged::CreateLambda([this](ECheckBoxState NewState)
							{
								bApplyToAll = (NewState == ECheckBoxState::Checked);
							}))
					];
				}

				ChildSlot
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
					.Padding(12.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							Options
						]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.f, 16.f, 0.f, 0.f)
						[
							SNew(SUniformGridPanel)
							.SlotPadding(FAppStyle::GetMargin("StandardDialog.SlotPadding"))
							.MinDesiredSlotWidth(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
							.MinDesiredSlotHeight(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
							+ SUniformGridPanel::Slot(0, 0)
							[
								SNew(SButton)
								.HAlign(HAlign_Center)
								.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
								.Text(LOCTEXT("ExportButton", "Export"))
								.OnClicked(this, &SOptionsPanel::OnConfirm)
							]
							+ SUniformGridPanel::Slot(1, 0)
							[
								SNew(SButton)
								.HAlign(HAlign_Center)
								.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
								.Text(LOCTEXT("CancelButton", "Cancel"))
								.OnClicked(this, &SOptionsPanel::OnCancel)
							]
						]
					]
				];
			}

			bool WasConfirmed() const { return bConfirmed; }
			bool ShouldApplyToAll() const { return bApplyToAll; }
			const FGroomExportSettings& GetSettings() const { return Settings; }

		private:
			TSharedRef<SWidget> MakeCheckBox(
				const FText& Label,
				const FText& ToolTip,
				TAttribute<ECheckBoxState> IsChecked,
				FOnCheckStateChanged OnChanged)
			{
				return SNew(SCheckBox)
					.IsChecked(IsChecked)
					.OnCheckStateChanged(OnChanged)
					.ToolTipText(ToolTip)
					[
						SNew(STextBlock).Text(Label).ToolTipText(ToolTip)
					];
			}

			/** Adds a checkbox bound directly to a bool member of Settings. */
			void AddSimpleCheckBox(TSharedRef<SVerticalBox> Container, const FText& Label, const FText& ToolTip, bool& Value)
			{
				bool* ValuePtr = &Value;
				Container->AddSlot().AutoHeight().Padding(0.f, 2.f)
				[
					MakeCheckBox(Label, ToolTip,
						TAttribute<ECheckBoxState>::CreateLambda([ValuePtr]()
						{
							return *ValuePtr ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
						}),
						FOnCheckStateChanged::CreateLambda([ValuePtr](ECheckBoxState NewState)
						{
							*ValuePtr = (NewState == ECheckBoxState::Checked);
						}))
				];
			}

			FReply OnConfirm()
			{
				bConfirmed = true;
				CloseWindow();
				return FReply::Handled();
			}

			FReply OnCancel()
			{
				bConfirmed = false;
				CloseWindow();
				return FReply::Handled();
			}

			void CloseWindow()
			{
				if (TSharedPtr<SWindow> Window = ParentWindow.Pin())
				{
					Window->RequestDestroyWindow();
				}
			}

			TWeakPtr<SWindow> ParentWindow;
			FGroomExportSettings Settings;
			int32 NumAssets = 1;
			bool bApplyToAll = false;
			bool bConfirmed = false;
		};
	} // namespace Private

	EResult Show(FGroomExportSettings& InOutSettings, bool& bOutApplyToAll, int32 InNumAssets)
	{
		TSharedRef<SWindow> Window = SNew(SWindow)
			.Title(LOCTEXT("DialogTitle", "Export Groom to Alembic"))
			.SizingRule(ESizingRule::Autosized)
			.SupportsMinimize(false)
			.SupportsMaximize(false);

		TSharedRef<Private::SOptionsPanel> Panel = SNew(Private::SOptionsPanel)
			.ParentWindow(Window)
			.InitialSettings(InOutSettings)
			.NumAssets(InNumAssets);

		Window->SetContent(Panel);

		if (GEditor)
		{
			GEditor->EditorAddModalWindow(Window);
		}
		else
		{
			FSlateApplication::Get().AddModalWindow(Window, nullptr);
		}

		if (!Panel->WasConfirmed())
		{
			return EResult::Cancel;
		}

		InOutSettings = Panel->GetSettings();
		bOutApplyToAll = Panel->ShouldApplyToAll();
		return EResult::Export;
	}
} // namespace GroomExportOptionsDialog

#undef LOCTEXT_NAMESPACE
