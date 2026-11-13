#include "CaseDataLoader.h"

#include "CaseDataTypes.h"
#include "CaseTruthTypes.h"
#include "Interfaces/IPluginManager.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace CaseDataLoaderPrivate
{
	/** Finds an enabled plugin and reads one of its CaseData files into a string. */
	static bool ReadCaseFile(FName CasePluginName, const TCHAR* FileName, FString& OutJson, FString& OutError)
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(CasePluginName.ToString());
		if (!Plugin.IsValid() || !Plugin->IsEnabled())
		{
			OutError = FString::Printf(TEXT("Case plugin '%s' is not installed or not enabled."), *CasePluginName.ToString());
			return false;
		}

		const FString Path = FPaths::Combine(Plugin->GetBaseDir(), TEXT("CaseData"), FileName);
		if (!FFileHelper::LoadFileToString(OutJson, *Path))
		{
			OutError = FString::Printf(TEXT("Could not read '%s'."), *Path);
			return false;
		}
		return true;
	}

	template <typename StructType>
	static bool ParseCaseFile(FName CasePluginName, const TCHAR* FileName, StructType& OutStruct, FString& OutError)
	{
		FString Json;
		if (!ReadCaseFile(CasePluginName, FileName, Json, OutError))
		{
			return false;
		}

		OutStruct = StructType();
		if (!FJsonObjectConverter::JsonObjectStringToUStruct(Json, &OutStruct, 0, 0))
		{
			OutError = FString::Printf(TEXT("'%s' in case plugin '%s' is not valid JSON or does not match the expected layout."),
				FileName, *CasePluginName.ToString());
			return false;
		}

		if (OutStruct.SchemaVersion != GCaseContentSchemaVersion)
		{
			OutError = FString::Printf(TEXT("'%s' has schema version %d but this build expects %d."),
				FileName, OutStruct.SchemaVersion, GCaseContentSchemaVersion);
			return false;
		}
		return true;
	}
}

bool FCaseDataLoader::LoadContent(FName CasePluginName, FCaseContent& OutContent, FString& OutError)
{
	return CaseDataLoaderPrivate::ParseCaseFile(CasePluginName, TEXT("Case.json"), OutContent, OutError);
}

bool FCaseDataLoader::LoadTruth(FName CasePluginName, FCaseTruth& OutTruth, FString& OutError)
{
	return CaseDataLoaderPrivate::ParseCaseFile(CasePluginName, TEXT("Truth.json"), OutTruth, OutError);
}

FString FCaseDataLoader::GetMediaPath(FName CasePluginName, const FString& FileName)
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(CasePluginName.ToString());
	if (!Plugin.IsValid() || FileName.IsEmpty())
	{
		return FString();
	}
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(Plugin->GetBaseDir(), TEXT("Media"), FileName));
}
