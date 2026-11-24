using UnrealBuildTool;

public class ProperFootballEditorTarget : TargetRules
{
	public ProperFootballEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("ProperFootball");
	}
}
