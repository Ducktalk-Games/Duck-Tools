// Copyright Epic Games, Inc. All Rights Reserved.

#include "SetTexturestoMaterial.h"
#include "Styling/AppStyle.h"
#include "Misc/Paths.h"
#include "Editor.h"
#include "Editor/UnrealEd/Public/Subsystems/EditorAssetSubsystem.h"
#include "IContentBrowserSingleton.h"
#include "Materials/MaterialInstanceConstant.h"
#include "AssetToolsModule.h"
#include "ScopedTransaction.h"
#include "Engine/Texture2D.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "FramePro/FramePro.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/PlatformFileManager.h"

#define LOCTEXT_NAMESPACE "FSetTexturesToMaterialModule"

void FSetTexturesToMaterialModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	TArray<FContentBrowserMenuExtender_SelectedAssets>& CBMenuAssetExtenderDelegates{
		ContentBrowserModule.GetAllAssetViewContextMenuExtenders()
	};
	CBMenuAssetExtenderDelegates.Add(FContentBrowserMenuExtender_SelectedAssets::CreateStatic(&AssetToolsExtender));
}

void FSetTexturesToMaterialModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

TSharedRef<FExtender> FSetTexturesToMaterialModule::AssetToolsExtender(const TArray<FAssetData>& SelectedAssets)
{
	TSharedRef<FExtender> Extender = MakeShared<FExtender>();
	Extender->AddMenuExtension(
		"CommonAssetActions",
		EExtensionHook::After,
		nullptr,
		FMenuExtensionDelegate::CreateStatic(&AssetToolsExtenderFunc, SelectedAssets)
	);
	return Extender;
}

void FSetTexturesToMaterialModule::AssetToolsExtenderFunc(FMenuBuilder& MenuBuilder, const TArray<FAssetData> SelectedAssets)
{
	MenuBuilder.BeginSection("Tom's Asset Tools", LOCTEXT("TOMS_ASSET_TOOLS_CONTEXT", "Tom's Asset Tools"));
	{
		TArray<FAssetData> SelectedTextureAssets;

		for (auto AssetIt = SelectedAssets.CreateConstIterator(); AssetIt; ++AssetIt)
		{
			const FAssetData& Asset{ *AssetIt };
			if (!Asset.IsRedirector() && !(Asset.PackageFlags & PKG_FilterEditorOnly))
			{
				if (Asset.GetClass()->IsChildOf(UTexture2D::StaticClass()))
				{
					SelectedTextureAssets.Add(Asset);
				}
			}
		}

		if (!SelectedTextureAssets.IsEmpty())
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("Set Texture(s) to Material Instance(s)", "Set Texture(s) to Material Instance(s)"),
				LOCTEXT("Set each texture to a material instance corresponding to the texture's name and parameter",
					"Set each texture to a material instance corresponding to the texture's name and parameter"),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), ""),
				/*FUIAction(FExecuteAction::CreateRaw(this, &FSetTexturesToMaterialModule::SetTexturesToMaterialInstance, SelectedAssets)),*/
				FUIAction(FExecuteAction::CreateLambda([&, SelectedTextureAssets]()
					{
						SetTexturesToMaterialInstance(SelectedTextureAssets);
					})),
				NAME_None,
				EUserInterfaceActionType::Button);
		}
	}
	MenuBuilder.EndSection();
}

