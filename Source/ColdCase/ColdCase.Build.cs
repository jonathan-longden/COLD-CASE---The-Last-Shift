// The thin game module: game mode, player, startup. No investigation rules live here.
using UnrealBuildTool;

public class ColdCase : ModuleRules
{
	public ColdCase(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"InvestigationRuntime"
		});
	}
}
