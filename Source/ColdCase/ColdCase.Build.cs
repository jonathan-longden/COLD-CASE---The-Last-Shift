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

		// Interface pictures are loose image files, copied beside the packaged game.
		RuntimeDependencies.Add("$(ProjectDir)/Art/UI/...", StagedFileType.NonUFS);

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"InvestigationRuntime",
			"InputCore",
			"MediaAssets",
			"MediaUtils",
			"AudioMixer",
			"SignalProcessing",
			"Slate",
			"SlateCore"
		});
	}
}
