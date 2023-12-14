/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PrecompiledHeader.h"

#include <QtWidgets/QInputDialog>
#include <QtWidgets/QMessageBox>
//#include <limits>

#include "pcsx2/Host.h"

//audio
#include "pcsx2/SPU2/Global.h"
#include "pcsx2/SPU2/spu2.h"
#include "pcsx2/VMManager.h"

#include "QuickSettingsWidget.h"
//#include "EmulationSettingsWidget.h"
//#include "GraphicsSettingsWidget.h"
//#include "AudioSettingsWidget.h"
#include "QtUtils.h"
#include "SettingWidgetBinder.h"
#include "SettingsDialog.h"

static constexpr s32 DEFAULT_TARGET_LATENCY = 60;
static constexpr s32 DEFAULT_OUTPUT_LATENCY = 20;
static constexpr s32 DEFAULT_VOLUME = 100;
static constexpr u32 DEFAULT_FRAME_LATENCY = 2;
static constexpr s32 DEFAULT_SYNCHRONIZATION_MODE = 0;



QuickSettingsWidget::QuickSettingsWidget(SettingsDialog* dialog, QWidget* parent)
	: QWidget(parent)
	, m_dialog(dialog)
{
	SettingsInterface* sif = dialog->getSettingsInterface();

	m_ui.setupUi(this);

	//performance preset
	static const char* preset_entries[] = {"Max Performance", "Balanced", "Max Quality", "Custom", nullptr};
	static const char* preset_values[] = { "1", "2", "3", "0", nullptr};

	SettingWidgetBinder::BindWidgetToEnumSetting(
		sif, m_ui.perfPreset, "EmuCore", "performance_preset", preset_entries, preset_values, "0");
	connect(m_ui.perfPreset, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &QuickSettingsWidget::presetChanged);

	//internal resolution
	static const char* upscale_entries[] = {"Native (PS2) (Default)", "1.25x Native", "1.5x Native", "1.75x Native", "2x Native (~720p)",
		"2.25x Native", "2.5x Native", "2.75x Native", "3x Native (~1080p)", "3.5x Native", "4x Native (~1440p/2K)", "5x Native (~1620p)",
		"6x Native (~2160p/4K)", "7x Native (~2520p)", "8x Native (~2880p/5K)", nullptr};
	static const char* upscale_values[] = {
		"1", "1.25", "1.5", "1.75", "2", "2.25", "2.5", "2.75", "3", "3.5", "4", "5", "6", "7", "8", nullptr};
	SettingWidgetBinder::BindWidgetToEnumSetting(
		sif, m_ui.upscaleMultiplier, "EmuCore/GS", "upscale_multiplier", upscale_entries, upscale_values, "1.0");

	//bilinear filtering
	SettingWidgetBinder::BindWidgetToIntSetting(
		sif, m_ui.bilinearFiltering, "EmuCore/GS", "linear_present_mode", static_cast<int>(GSPostBilinearMode::BilinearSmooth));


	//aspect ratio
	SettingWidgetBinder::BindWidgetToEnumSetting(
		sif, m_ui.aspectRatio, "EmuCore/GS", "AspectRatio", Pcsx2Config::GSOptions::AspectRatioNames, AspectRatioType::RAuto4_3_3_2);

	//vsync
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.vsync, "EmuCore/GS", "VsyncEnable", 0);

	//volume
	m_ui.volume->setValue(m_dialog->getEffectiveIntValue("SPU2/Mixing", "FinalVolume", DEFAULT_VOLUME));
	connect(m_ui.volume, &QSlider::valueChanged, this, &QuickSettingsWidget::volumeChanged);
	QuickSettingsWidget::updateVolumeLabel();

	//optimal frame pacing
	connect(m_ui.optimalFramePacing, &QCheckBox::stateChanged, this, &QuickSettingsWidget::onOptimalFramePacingChanged);
	m_ui.optimalFramePacing->setTristate(dialog->isPerGameSettings());

	//target and output latency
	QuickSettingsWidget::updateTargetLatencyRange();
	SettingWidgetBinder::BindSliderToIntSetting(
		//: Measuring unit that will appear after the number selected in its option. Adapt the space depending on your language's rules.
		sif, m_ui.targetLatency, m_ui.targetLatencyLabel, tr(" ms"), "SPU2/Output", "Latency", DEFAULT_TARGET_LATENCY);
	SettingWidgetBinder::BindSliderToIntSetting(
		sif, m_ui.outputLatency, m_ui.outputLatencyLabel, tr(" ms"), "SPU2/Output", "OutputLatency", DEFAULT_OUTPUT_LATENCY);

	//connect(m_ui.targetLatency, &QSlider::valueChanged, this, &QuickSettingsWidget::updateLatencyLabels);
	//connect(m_ui.outputLatency, &QSlider::valueChanged, this, &QuickSettingsWidget::updateLatencyLabels);

	QuickSettingsWidget::onMinimalOutputLatencyStateChanged();

	dialog->registerWidgetHelp(m_ui.perfPreset, tr("Performance Preset"), tr("Custom"),
		tr("Adjusts multiple settings based on the selected preset. Max Performance and Balanced presets enable "
		   "Speedhacks, lowers Aniostrophic Filtering and Blending, and disables Optimal Frame Pacing."));

	dialog->registerWidgetHelp(m_ui.upscaleMultiplier, tr("Internal Resolution"), tr("Native (PS2) (Default)"),
		tr("Control the resolution at which games are rendered. High resolutions can impact performance on "
		   "older or lower-end GPUs.<br>Non-native resolution may cause minor graphical issues in some games.<br>"
		   "FMV resolution will remain unchanged, as the video files are pre-rendered."));

	dialog->registerWidgetHelp(m_ui.bilinearFiltering, tr("Bilinear Filtering"), tr("Bilinear (Smooth)"),
		tr("Enables bilinear post processing filter. Smooths the overall picture as it is displayed on the screen. Corrects "
		   "positioning between pixels."));

	dialog->registerWidgetHelp(m_ui.aspectRatio, tr("Aspect Ratio"), tr("Auto Standard (4:3/3:2 Progressive)"),
		tr("Changes the aspect ratio used to display the console's output to the screen. The default is Auto Standard (4:3/3:2 "
		   "Progressive) which automatically adjusts the aspect ratio to match how a game would be shown on a typical TV of the era."));

	dialog->registerWidgetHelp(m_ui.vsync, tr("VSync"), tr("Unchecked"),
		tr("Enable this option to match PCSX2's refresh rate with your current monitor or screen. VSync is automatically disabled when "
		   "it is not possible (eg. running at non-100% speed)."));

	dialog->registerWidgetHelp(m_ui.volume, tr("Volume"), tr("100%"),
		tr("Pre-applies a volume modifier to the game's audio output before forwarding it to your computer."));

	dialog->registerWidgetHelp(m_ui.optimalFramePacing, tr("Optimal Frame Pacing"), tr("Unchecked"),
		tr("Sets the VSync queue size to 0, making every frame be completed and presented by the GS before input is polled and the next frame begins. "
		   "Using this setting can reduce input lag at the cost of measurably higher CPU and GPU requirements."));

	dialog->registerWidgetHelp(m_ui.targetLatency, tr("Target Latency"), tr("60 ms"),
		tr("Determines the buffer size which the time stretcher will try to keep filled. It effectively selects the average latency, as "
		   "audio will be stretched/shrunk to keep the buffer size within check."));

	dialog->registerWidgetHelp(m_ui.outputLatency, tr("Output Latency"), tr("20 ms"),
		tr("Determines the latency from the buffer to the host audio output. This can be set lower than the target latency to reduce audio "
		   "delay."));

	//EmulationSettingsWidget::updateOptimalFramePacing();
}

