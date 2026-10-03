// Groom Alembic Exporter

#include "GroomAlembicWriter.h"

#include "GroomAsset.h"
#include "HairAttributes.h"
#include "HairDescription.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"

#include <string>
#include <vector>

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#endif

THIRD_PARTY_INCLUDES_START
#include "Alembic/AbcGeom/All.h"
#include "Alembic/AbcCoreOgawa/All.h"
THIRD_PARTY_INCLUDES_END

#if PLATFORM_WINDOWS
#include "Windows/HideWindowsPlatformTypes.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogGroomExport, Log, All);

#define LOCTEXT_NAMESPACE "GroomExport"

namespace GroomExportPrivate
{
	using namespace Alembic::AbcGeom;

	/** Convert a groom-space (Unreal) position into the requested output convention. */
	FORCEINLINE Alembic::Abc::V3f ConvertPosition(const FVector3f& In, const FGroomExportSettings& Settings)
	{
		const float S = Settings.UnitScale;
		if (Settings.CoordSystem == EGroomExportCoordSystem::YUpRightHanded)
		{
			// Unreal (Z-up, left handed) -> Y-up right handed.
			// The Y/Z swap has determinant -1, which is exactly what a left-to-right handed
			// basis change requires: the groom keeps its shape and is not mirrored.
			return Alembic::Abc::V3f(In.X * S, In.Z * S, In.Y * S);
		}
		return Alembic::Abc::V3f(In.X * S, In.Y * S, In.Z * S);
	}

	/**
	 * Attributes that never become an arbGeomParam: either they have a dedicated Alembic
	 * channel, or they are redundant with the curve schema itself. Note this does not
	 * depend on the settings - when the user turns a channel off the attribute is dropped
	 * rather than silently re-appearing under its raw name.
	 */
	bool IsExplicitlyHandledAttribute(FName Name)
	{
		// Written as the OCurves "width" parameter.
		if (Name == HairAttribute::Strand::Width)
		{
			return true;
		}
		// Written as the OCurves "uv" parameter.
		if (Name == HairAttribute::Strand::RootUV)
		{
			return true;
		}
		// Curve/basis type are carried by the OCurves schema itself (linear / no basis).
		if (Name == HairAttribute::Strand::BasisType || Name == HairAttribute::Strand::CurveType)
		{
			return true;
		}
		// Per-strand knot arrays are meaningless for the linear curves we write, and a
		// TArrayAttribute cannot be expressed as a flat geom param.
		if (Name == HairAttribute::Strand::Knots)
		{
			return true;
		}
		return false;
	}

	/**
	 * A "groom_*" attribute of the source hair description that is re-emitted as an
	 * arbGeomParam. Attribute refs are resolved once, then sampled per group.
	 */
	struct FPassthroughAttribute
	{
		enum class EValueType : uint8 { Int, Float, Vector2, Vector3, String };

		FName Name;
		EValueType ValueType = EValueType::Int;
		bool bVertexScope = false;

		TStrandAttributesRef<int>       StrandInt;
		TStrandAttributesRef<float>     StrandFloat;
		TStrandAttributesRef<FVector2f> StrandVec2;
		TStrandAttributesRef<FVector3f> StrandVec3;
		TStrandAttributesRef<FName>     StrandName;

		TVertexAttributesRef<int>       VertexInt;
		TVertexAttributesRef<float>     VertexFloat;
		TVertexAttributesRef<FVector2f> VertexVec2;
		TVertexAttributesRef<FVector3f> VertexVec3;
	};

