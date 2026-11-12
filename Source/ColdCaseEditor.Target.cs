// Build target for the Unreal Editor with our code loaded.
using UnrealBuildTool;

public class ColdCaseEditorTarget : TargetRules
{
	public ColdCaseEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("ColdCase");
	}
}
