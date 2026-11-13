// This module has no gameplay code. It exists so the case's text files (CaseData folder)
// are copied into packaged builds. When case content moves into Data Assets it can go.
using UnrealBuildTool;

public class Case002_NinthFloor : ModuleRules
{
	public Case002_NinthFloor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core" });

		RuntimeDependencies.Add("$(PluginDir)/CaseData/...", StagedFileType.UFS);
	}
}