	/** Resolve every "groom_*" strand/vertex attribute we know how to write. */
	void GatherPassthroughAttributes(
		FHairDescription& HairDescription,
		TArray<FPassthroughAttribute>& Out)
	{
		// Matches the importer's filter (AlembicHairTranslatorUtils::IsAttributeValid).
		auto IsGroomAttribute = [](FName Name)
		{
			return Name.ToString().StartsWith(TEXT("groom_"));
		};

		{
			TArray<FName> Names;
			HairDescription.StrandAttributes().GetAttributeNames(Names);
			for (FName Name : Names)
			{
				if (!IsGroomAttribute(Name) || IsExplicitlyHandledAttribute(Name))
				{
					continue;
				}

				FPassthroughAttribute Attr;
				Attr.Name = Name;
				Attr.bVertexScope = false;

				Attr.StrandInt = HairDescription.StrandAttributes().GetAttributesRef<int>(Name);
				Attr.StrandFloat = HairDescription.StrandAttributes().GetAttributesRef<float>(Name);
				Attr.StrandVec2 = HairDescription.StrandAttributes().GetAttributesRef<FVector2f>(Name);
				Attr.StrandVec3 = HairDescription.StrandAttributes().GetAttributesRef<FVector3f>(Name);
				Attr.StrandName = HairDescription.StrandAttributes().GetAttributesRef<FName>(Name);

				if (Attr.StrandInt.IsValid())
				{
					Attr.ValueType = FPassthroughAttribute::EValueType::Int;
				}
				else if (Attr.StrandFloat.IsValid())
				{
					Attr.ValueType = FPassthroughAttribute::EValueType::Float;
				}
				else if (Attr.StrandVec2.IsValid())
				{
					Attr.ValueType = FPassthroughAttribute::EValueType::Vector2;
				}
				else if (Attr.StrandVec3.IsValid())
				{
					Attr.ValueType = FPassthroughAttribute::EValueType::Vector3;
				}
				else if (Attr.StrandName.IsValid())
				{
					Attr.ValueType = FPassthroughAttribute::EValueType::String;
				}
				else
				{
					UE_LOG(LogGroomExport, Verbose, TEXT("Skipping strand attribute '%s': unsupported value type."), *Name.ToString());
					continue;
				}

				Out.Add(MoveTemp(Attr));
			}
		}

		{
			TArray<FName> Names;
			HairDescription.VertexAttributes().GetAttributeNames(Names);
			for (FName Name : Names)
			{
				if (!IsGroomAttribute(Name))
				{
					continue;
				}
				// groom_width at vertex scope is written as the OCurves width parameter.
				if (Name == HairAttribute::Vertex::Width)
				{
					continue;
				}

				FPassthroughAttribute Attr;
				Attr.Name = Name;
				Attr.bVertexScope = true;

				Attr.VertexInt = HairDescription.VertexAttributes().GetAttributesRef<int>(Name);
				Attr.VertexFloat = HairDescription.VertexAttributes().GetAttributesRef<float>(Name);
				Attr.VertexVec2 = HairDescription.VertexAttributes().GetAttributesRef<FVector2f>(Name);
				Attr.VertexVec3 = HairDescription.VertexAttributes().GetAttributesRef<FVector3f>(Name);

				if (Attr.VertexInt.IsValid())
				{
					Attr.ValueType = FPassthroughAttribute::EValueType::Int;
				}
				else if (Attr.VertexFloat.IsValid())
				{
					Attr.ValueType = FPassthroughAttribute::EValueType::Float;
				}
				else if (Attr.VertexVec2.IsValid())
				{
					Attr.ValueType = FPassthroughAttribute::EValueType::Vector2;
				}
				else if (Attr.VertexVec3.IsValid())
				{
					Attr.ValueType = FPassthroughAttribute::EValueType::Vector3;
				}
				else
				{
					UE_LOG(LogGroomExport, Verbose, TEXT("Skipping vertex attribute '%s': unsupported value type."), *Name.ToString());
					continue;
				}

				Out.Add(MoveTemp(Attr));
			}
		}
	}

