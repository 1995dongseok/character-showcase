using UnrealBuildTool;

public class CharacterShowcaseTarget : TargetRules
{
	public CharacterShowcaseTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		ExtraModuleNames.Add("CharacterShowcase");
	}
}
