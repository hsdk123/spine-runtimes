/******************************************************************************
* Spine Runtimes Software License v2.5
*
* Copyright (c) 2013-2016, Esoteric Software
* All rights reserved.
*
* You are granted a perpetual, non-exclusive, non-sublicensable, and
* non-transferable license to use, install, execute, and perform the Spine
* Runtimes software and derivative works solely for personal or internal
* use. Without the written permission of Esoteric Software (see Section 2 of
* the Spine Software License Agreement), you may not (a) modify, translate,
* adapt, or develop new applications using the Spine Runtimes or otherwise
* create derivative works or improvements of the Spine Runtimes or (b) remove,
* delete, alter, or obscure any trademarks or any copyright, trademark, patent,
* or other intellectual property or proprietary rights notices on or in the
* Software, including any copy thereof. Redistributions in binary or source
* form must include this license and terms.
*
* THIS SOFTWARE IS PROVIDED BY ESOTERIC SOFTWARE "AS IS" AND ANY EXPRESS OR
* IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
* MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
* EVENT SHALL ESOTERIC SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, BUSINESS INTERRUPTION, OR LOSS OF
* USE, DATA, OR PROFITS) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
* IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

#include "SpinePluginPrivatePCH.h"
#include "SpineWidget.h"
#include "SSpineWidget.h"
#include "Engine.h"
#include "spine/spine.h"

#define LOCTEXT_NAMESPACE "Spine"

using namespace spine;

void callback(AnimationState* state, spine::EventType type, TrackEntry* entry, Event* event);

USpineWidget::USpineWidget(const FObjectInitializer& ObjectInitializer): Super(ObjectInitializer) {
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> NormalMaterialRef(TEXT("/SpinePlugin/UI_SpineUnlitNormalMaterial"));
	NormalBlendMaterial = NormalMaterialRef.Object;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> AdditiveMaterialRef(TEXT("/SpinePlugin/UI_SpineUnlitAdditiveMaterial"));
	AdditiveBlendMaterial = AdditiveMaterialRef.Object;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MultiplyMaterialRef(TEXT("/SpinePlugin/UI_SpineUnlitMultiplyMaterial"));
	MultiplyBlendMaterial = MultiplyMaterialRef.Object;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ScreenMaterialRef(TEXT("/SpinePlugin/UI_SpineUnlitScreenMaterial"));
	ScreenBlendMaterial = ScreenMaterialRef.Object;

	TextureParameterName = FName(TEXT("SpriteTexture"));

	worldVertices.ensureCapacity(1024 * 2);

	bAutoPlaying = true;
}

void USpineWidget::SynchronizeProperties() {
	Super::SynchronizeProperties();

	if (slateWidget.IsValid()) {
		CheckState();
		if (skeleton) {
			Tick(0, false);			
			slateWidget->SetData(this);
		} else {
			slateWidget->SetData(nullptr);		
		}
		trackEntries.Empty();
	}
}

void USpineWidget::ReleaseSlateResources(bool bReleaseChildren) {
	Super::ReleaseSlateResources(bReleaseChildren);
	slateWidget.Reset();
}

TSharedRef<SWidget> USpineWidget::RebuildWidget() {
	this->slateWidget = SNew(SSpineWidget);
	return this->slateWidget.ToSharedRef();
}

#if WITH_EDITOR
const FText USpineWidget::GetPaletteCategory() {
	return LOCTEXT("Spine", "Spine");
}
#endif

void USpineWidget::Tick(float DeltaTime, bool CallDelegates) {
	CheckState();

	if (state && bAutoPlaying) {
		state->update(DeltaTime);
		state->apply(*skeleton);
		if (CallDelegates) BeforeUpdateWorldTransform.Broadcast(this);
		skeleton->updateWorldTransform();
		if (CallDelegates) AfterUpdateWorldTransform.Broadcast(this);
	}
}

void USpineWidget::CheckState() {
	bool needsUpdate = lastAtlas != Atlas || lastData != SkeletonData;

	if (!needsUpdate) {
		// Are we doing a re-import? Then check if the underlying spine-cpp data
		// has changed.
		if (lastAtlas && lastAtlas == Atlas && lastData && lastData == SkeletonData) {
			spine::Atlas* atlas = Atlas->GetAtlas();
			if (lastSpineAtlas != atlas) {
				needsUpdate = true;
			}
			if (skeleton && skeleton->getData() != SkeletonData->GetSkeletonData(atlas)) {
				needsUpdate = true;
			}
		}
	}

	if (needsUpdate) {
		DisposeState();

		if (Atlas && SkeletonData) {
			spine::SkeletonData *data = SkeletonData->GetSkeletonData(Atlas->GetAtlas());
			if (data) {
				skeleton = new (__FILE__, __LINE__) Skeleton(data);
				AnimationStateData* stateData = SkeletonData->GetAnimationStateData(Atlas->GetAtlas());
				state = new (__FILE__, __LINE__) AnimationState(stateData);
				state->setRendererObject((void*)this);
				state->setListener(callback);
				trackEntries.Empty();
			}
		}

		lastAtlas = Atlas;
		lastSpineAtlas = Atlas ? Atlas->GetAtlas() : nullptr;
		lastData = SkeletonData;
	}
}

void USpineWidget::DisposeState() {
	if (state) {
		delete state;
		state = nullptr;
	}

	if (skeleton) {
		delete skeleton;
		skeleton = nullptr;
	}

	trackEntries.Empty();
}

void USpineWidget::FinishDestroy() {
	DisposeState();
	Super::FinishDestroy();
}

bool USpineWidget::SetSkin(const FString skinName) {
	CheckState();
	if (skeleton) {
		spine::Skin* skin = skeleton->getData()->findSkin(TCHAR_TO_UTF8(*skinName));
		if (!skin) return false;
		skeleton->setSkin(skin);
		return true;
	}
	else return false;
}

void USpineWidget::GetSkins(TArray<FString> &Skins) {
	CheckState();
	if (skeleton) {
		for (size_t i = 0, n = skeleton->getData()->getSkins().size(); i < n; i++) {
			Skins.Add(skeleton->getData()->getSkins()[i]->getName().buffer());
		}
	}
}

bool USpineWidget::HasSkin(const FString skinName) {
	CheckState();
	if (skeleton) {
		return skeleton->getData()->findAnimation(TCHAR_TO_UTF8(*skinName)) != nullptr;
	}
	return false;
}

bool USpineWidget::SetAttachment(const FString slotName, const FString attachmentName) {
	CheckState();
	if (skeleton) {
		if (!skeleton->getAttachment(TCHAR_TO_UTF8(*slotName), TCHAR_TO_UTF8(*attachmentName))) return false;
		skeleton->setAttachment(TCHAR_TO_UTF8(*slotName), TCHAR_TO_UTF8(*attachmentName));
		return true;
	}
	return false;
}

void USpineWidget::UpdateWorldTransform() {
	CheckState();
	if (skeleton) {
		skeleton->updateWorldTransform();
	}
}

void USpineWidget::SetToSetupPose() {
	CheckState();
	if (skeleton) skeleton->setToSetupPose();
}

void USpineWidget::SetBonesToSetupPose() {
	CheckState();
	if (skeleton) skeleton->setBonesToSetupPose();
}

void USpineWidget::SetSlotsToSetupPose() {
	CheckState();
	if (skeleton) skeleton->setSlotsToSetupPose();
}

void USpineWidget::SetScaleX(float scaleX) {
	CheckState();
	if (skeleton) skeleton->setScaleX(scaleX);
}

float USpineWidget::GetScaleX() {
	CheckState();
	if (skeleton) return skeleton->getScaleX();
	return 1;
}

void USpineWidget::SetScaleY(float scaleY) {
	CheckState();
	if (skeleton) skeleton->setScaleY(scaleY);
}

float USpineWidget::GetScaleY() {
	CheckState();
	if (skeleton) return skeleton->getScaleY();
	return 1;
}

void USpineWidget::GetBones(TArray<FString> &Bones) {
	CheckState();
	if (skeleton) {
		for (size_t i = 0, n = skeleton->getBones().size(); i < n; i++) {
			Bones.Add(skeleton->getBones()[i]->getData().getName().buffer());
		}
	}
}

bool USpineWidget::HasBone(const FString BoneName) {
	CheckState();
	if (skeleton) {
		return skeleton->getData()->findBone(TCHAR_TO_UTF8(*BoneName)) != nullptr;
	}
	return false;
}

void USpineWidget::GetSlots(TArray<FString> &Slots) {
	CheckState();
	if (skeleton) {
		for (size_t i = 0, n = skeleton->getSlots().size(); i < n; i++) {
			Slots.Add(skeleton->getSlots()[i]->getData().getName().buffer());
		}
	}
}

bool USpineWidget::HasSlot(const FString SlotName) {
	CheckState();
	if (skeleton) {
		return skeleton->getData()->findSlot(TCHAR_TO_UTF8(*SlotName)) != nullptr;
	}
	return false;
}

void USpineWidget::GetAnimations(TArray<FString> &Animations) {
	CheckState();
	if (skeleton) {
		for (size_t i = 0, n = skeleton->getData()->getAnimations().size(); i < n; i++) {
			Animations.Add(skeleton->getData()->getAnimations()[i]->getName().buffer());
		}
	}
}

bool USpineWidget::HasAnimation(FString AnimationName) {
	CheckState();
	if (skeleton) {
		return skeleton->getData()->findAnimation(TCHAR_TO_UTF8(*AnimationName)) != nullptr;
	}
	return false;
}

float USpineWidget::GetAnimationDuration(FString AnimationName) {
	CheckState();
	if (skeleton) {
		spine::Animation *animation = skeleton->getData()->findAnimation(TCHAR_TO_UTF8(*AnimationName));
		if (animation == nullptr) return 0;
		else return animation->getDuration();
	}
	return 0;
}

void USpineWidget::SetAutoPlay(bool bInAutoPlays)
{
	bAutoPlaying = bInAutoPlays;
}

void USpineWidget::SetPlaybackTime(float InPlaybackTime, bool bCallDelegates)
{
	CheckState();

	if (state && state->getCurrent(0)) {
		spine::Animation* CurrentAnimation = state->getCurrent(0)->getAnimation();
		const float CurrentTime = state->getCurrent(0)->getTrackTime();
		InPlaybackTime = FMath::Clamp(InPlaybackTime, 0.0f, CurrentAnimation->getDuration());
		const float DeltaTime = InPlaybackTime - CurrentTime;
		state->update(DeltaTime);
		state->apply(*skeleton);

		//Call delegates and perform the world transform
		if (bCallDelegates)
		{
			BeforeUpdateWorldTransform.Broadcast(this);
		}
		skeleton->updateWorldTransform();
		if (bCallDelegates)
		{
			AfterUpdateWorldTransform.Broadcast(this);
		}
	}
}

void USpineWidget::SetTimeScale(float timeScale) {
	CheckState();
	if (state) state->setTimeScale(timeScale);
}

float USpineWidget::GetTimeScale() {
	CheckState();
	if (state) return state->getTimeScale();
	return 1;
}

UTrackEntry* USpineWidget::SetAnimation(int trackIndex, FString animationName, bool loop) {
	CheckState();
	if (state && skeleton->getData()->findAnimation(TCHAR_TO_UTF8(*animationName))) {
		state->disableQueue();
		TrackEntry* entry = state->setAnimation(trackIndex, TCHAR_TO_UTF8(*animationName), loop);
		state->enableQueue();
		UTrackEntry* uEntry = NewObject<UTrackEntry>();
		uEntry->SetTrackEntry(entry);
		trackEntries.Add(uEntry);
		return uEntry;
	}
	else return NewObject<UTrackEntry>();

}

UTrackEntry* USpineWidget::AddAnimation(int trackIndex, FString animationName, bool loop, float delay) {
	CheckState();
	if (state && skeleton->getData()->findAnimation(TCHAR_TO_UTF8(*animationName))) {
		state->disableQueue();
		TrackEntry* entry = state->addAnimation(trackIndex, TCHAR_TO_UTF8(*animationName), loop, delay);
		state->enableQueue();
		UTrackEntry* uEntry = NewObject<UTrackEntry>();
		uEntry->SetTrackEntry(entry);
		trackEntries.Add(uEntry);
		return uEntry;
	}
	else return NewObject<UTrackEntry>();
}

UTrackEntry* USpineWidget::SetEmptyAnimation(int trackIndex, float mixDuration) {
	CheckState();
	if (state) {
		TrackEntry* entry = state->setEmptyAnimation(trackIndex, mixDuration);
		UTrackEntry* uEntry = NewObject<UTrackEntry>();
		uEntry->SetTrackEntry(entry);
		trackEntries.Add(uEntry);
		return uEntry;
	}
	else return NewObject<UTrackEntry>();
}

UTrackEntry* USpineWidget::AddEmptyAnimation(int trackIndex, float mixDuration, float delay) {
	CheckState();
	if (state) {
		TrackEntry* entry = state->addEmptyAnimation(trackIndex, mixDuration, delay);
		UTrackEntry* uEntry = NewObject<UTrackEntry>();
		uEntry->SetTrackEntry(entry);
		trackEntries.Add(uEntry);
		return uEntry;
	}
	else return NewObject<UTrackEntry>();
}

UTrackEntry* USpineWidget::GetCurrent(int trackIndex) {
	CheckState();
	if (state) {
		TrackEntry* entry = state->getCurrent(trackIndex);
		if (entry->getRendererObject()) {
			return (UTrackEntry*)entry->getRendererObject();
		}
		else {
			UTrackEntry* uEntry = NewObject<UTrackEntry>();
			uEntry->SetTrackEntry(entry);
			trackEntries.Add(uEntry);
			return uEntry;
		}
	}
	else return NewObject<UTrackEntry>();
}

void USpineWidget::ClearTracks() {
	CheckState();
	if (state) {
		state->clearTracks();
	}
}

void USpineWidget::ClearTrack(int trackIndex) {
	CheckState();
	if (state) {
		state->clearTrack(trackIndex);
	}
}