	/** Write groom-scope metadata as user properties on the root xform (groom spec 1.1+). */
	void WriteGroomUserProperties(OXform& Xform, FHairDescription& HairDescription)
	{
		OCompoundProperty UserProps = Xform.getSchema().getUserProperties();

		// Version of the groom Alembic spec this file follows.
		{
			Alembic::Abc::OInt32Property Major(UserProps, "groom_version_major");
			Major.set(1);
			Alembic::Abc::OInt32Property Minor(UserProps, "groom_version_minor");
			Minor.set(3);
		}
		{
			Alembic::Abc::OStringProperty Tool(UserProps, "groom_tool");
			Tool.set("UnrealEngine.GroomExport");
		}

		// Carry over any groom-scope attributes the source description had.
		TArray<FName> Names;
		HairDescription.GroomAttributes().GetAttributeNames(Names);
		for (FName Name : Names)
		{
			const FString NameStr = Name.ToString();
			if (!NameStr.StartsWith(TEXT("groom_")))
			{
				continue;
			}
			// Already written above.
			if (Name == HairAttribute::Groom::MajorVersion
				|| Name == HairAttribute::Groom::MinorVersion
				|| Name == HairAttribute::Groom::Tool)
			{
				continue;
			}

			const std::string PropName(TCHAR_TO_UTF8(*NameStr));

			TGroomAttributesRef<int> RefI = HairDescription.GroomAttributes().GetAttributesRef<int>(Name);
			TGroomAttributesRef<float> RefF = HairDescription.GroomAttributes().GetAttributesRef<float>(Name);
			TGroomAttributesRef<FName> RefN = HairDescription.GroomAttributes().GetAttributesRef<FName>(Name);
			TGroomAttributesRef<FVector3f> RefV = HairDescription.GroomAttributes().GetAttributesRef<FVector3f>(Name);

			if (RefI.IsValid())
			{
				Alembic::Abc::OInt32Property Prop(UserProps, PropName);
				Prop.set(RefI[FGroomID(0)]);
			}
			else if (RefF.IsValid())
			{
				Alembic::Abc::OFloatProperty Prop(UserProps, PropName);
				Prop.set(RefF[FGroomID(0)]);
			}
			else if (RefN.IsValid())
			{
				Alembic::Abc::OStringProperty Prop(UserProps, PropName);
				Prop.set(std::string(TCHAR_TO_UTF8(*RefN[FGroomID(0)].ToString())));
			}
			else if (RefV.IsValid())
			{
				const FVector3f V = RefV[FGroomID(0)];
				Alembic::Abc::OV3fProperty Prop(UserProps, PropName);
				Prop.set(Alembic::Abc::V3f(V.X, V.Y, V.Z));
			}
		}
	}
} // namespace GroomExportPrivate