QuickSettingsWidget::~QuickSettingsWidget() = default;

void QuickSettingsWidget::volumeChanged(int value)
{
	// Nasty, but needed so we don't do a full settings apply and lag while dragging.
	if (SettingsInterface* sif = m_dialog->getSettingsInterface())
	{
		if (!m_ui.volumeLabel->font().bold())
		{
			QFont bold_font(m_ui.volumeLabel->font());
			bold_font.setBold(true);
			m_ui.volumeLabel->setFont(bold_font);
		}

		sif->SetIntValue("SPU2/Mixing", "FinalVolume", value);
		sif->Save();
	}
	else
	{
		Host::SetBaseIntSettingValue("SPU2/Mixing", "FinalVolume", value);
		Host::CommitBaseSettingChanges();
	}

	// Push through to emu thread since we're not applying.
	if (QtHost::IsVMValid())
	{
		Host::RunOnCPUThread([value]() {
			if (!VMManager::HasValidVM())
				return;

			EmuConfig.SPU2.FinalVolume = value;
			SPU2::SetOutputVolume(value);
		});
	}

	updateVolumeLabel();
}

void QuickSettingsWidget::updateVolumeLabel()
{
	//: Variable value that indicates a percentage. Preserve the %1 variable, adapt the latter % (and/or any possible spaces) to your language's ruleset.
	m_ui.volumeLabel->setText(tr("%1%").arg(m_ui.volume->value()));
}

