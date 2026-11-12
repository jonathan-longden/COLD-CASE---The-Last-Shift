// Runtime half of the investigation framework. Everything here must stay case-agnostic.
using UnrealBuildTool;

public class InvestigationRuntime : ModuleRules
{
	public InvestigationRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"DeveloperSettings"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Json",
			"JsonUtilities",
			"Projects"
		});
	}
}