bool FGroomAlembicWriter::Export(
	const UGroomAsset* InGroomAsset,
	const FString& InFilename,
	const FGroomExportSettings& InSettings,
	FGroomExportResult& OutResult)
{
#if WITH_EDITORONLY_DATA
	using namespace GroomExportPrivate;
	using namespace Alembic::AbcGeom;

	if (!InGroomAsset)
	{
		OutResult.ErrorText = LOCTEXT("NullGroom", "No groom asset supplied.");
		return false;
	}

	// The source description is the only lossless representation of the groom; the
	// per-platform bulk data is decimated and quantized.
	FHairDescription HairDescription = InGroomAsset->GetHairDescription();

	const int32 NumStrands = HairDescription.GetNumStrands();
	const int32 NumVertices = HairDescription.GetNumVertices();

	if (NumStrands <= 0 || NumVertices <= 0)
	{
		OutResult.ErrorText = LOCTEXT("NoDescription",
			"This groom has no source hair description, so its original strand data cannot be "
			"exported. That happens when the asset was cooked or its editor-only source data "
			"was stripped. Re-import the groom from its original file and export again.");
		return false;
	}

	TVertexAttributesRef<FVector3f> PositionRef =
		HairDescription.VertexAttributes().GetAttributesRef<FVector3f>(HairAttribute::Vertex::Position);
	TStrandAttributesRef<int> VertexCountRef =
		HairDescription.StrandAttributes().GetAttributesRef<int>(HairAttribute::Strand::VertexCount);

	if (!PositionRef.IsValid() || !VertexCountRef.IsValid())
	{
		OutResult.ErrorText = LOCTEXT("MissingCoreAttributes",
			"The groom's hair description is missing its position or vertex-count attribute.");
		return false;
	}

	// Optional attributes feeding the dedicated Alembic channels.
	TStrandAttributesRef<int>       GroupIDRef     = HairDescription.StrandAttributes().GetAttributesRef<int>(HairAttribute::Strand::GroupID);
	TStrandAttributesRef<int>       GuideRef       = HairDescription.StrandAttributes().GetAttributesRef<int>(HairAttribute::Strand::Guide);
	TStrandAttributesRef<float>     StrandWidthRef = HairDescription.StrandAttributes().GetAttributesRef<float>(HairAttribute::Strand::Width);
	TStrandAttributesRef<FVector2f> RootUVRef      = HairDescription.StrandAttributes().GetAttributesRef<FVector2f>(HairAttribute::Strand::RootUV);
	TVertexAttributesRef<float>     VertexWidthRef = HairDescription.VertexAttributes().GetAttributesRef<float>(HairAttribute::Vertex::Width);

	const bool bUseVertexWidth = InSettings.bExportWidths && VertexWidthRef.IsValid();
	const bool bUseStrandWidth = InSettings.bExportWidths && !bUseVertexWidth && StrandWidthRef.IsValid();
	const bool bUseRootUV      = InSettings.bExportRootUVs && RootUVRef.IsValid();

	// Vertices are stored consecutively per strand, in strand order - this is how the
	// importer fills the description (one AddVertex per curve point, in order).
	TArray<int32> StrandVertexOffset;
	StrandVertexOffset.SetNumUninitialized(NumStrands);
	{
		int32 Running = 0;
		for (int32 StrandIndex = 0; StrandIndex < NumStrands; ++StrandIndex)
		{
			StrandVertexOffset[StrandIndex] = Running;
			const int32 Count = VertexCountRef[FStrandID(StrandIndex)];
			if (Count < 2 || Running + Count > NumVertices)
			{
				OutResult.ErrorText = FText::Format(
					LOCTEXT("BadVertexCountFmt",
						"Strand {0} declares {1} vertices, which does not fit the description's {2} vertices. The groom's source data looks corrupt."),
					FText::AsNumber(StrandIndex), FText::AsNumber(Count), FText::AsNumber(NumVertices));
				return false;
			}
			Running += Count;
		}
	}

	// Bucket strands by group, preserving first-seen group order.
	TArray<int32> GroupIDs;
	TArray<TArray<int32>> GroupStrands;
	{
		TMap<int32, int32> GroupIDToIndex;
		for (int32 StrandIndex = 0; StrandIndex < NumStrands; ++StrandIndex)
		{
			if (!InSettings.bExportGuideCurves && GuideRef.IsValid() && GuideRef[FStrandID(StrandIndex)] != 0)
			{
				++OutResult.NumGuideCurvesSkipped;
				continue;
			}

			const int32 GroupID = (InSettings.bSplitByGroup && GroupIDRef.IsValid())
				? GroupIDRef[FStrandID(StrandIndex)]
				: 0;

			int32* ExistingIndex = GroupIDToIndex.Find(GroupID);
			if (!ExistingIndex)
			{
				const int32 NewIndex = GroupIDs.Add(GroupID);
				GroupStrands.AddDefaulted();
				ExistingIndex = &GroupIDToIndex.Add(GroupID, NewIndex);
			}
			GroupStrands[*ExistingIndex].Add(StrandIndex);
		}
	}

	if (GroupIDs.Num() == 0)
	{
		OutResult.ErrorText = LOCTEXT("NoCurvesToExport",
			"There are no curves left to export. If this groom only contains guides, enable 'Export guide curves'.");
		return false;
	}

	TArray<FPassthroughAttribute> PassthroughAttributes;
	if (InSettings.bExportGroomAttributes)
	{
		GatherPassthroughAttributes(HairDescription, PassthroughAttributes);
	}

	// Make sure the destination directory exists before Alembic tries to open the file.
	const FString AbsoluteFilename = FPaths::ConvertRelativePathToFull(InFilename);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(AbsoluteFilename), /*Tree*/ true);

	try
	{
		Alembic::Abc::OArchive Archive(
			Alembic::AbcCoreOgawa::WriteArchive(),
			TCHAR_TO_UTF8(*AbsoluteFilename),
			Alembic::Abc::ErrorHandler::kThrowPolicy);

		if (!Archive.valid())
		{
			OutResult.ErrorText = FText::Format(
				LOCTEXT("ArchiveOpenFailedFmt", "Could not create the Alembic archive '{0}'."),
				FText::FromString(AbsoluteFilename));
			return false;
		}

		Alembic::Abc::OObject TopObject = Archive.getTop();
		OXform Xform(TopObject, "groom");
		{
			// Identity transform; positions are already baked into the requested space.
			XformSample IdentitySample;
			Xform.getSchema().set(IdentitySample);
		}
		WriteGroomUserProperties(Xform, HairDescription);

		for (int32 GroupIndex = 0; GroupIndex < GroupIDs.Num(); ++GroupIndex)
		{
			const TArray<int32>& Strands = GroupStrands[GroupIndex];
			if (Strands.Num() == 0)
			{
				continue;
			}

			int32 GroupPointCount = 0;
			for (int32 StrandIndex : Strands)
			{
				GroupPointCount += VertexCountRef[FStrandID(StrandIndex)];
			}

			std::vector<Alembic::Abc::V3f> Positions;
			Positions.reserve(GroupPointCount);
			std::vector<int32_t> NVertices;
			NVertices.reserve(Strands.Num());
			std::vector<float> Widths;
			std::vector<Alembic::Abc::V2f> UVs;

			if (bUseVertexWidth)      { Widths.reserve(GroupPointCount); }
			else if (bUseStrandWidth) { Widths.reserve(Strands.Num()); }
			if (bUseRootUV)           { UVs.reserve(Strands.Num()); }

			Alembic::Abc::Box3d Bounds;
			Bounds.makeEmpty();

			for (int32 StrandIndex : Strands)
			{
				const int32 Count = VertexCountRef[FStrandID(StrandIndex)];
				const int32 Offset = StrandVertexOffset[StrandIndex];

				NVertices.push_back(static_cast<int32_t>(Count));

				for (int32 PointIndex = 0; PointIndex < Count; ++PointIndex)
				{
					const FVertexID VertexID(Offset + PointIndex);
					const Alembic::Abc::V3f P = ConvertPosition(PositionRef[VertexID], InSettings);
					Positions.push_back(P);
					Bounds.extendBy(Alembic::Abc::V3d(P.x, P.y, P.z));

					if (bUseVertexWidth)
					{
						Widths.push_back(VertexWidthRef[VertexID] * InSettings.UnitScale);
					}
				}

				if (bUseStrandWidth)
				{
					Widths.push_back(StrandWidthRef[FStrandID(StrandIndex)] * InSettings.UnitScale);
				}
				if (bUseRootUV)
				{
					const FVector2f UV = RootUVRef[FStrandID(StrandIndex)];
					UVs.push_back(Alembic::Abc::V2f(UV.X, UV.Y));
				}
			}

			const FString CurvesName = InSettings.bSplitByGroup
				? FString::Printf(TEXT("group_%d"), GroupIDs[GroupIndex])
				: FString(TEXT("curves"));

			OCurves Curves(Xform, TCHAR_TO_UTF8(*CurvesName));
			OCurvesSchema& Schema = Curves.getSchema();

			OCurvesSchema::Sample Sample;
			Sample.setPositions(Alembic::Abc::P3fArraySample(Positions.data(), Positions.size()));
			Sample.setCurvesNumVertices(Alembic::Abc::Int32ArraySample(NVertices.data(), NVertices.size()));
			// Hair strands are polylines: linear, open, and without a spline basis.
			Sample.setType(kLinear);
			Sample.setWrap(kNonPeriodic);
			Sample.setBasis(kNoBasis);
			Sample.setSelfBounds(Bounds);

			if (!Widths.empty())
			{
				Sample.setWidths(OFloatGeomParam::Sample(
					Alembic::Abc::FloatArraySample(Widths.data(), Widths.size()),
					bUseVertexWidth ? kVertexScope : kUniformScope));
				OutResult.bWroteWidths = true;
			}
			if (!UVs.empty())
			{
				Sample.setUVs(OV2fGeomParam::Sample(
					Alembic::Abc::V2fArraySample(UVs.data(), UVs.size()),
					kUniformScope));
				OutResult.bWroteRootUVs = true;
			}

			Schema.set(Sample);

			// Everything else the groom carried, re-emitted verbatim as arbGeomParams so a
			// re-import restores the same hair description attributes.
			if (PassthroughAttributes.Num() > 0)
			{
				OCompoundProperty ArbParams = Schema.getArbGeomParams();

				for (FPassthroughAttribute& Attr : PassthroughAttributes)
				{
					const std::string ParamName(TCHAR_TO_UTF8(*Attr.Name.ToString()));
					const GeometryScope Scope = Attr.bVertexScope ? kVertexScope : kUniformScope;
					const int32 NumValues = Attr.bVertexScope ? GroupPointCount : Strands.Num();

					// Walk the group's elements in the same order the positions were written.
					auto ForEachElement = [&](TFunctionRef<void(int32 /*ElementIndex*/)> Visit)
					{
						if (Attr.bVertexScope)
						{
							for (int32 StrandIndex : Strands)
							{
								const int32 Count = VertexCountRef[FStrandID(StrandIndex)];
								const int32 Offset = StrandVertexOffset[StrandIndex];
								for (int32 PointIndex = 0; PointIndex < Count; ++PointIndex)
								{
									Visit(Offset + PointIndex);
								}
							}
						}
						else
						{
							for (int32 StrandIndex : Strands)
							{
								Visit(StrandIndex);
							}
						}
					};

					switch (Attr.ValueType)
					{
					case FPassthroughAttribute::EValueType::Int:
					{
						std::vector<int32_t> Values;
						Values.reserve(NumValues);
						ForEachElement([&](int32 Index)
						{
							Values.push_back(static_cast<int32_t>(Attr.bVertexScope
								? Attr.VertexInt[FVertexID(Index)]
								: Attr.StrandInt[FStrandID(Index)]));
						});
						OInt32GeomParam Param(ArbParams, ParamName, /*isIndexed*/ false, Scope, 1);
						Param.set(OInt32GeomParam::Sample(
							Alembic::Abc::Int32ArraySample(Values.data(), Values.size()), Scope));
						break;
					}
					case FPassthroughAttribute::EValueType::Float:
					{
						std::vector<float> Values;
						Values.reserve(NumValues);
						ForEachElement([&](int32 Index)
						{
							Values.push_back(Attr.bVertexScope
								? Attr.VertexFloat[FVertexID(Index)]
								: Attr.StrandFloat[FStrandID(Index)]);
						});
						OFloatGeomParam Param(ArbParams, ParamName, false, Scope, 1);
						Param.set(OFloatGeomParam::Sample(
							Alembic::Abc::FloatArraySample(Values.data(), Values.size()), Scope));
						break;
					}
					case FPassthroughAttribute::EValueType::Vector2:
					{
						std::vector<Alembic::Abc::V2f> Values;
						Values.reserve(NumValues);
						ForEachElement([&](int32 Index)
						{
							const FVector2f V = Attr.bVertexScope
								? Attr.VertexVec2[FVertexID(Index)]
								: Attr.StrandVec2[FStrandID(Index)];
							Values.push_back(Alembic::Abc::V2f(V.X, V.Y));
						});
						OV2fGeomParam Param(ArbParams, ParamName, false, Scope, 1);
						Param.set(OV2fGeomParam::Sample(
							Alembic::Abc::V2fArraySample(Values.data(), Values.size()), Scope));
						break;
					}
					case FPassthroughAttribute::EValueType::Vector3:
					{
						std::vector<Alembic::Abc::V3f> Values;
						Values.reserve(NumValues);
						ForEachElement([&](int32 Index)
						{
							const FVector3f V = Attr.bVertexScope
								? Attr.VertexVec3[FVertexID(Index)]
								: Attr.StrandVec3[FStrandID(Index)];
							Values.push_back(Alembic::Abc::V3f(V.X, V.Y, V.Z));
						});
						OV3fGeomParam Param(ArbParams, ParamName, false, Scope, 1);
						Param.set(OV3fGeomParam::Sample(
							Alembic::Abc::V3fArraySample(Values.data(), Values.size()), Scope));
						break;
					}
					case FPassthroughAttribute::EValueType::String:
					{
						// Only strand-scope FName attributes reach here.
						std::vector<std::string> Values;
						Values.reserve(NumValues);
						ForEachElement([&](int32 Index)
						{
							Values.push_back(std::string(TCHAR_TO_UTF8(*Attr.StrandName[FStrandID(Index)].ToString())));
						});
						OStringGeomParam Param(ArbParams, ParamName, false, Scope, 1);
						Param.set(OStringGeomParam::Sample(
							Alembic::Abc::StringArraySample(Values.data(), Values.size()), Scope));
						break;
					}
					}
				}
			}

			OutResult.NumCurves += Strands.Num();
			OutResult.NumPoints += GroupPointCount;
			++OutResult.NumGroups;
		}
	}
	catch (const std::exception& Exception)
	{
		OutResult.ErrorText = FText::Format(
			LOCTEXT("AlembicExceptionFmt", "Alembic failed while writing '{0}': {1}"),
			FText::FromString(AbsoluteFilename),
			FText::FromString(FString(ANSI_TO_TCHAR(Exception.what()))));
		return false;
	}

	UE_LOG(LogGroomExport, Log,
		TEXT("Exported '%s' to '%s': %d group(s), %d curve(s), %d point(s)%s."),
		*InGroomAsset->GetName(), *AbsoluteFilename,
		OutResult.NumGroups, OutResult.NumCurves, OutResult.NumPoints,
		OutResult.NumGuideCurvesSkipped > 0 ? TEXT(" (guides skipped)") : TEXT(""));

	return true;
#else
	OutResult.ErrorText = LOCTEXT("EditorOnly", "Groom export is only available in editor builds.");
	return false;
#endif
}

#undef LOCTEXT_NAMESPACE