void QuickSettingsWidget::onOptimalFramePacingChanged()
{
	//const QSignalBlocker sb(m_ui.maxFrameLatency);

	std::optional<int> value;
	bool optimal = false;
	if (m_ui.optimalFramePacing->checkState() != Qt::PartiallyChecked)
	{
		optimal = m_ui.optimalFramePacing->isChecked();
		value = optimal ? 0 : DEFAULT_FRAME_LATENCY;
	}
	else
	{
		value = m_dialog->getEffectiveIntValue("EmuCore/GS", "VsyncQueueSize", DEFAULT_FRAME_LATENCY);
		optimal = (value == 0);
	}

	//m_ui.maxFrameLatency->setMinimum(optimal ? 0 : 1);
	//m_ui.maxFrameLatency->setValue(optimal ? 0 : DEFAULT_FRAME_LATENCY);
	//m_ui.maxFrameLatency->setEnabled(!m_dialog->isPerGameSettings() && !m_ui.optimalFramePacing->isChecked());
	if (value != m_dialog->getEffectiveIntValue("EmuCore/GS", "VsyncQueueSize", DEFAULT_FRAME_LATENCY))
		m_dialog->setStringSettingValue("EmuCore", "performance_preset", "0" );
	m_dialog->setIntSettingValue("EmuCore/GS", "VsyncQueueSize", value);

}

void QuickSettingsWidget::updateTargetLatencyRange()
{
	const Pcsx2Config::SPU2Options::SynchronizationMode sync_mode = static_cast<Pcsx2Config::SPU2Options::SynchronizationMode>(
		m_dialog->getIntValue("SPU2/Output", "SynchMode", DEFAULT_SYNCHRONIZATION_MODE).value_or(DEFAULT_SYNCHRONIZATION_MODE));

	m_ui.targetLatency->setMinimum((sync_mode == Pcsx2Config::SPU2Options::SynchronizationMode::TimeStretch) ?
									   Pcsx2Config::SPU2Options::MIN_LATENCY_TIMESTRETCH :
									   Pcsx2Config::SPU2Options::MIN_LATENCY);
	m_ui.targetLatency->setMaximum(Pcsx2Config::SPU2Options::MAX_LATENCY);
}

void QuickSettingsWidget::onMinimalOutputLatencyStateChanged()
{
	m_ui.outputLatency->setEnabled(!m_dialog->getEffectiveBoolValue("SPU2/Output", "OutputLatencyMinimal", false));
}


