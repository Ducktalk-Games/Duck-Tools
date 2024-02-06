// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ContentBrowserModule.h"
#include "MaterialTypes.h"
#include "Materials/MaterialInstanceConstant.h"

class FSetTexturesToMaterialModule: public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static inline FSetTexturesToMaterialModule& Get() {
		return FModuleManager::LoadModuleChecked<FSetTexturesToMaterialModule>("***SetTexturesToMaterial***");
	}

	static inline bool IsAvailable() {
		return FModuleManager::Get().IsModuleLoaded("SetTexturesToMaterial");
	}

private:
	static void SetTexturesToMaterialInstance(TArray<FAssetData> SelectedTextureAssets);
	static TSharedRef<FExtender> AssetToolsExtender(const TArray<FAssetData>& SelectedAssets);
	static void AssetToolsExtenderFunc(FMenuBuilder& MenuBuilder, const TArray<FAssetData> SelectedAssets);
	FMaterialParameterInfo FindParameterFromSuffix(FString Suffix, UMaterialInstanceConstant* MaterialInstance);
};
