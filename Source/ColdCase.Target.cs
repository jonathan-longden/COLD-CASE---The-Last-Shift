// Build target for the game itself (what ends up in the packaged .exe).
using UnrealBuildTool;

public class ColdCaseTarget : TargetRules
{
	public ColdCaseTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("ColdCase");
	}
}
