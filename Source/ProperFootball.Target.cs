using UnrealBuildTool;

public class ProperFootballTarget : TargetRules
{
	public ProperFootballTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("ProperFootball");
	}
}
