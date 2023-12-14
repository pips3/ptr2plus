/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
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

#pragma once

#include <QtWidgets/QWidget>

#include "ui_QuickSettingsWidget.h"

class SettingsDialog;

class QuickSettingsWidget : public QWidget
{
	Q_OBJECT

public:
	QuickSettingsWidget(SettingsDialog* dialog, QWidget* parent);
	~QuickSettingsWidget();

private Q_SLOTS:
	void volumeChanged(int value);
	void updateVolumeLabel();
	void onMinimalOutputLatencyStateChanged();
	void updateTargetLatencyRange();
	void onOptimalFramePacingChanged();
	void presetChanged();
	//void updateOptimalFramePacing();

private:

	SettingsDialog* m_dialog;

	Ui::QuickSettingsWidget m_ui;
	//u32 m_output_device_latency = 0;
};
