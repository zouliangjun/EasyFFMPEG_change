using System.IO;
using UnrealBuildTool;

public class EasyFFMPEG : ModuleRules
{
    private string ModulePath
    {
        get { return ModuleDirectory; }
    }

    private string ThirdPartyPath
    {
        get { return Path.GetFullPath(Path.Combine(ModulePath, "../../ThirdParty/")); }
    }

    private string UProjectPath
    {
        get { return Directory.GetParent(ModulePath).Parent.FullName; }
    }
    private void CopyToBinaries(string FilePath, ReadOnlyTargetRules Type)
    {
        string binariesDir = Path.Combine(UProjectPath, "Binaries", Target.Platform.ToString());
        string filename = Path.GetFileName(FilePath);

        System.Console.WriteLine("Writing file " + FilePath + " to " + binariesDir);

        if (!Directory.Exists(binariesDir))
        {
            Directory.CreateDirectory(binariesDir);
        }

        if (!File.Exists(Path.Combine(binariesDir, filename)))
        {
            File.Copy(FilePath, Path.Combine(binariesDir, filename), true);
        }
    }
    public bool LoadFFmpeg(ReadOnlyTargetRules Target)
    {
        bool isLibrarySupported = false;

        if ((Target.Platform == UnrealTargetPlatform.Win64))
        {
            isLibrarySupported = true;

            string PlatformString = "x64";
            string LibrariesPath = Path.Combine(Path.Combine(Path.Combine(ThirdPartyPath, "ffmpeg", "libs"), "vs"), "x64");


            System.Console.WriteLine("... LibrariesPath -> " + LibrariesPath);

            PublicAdditionalLibraries.Add(Path.Combine(LibrariesPath, "avcodec.lib"));
            PublicAdditionalLibraries.Add(Path.Combine(LibrariesPath, "avdevice.lib"));
            PublicAdditionalLibraries.Add(Path.Combine(LibrariesPath, "avfilter.lib"));
            PublicAdditionalLibraries.Add(Path.Combine(LibrariesPath, "avformat.lib"));
            PublicAdditionalLibraries.Add(Path.Combine(LibrariesPath, "avutil.lib"));
            PublicAdditionalLibraries.Add(Path.Combine(LibrariesPath, "swresample.lib"));
            PublicAdditionalLibraries.Add(Path.Combine(LibrariesPath, "swscale.lib"));

            string[] dlls = { "avcodec-58.dll", "avdevice-58.dll", "avfilter-7.dll", "avformat-58.dll", "avutil-56.dll", "swresample-3.dll", "swscale-5.dll", "postproc-55.dll" };

            string BinariesPath = Path.Combine(Path.Combine(Path.Combine(ThirdPartyPath, "ffmpeg", "bin"), "vs"), "x64");
            foreach (string dll in dlls)
            {
                PublicDelayLoadDLLs.Add(dll);
                CopyToBinaries(Path.Combine(BinariesPath, dll), Target);
                RuntimeDependencies.Add(Path.Combine(BinariesPath, dll), StagedFileType.NonUFS);
            }

        }
        else if (Target.Platform == UnrealTargetPlatform.Android)
        {
            isLibrarySupported = true;

            string LibrariesPath = Path.Combine(Path.Combine(ThirdPartyPath, "ffmpeg", "libs"), "android");
            string[] Platforms = { "armeabi-v7a"
                                , "arm64-v8a"
                                //, "x86"
                                , "x86_64" 
            };


            string[] libs = { "libavcodec.so", "libavdevice.so", "libavfilter.so", "libavformat.so", "libavutil.so", "libswresample.so", "libswscale.so" };

            System.Console.WriteLine("Architecture: " + Target);


            foreach (string platform in Platforms)
            {
                foreach (string lib in libs)
                {
                    PublicAdditionalLibraries.Add(Path.Combine(Path.Combine(LibrariesPath, platform), lib));
                }
            }

            string finalPath = Path.Combine(ModulePath, "EasyFFMPEG_APL.xml");
            System.Console.WriteLine("... APL Path -> " + finalPath);
            AdditionalPropertiesForReceipt.Add("AndroidPlugin", finalPath);

        }

        if (isLibrarySupported)
        {
            // Include path
            PublicIncludePaths.Add(Path.Combine(ThirdPartyPath, "ffmpeg", "include"));
        }


        return isLibrarySupported;
    }
    public  EasyFFMPEG  (ReadOnlyTargetRules Target) : base(Target)

    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.AddRange(
            new string[] {
				// ... add public include paths required here ...
			}
            );


        PrivateIncludePaths.AddRange(
            new string[] {
				// ... add other private include paths required here ...
			}
            );


        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
				// ... add other public dependencies that you statically link with here ...
			}
            );


        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "RenderCore",
                "RHI",
                "Projects",
                "MovieSceneCapture",
               // "Android",

                "MediaUtils",
                //"launch",
				// ... add private dependencies that you statically link with here ...	
			}
            );
            if (Target.Platform == UnrealTargetPlatform.Android)
            {
                PrivateDependencyModuleNames.AddRange(
                    new string[]
                    {
                        "ApplicationCore",
                        "Launch"
                    }
                );
            }


        DynamicallyLoadedModuleNames.AddRange(
            new string[]
            {
				// ... add any modules that your module loads dynamically here ...
			}
            );

        if (Target.Type == TargetRules.TargetType.Editor)
        {
            PrivateDependencyModuleNames.Add("UnrealEd");
        }

        LoadFFmpeg(Target);
    }
}


//{
//    "Name": "FFMPEGMedia",
//			"Type": "RuntimeNoCommandlet",
//			"LoadingPhase" : "PreLoadingScreen",
//			"WhitelistPlatforms" : [ "Mac", "Win64", "Android" ]
//		},
//		{
//    "Name" : "FFMPEGMediaFactory",
//			"Type" : "RuntimeNoCommandlet",
//			"LoadingPhase" : "PostEngineInit",
//			"WhitelistPlatforms" : [ "Mac",  "Win64", "Android" ]
//		},