void FSetTexturesToMaterialModule::SetTexturesToMaterialInstance(TArray<FAssetData> SelectedTextureAssets)
{
	// FScopedTransaction Transaction(LOCTEXT("SetTexturesToMaterial", "Set Textures To Material Instance"));
	
	for (FAssetData SelectedAsset : SelectedTextureAssets)
	{
		SelectedAsset.GetAsset()->SetFlags(RF_Transactional);
		// SelectedAsset.GetAsset()->Modify();
		
		FString RelativeTexturePath{ SelectedAsset.PackagePath.ToString() };

		// If selected asset is not within an _Textures folder, Create Textures Folder ---------------------------------------

		if (!RelativeTexturePath.EndsWith("_Textures"))
		{

			// Project content dir, presented C:/../../
			FString Path = FPaths::ConvertRelativePathToFull(*FPaths::ProjectContentDir());

			// The texture path, presented /Game/../../_Textures/
			RelativeTexturePath = FPaths::Combine(RelativeTexturePath, "_Textures/");

			// Combining both the absolute project content dir and the texture path, presented C:/../../../_Textures/
			FString TexturePath{ RelativeTexturePath.Replace(*FString("/Game/"), *Path) };
			
		}

		// --------------------------------------------------------------------------------------------------------------------

		// If asset does not start with "T_" rename it, move it to _Textures with new name ------------------------------------

		FString ObjectName{ SelectedAsset.GetAsset()->GetName() };

		if (!ObjectName.StartsWith("T_"))
		{
			ObjectName.InsertAt(0, FString("T_"));
		}

		FString SourcePath{ SelectedAsset.PackageName.ToString() };
		FString DestinationPath{ FPaths::Combine(RelativeTexturePath, ObjectName) };

		// Create the material instance name for the selected texture

		TArray<FString> ObjectNameArray;
		ObjectName.ParseIntoArray(ObjectNameArray, TEXT("_"));

		// ObjectName = MI_TextureName_Suffix
		ObjectName = ObjectNameArray[1];
		ObjectName.InsertAt(0, TEXT("MI_"));

		// Look for the material instance name 

		FString MaterialInstanceFolderPath{ RelativeTexturePath };

		MaterialInstanceFolderPath.RemoveFromEnd(TEXT("_Textures/"));

		FString MaterialInstanceFilePath{ MaterialInstanceFolderPath.Append(ObjectName) };
		
		UEditorAssetSubsystem* EditorAssetSubsystem{ GEditor->GetEditorSubsystem<UEditorAssetSubsystem>() };

		UMaterialInstanceConstant* MaterialInstance{ Cast<UMaterialInstanceConstant>(EditorAssetSubsystem->LoadAsset(MaterialInstanceFilePath)) };
		
		// If the material instance load fails, create a new material instance
		
		if (!MaterialInstance)
		{
			FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
			
			MaterialInstance = Cast<UMaterialInstanceConstant>(
				AssetToolsModule.Get().CreateAsset(
					ObjectName,
					FPaths::GetPath(MaterialInstanceFolderPath),
					UMaterialInstanceConstant::StaticClass(),
					NewObject<UMaterialInstanceConstantFactoryNew>()
				)
			);

			MaterialInstance->SetFlags(RF_Transactional);
			// MaterialInstance->Modify();

			UE_LOG(LogTemp, Warning, TEXT("Hey we are doing %s"), *MaterialInstance->GetName());
			
			EditorAssetSubsystem->SaveAsset(MaterialInstance->GetPackage()->GetPathName());
		}
		
		// Get the master material
		UMaterialInterface* MasterMaterial{ Cast<UMaterialInterface>(EditorAssetSubsystem->LoadAsset("/SetTexturesToMaterial/M_Master")) };

		// If the Material Instance and Master Material exist, and the material instance parent is not the master material, set the master material to the instance parent

		if (!MasterMaterial)
		{
			UE_LOG(LogTemp, Error, TEXT("Can't find Master Material 'M_Master' in content folder"));
			return;
		}
		if (!MaterialInstance)
		{
			UE_LOG(LogTemp, Error, TEXT("Can't find Material Instance"));
			return;
		}
		
		
		if (MaterialInstance->Parent != MasterMaterial)
		{
			MaterialInstance->SetParentEditorOnly(MasterMaterial);
		}
		
		TArray<FMaterialParameterInfo> MaterialInfo;
		TArray<FGuid> TextureParameterValues;

		MaterialInstance->GetAllTextureParameterInfo(MaterialInfo, TextureParameterValues);
		bool FoundParameter = false;
		
		for (FMaterialParameterInfo TextureParameterInfo : MaterialInfo)
		{
			FString ParameterString { TextureParameterInfo.Name.ToString() };
			if (ObjectNameArray.Last() == ParameterString)
			{
				UTexture2D* Texture { Cast<UTexture2D>(SelectedAsset.GetAsset()) };
				if (Texture)
				{
					UTexture* MasterMatTexture = nullptr;
					MasterMaterial->GetTextureParameterValue(TextureParameterInfo, MasterMatTexture, false);
					UE_LOG(LogTemp, Log, TEXT("Setting %s to SRGB of assigned"), *Texture->GetName());

					
					Texture->SRGB = MasterMatTexture->SRGB;
					MaterialInstance->SetTextureParameterValueEditorOnly(TextureParameterInfo, Texture);
					FoundParameter = true;
				}
				break;
			}
		}

		if (!FoundParameter)
		{
			TArray<FString> ParameterNameArray;
			for (FMaterialParameterInfo TextureParameterInfo : MaterialInfo)
			{
				ParameterNameArray.Add(TextureParameterInfo.Name.ToString());
			}
			UE_LOG(LogTemp, Error, TEXT("Could not find selected texture suffix in %s"), *FString::Join(ParameterNameArray, TEXT(" ,")))
		}
		
		if (SourcePath != DestinationPath)
		{
			EditorAssetSubsystem->RenameAsset(SourcePath, DestinationPath);
		}
	}
}



#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FSetTexturesToMaterialModule, SetTexturesToMaterial)