void QuickSettingsWidget::presetChanged()
{
	int curval = m_ui.perfPreset->currentIndex();
	//could be a switch
	if (m_ui.perfPreset->currentIndex() == 2) // max quality
	{
		//affinity disabled
		m_dialog->setIntSettingValue("EmuCore/CPU", "AffinityControlMode", 0);
		//EE 100%
		m_dialog->setIntSettingValue("EmuCore/Speedhacks", "EECycleRate", 0);
		//EE Cycle Skipping Disabled
		m_dialog->setIntSettingValue("EmuCore/Speedhacks", "EECycleSkip", 0);
		//Optimal Frame ON
		m_dialog->setIntSettingValue("EmuCore/GS", "VsyncQueueSize", 0);
		// Blending Max
		m_dialog->setIntSettingValue("EmuCore/GS", "accurate_blending_unit", 5);

		// ani 16x
		m_dialog->setStringSettingValue("EmuCore/GS", "MaxAnisotropy", "16");
		// Mip FULL
		m_dialog->setIntSettingValue("EmuCore/GS", "mipmap_hw", 2);
		//Trilinear Forced
		m_dialog->setIntSettingValue("EmuCore/GS", "TriFilter", 2);
	}
	else if (m_ui.perfPreset->currentIndex() == 1) // balanced
	{
		//affinity disabled
		m_dialog->setIntSettingValue("EmuCore/CPU", "AffinityControlMode", 0);
		//EE 75% under
		m_dialog->setIntSettingValue("EmuCore/Speedhacks", "EECycleRate", -1);
		//EE Cycle Skipping Disabled
		m_dialog->setIntSettingValue("EmuCore/Speedhacks", "EECycleSkip", 0);
		//Optimal Frame Off
		if (DEFAULT_FRAME_LATENCY < 2)
		{
			m_dialog->setIntSettingValue("EmuCore/GS", "VsyncQueueSize", 2);
		}
		else
		{
			m_dialog->setIntSettingValue("EmuCore/GS", "VsyncQueueSize", DEFAULT_FRAME_LATENCY);
		}
		// Blending medium
		m_dialog->setIntSettingValue("EmuCore/GS", "accurate_blending_unit", 2);
		// ani 8x
		m_dialog->setStringSettingValue("EmuCore/GS", "MaxAnisotropy", "8");
		// Mip FULL
		m_dialog->setIntSettingValue("EmuCore/GS", "mipmap_hw", 2);
		//Trilinear Forced
		m_dialog->setIntSettingValue("EmuCore/GS", "TriFilter", 2);
	}
	else if (m_ui.perfPreset->currentIndex() == 0) // max performance
	{
		//affinity VU EE GS
		m_dialog->setIntSettingValue("EmuCore/CPU", "AffinityControlMode", 3);
		//EE 60% under
		m_dialog->setIntSettingValue("EmuCore/Speedhacks", "EECycleRate", -2);
		//EE Cycle Skipping Disabled
		m_dialog->setIntSettingValue("EmuCore/Speedhacks", "EECycleSkip", 0);
		//Optimal Frame Off
		if (DEFAULT_FRAME_LATENCY < 2)
		{
			m_dialog->setIntSettingValue("EmuCore/GS", "VsyncQueueSize", 2);
		}
		else
		{
			m_dialog->setIntSettingValue("EmuCore/GS", "VsyncQueueSize", DEFAULT_FRAME_LATENCY);
		}
		// Blending Minimum
		m_dialog->setIntSettingValue("EmuCore/GS", "accurate_blending_unit", 0);

		// ani OFF
		m_dialog->setStringSettingValue("EmuCore/GS", "MaxAnisotropy", "0");
		// Mip FULL
		m_dialog->setIntSettingValue("EmuCore/GS", "mipmap_hw", 2);
		//Trilinear Forced
		m_dialog->setIntSettingValue("EmuCore/GS", "TriFilter", 2);

	}

}

