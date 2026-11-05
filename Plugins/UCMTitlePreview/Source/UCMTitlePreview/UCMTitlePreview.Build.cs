using UnrealBuildTool;
public class UCMTitlePreview : ModuleRules {
 public UCMTitlePreview(ReadOnlyTargetRules Target):base(Target){
  PCHUsage=PCHUsageMode.UseExplicitOrSharedPCHs;
  PublicDependencyModuleNames.AddRange(new[]{"Core","CoreUObject","Engine","UMG","Slate","SlateCore","InputCore","LevelSequence","MovieScene","Json","Niagara","NiagaraCore"});
 }
}
