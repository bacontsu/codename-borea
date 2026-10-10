#ifndef GAMEUI_OPTIONSSUBVOICE_H
#define GAMEUI_OPTIONSSUBVOICE_H

#include <vgui_controls/PropertyPage.h>

#include "ivoicetweak.h"

namespace vgui2
{
class Button;
class ImagePanel;
class Label;
class Slider;
}

class CCvarSlider;
class CCvarToggleCheckButton;

class COptionsSubVoice: public vgui2::PropertyPage
{
	DECLARE_CLASS_SIMPLE(COptionsSubVoice, vgui2::PropertyPage);

public:
	COptionsSubVoice(vgui2::Panel* parent);
	virtual ~COptionsSubVoice();

	MESSAGE_FUNC(OnPageHide, "PageHide");
	MESSAGE_FUNC_INT(OnSliderMoved, "SliderMoved", position);
	MESSAGE_FUNC_INT(OnCheckButtonChecked, "CheckButtonChecked", state);
	MESSAGE_FUNC(OnControlModified, "ControlModified");

	void OnApplyChanges() override;

	void OnResetData() override;

	void OnThink() override;

	void StartTestMicrophone();
	void EndTestMicrophone();

	void UseCurrentVoiceParameters();

	void OnCommand(const char* command) override;

	void ResetVoiceParameters();

private:
	IVoiceTweak* m_pVoiceTweak = nullptr;

	vgui2::CheckButton* m_pMicBoost = nullptr;
	vgui2::ImagePanel* m_pMicMeter = nullptr;
	vgui2::ImagePanel* m_pMicMeter2 = nullptr;
	vgui2::Button* m_pTestMicrophoneButton = nullptr;
	vgui2::Label* m_pMicrophoneSliderLabel = nullptr;
	vgui2::Slider* m_pMicrophoneVolume = nullptr;
	vgui2::Label* m_pReceiveSliderLabel = nullptr;
	CCvarSlider* m_pReceiveVolume = nullptr;
	CCvarToggleCheckButton* m_pVoiceEnableCheckButton = nullptr;

	int m_nMicVolumeValue;
	bool m_bMicBoostSelected;
	float m_fReceiveVolume;
	int m_nReceiveSliderValue;
	bool m_bVoiceOn;

private:
	COptionsSubVoice(const COptionsSubVoice&) = delete;
	COptionsSubVoice& operator=(const COptionsSubVoice&) = delete;
};


#endif // GAMEUI_OPTIONSSUBVOICE_H
