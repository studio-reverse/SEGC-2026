using UnrealBuildTool;

public class SEGC2026Target : TargetRules
{
	public SEGC2026Target(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("SEGC2026");
	}
}
