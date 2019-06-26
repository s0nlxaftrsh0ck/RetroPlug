#include "EmulatorView.h"

#include "platform/FileDialog.h"
#include "platform/Path.h"
#include "util/File.h"
#include "util/Serializer.h"

#include <tao/json.hpp>

EmulatorView::EmulatorView(SameBoyPlugPtr plug, RetroPlug* manager, IGraphics* graphics)
	: _plug(plug), _manager(manager), _graphics(graphics)
{
	memset(_videoScratch, 255, VIDEO_SCRATCH_SIZE);

	_settings = {
		{ "Color Correction", 2 },
		{ "High-pass Filter", 1 }
	};

	tao::json::value config;
	loadButtonConfig(config);
	_keyMap.load(config.at("gameboy"));
	_lsdjKeyMap.load(_keyMap, config.at("lsdj"));

	for (size_t i = 0; i < 2; i++) {
		_textIds[i] = new ITextControl(IRECT(0, -100, 0, 0), "", IText(23, COLOR_WHITE));
		graphics->AttachControl(_textIds[i]);
	}
}

EmulatorView::~EmulatorView() {
	HideText();

	if (_imageId != -1) {
		NVGcontext* ctx = (NVGcontext*)_graphics->GetDrawContext();
		nvgDeleteImage(ctx, _imageId);
	}
}

void EmulatorView::ShowText(const std::string & row1, const std::string & row2) {
	_showText = true;
	_textIds[0]->SetStr(row1.c_str());
	_textIds[1]->SetStr(row2.c_str());
	UpdateTextPosition();
}

void EmulatorView::HideText() {
	_showText = false;
	UpdateTextPosition();
}

void EmulatorView::UpdateTextPosition() {
	if (_showText) {
		float mid = _area.H() / 2;
		IRECT topRow(_area.L, mid - 25, _area.R, mid);
		IRECT bottomRow(_area.L, mid, _area.R, mid + 25);
		_textIds[0]->SetTargetAndDrawRECTs(topRow);
		_textIds[1]->SetTargetAndDrawRECTs(bottomRow);
	} else {
		_textIds[0]->SetTargetAndDrawRECTs(IRECT(0, -100, 0, 0));
		_textIds[1]->SetTargetAndDrawRECTs(IRECT(0, -100, 0, 0));
	}
}

void EmulatorView::SetArea(const IRECT & area) {
	_area = area;
	UpdateTextPosition();
}

void EmulatorView::Setup(SameBoyPlugPtr plug, RetroPlug* manager) {
	_plug = plug;
	_manager = manager;
	HideText();
}

bool EmulatorView::OnKey(const IKeyPress& key, bool down) {
	if (_plug && _plug->active()) {
		if (_plug->lsdj().found && _plug->lsdj().keyboardShortcuts) {
			return _lsdjKeyMap.onKey(key, down);
		} else {
			ButtonEvent ev;
			ev.id = _keyMap.getControllerButton((VirtualKey)key.VK);
			ev.down = down;

			if (ev.id != ButtonTypes::MAX) {
				_plug->setButtonState(ev);
				return true;
			}
		}
	}

	return false;
}

void EmulatorView::Draw(IGraphics& g) {
	if (_plug && _plug->active()) {
		MessageBus* bus = _plug->messageBus();

		// FIXME: This constant is the delta time between frames.
		// It is set to this because on windows iPlug doesn't go higher
		// than 30fps!  Should probably add some proper time calculation here.
		_lsdjKeyMap.update(bus, 33.3333333);

		size_t available = bus->video.readAvailable();
		if (available > 0) {
			// If we have multiple frames, skip to the latest
			if (available > VIDEO_FRAME_SIZE) {
				bus->video.advanceRead(available - VIDEO_FRAME_SIZE);
			}

			bus->video.read((char*)_videoScratch, VIDEO_FRAME_SIZE);

			// TODO: This is all a bit unecessary and should be handled in the SameBoy wrapper
			unsigned char* px = _videoScratch;
			for (int i = 0; i < VIDEO_WIDTH; i++) {
				for (int j = 0; j < VIDEO_HEIGHT; j++) {
					std::swap(px[0], px[2]);
					px[3] = 255;
					px += 4;
				}
			}
		}

		DrawPixelBuffer((NVGcontext*)g.GetDrawContext());
	}
}

void EmulatorView::DrawPixelBuffer(NVGcontext* vg) {
	if (_imageId == -1) {
		_imageId = nvgCreateImageRGBA(vg, VIDEO_WIDTH, VIDEO_HEIGHT, NVG_IMAGE_NEAREST, (const unsigned char*)_videoScratch);
	} else {
		nvgUpdateImage(vg, _imageId, (const unsigned char*)_videoScratch);
	}

	nvgBeginPath(vg);

	NVGpaint imgPaint = nvgImagePattern(vg, _area.L, _area.T, VIDEO_WIDTH * 2, VIDEO_HEIGHT * 2, 0, _imageId, _alpha);
	nvgRect(vg, _area.L, _area.T, _area.W(), _area.H());
	nvgFillPaint(vg, imgPaint);
	nvgFill(vg);
}

enum class SystemMenuItems : int {
	LoadRom,
	LoadRomAs,
	Reset,
	ResetAs,

	Sep1,

	NewSram,
	LoadSram,
	SaveSram,
	SaveSramAs,
};

void EmulatorView::CreateMenu(IPopupMenu* root, IPopupMenu* projectMenu) {
	if (!_plug) {
		return;
	}

	std::string romName = _plug->romName();
	bool loaded = !romName.empty();
	if (!loaded) {
		romName = "No ROM Loaded";
	}

	IPopupMenu* systemMenu = CreateSystemMenu();
	
	root->AddItem(romName.c_str(), (int)RootMenuItems::RomName, IPopupMenu::Item::kTitle);
	root->AddSeparator((int)RootMenuItems::Sep1);

	root->AddItem("Project", projectMenu, (int)RootMenuItems::Project);
	root->AddItem("System", systemMenu, (int)RootMenuItems::System);

	systemMenu->SetFunction([this](int indexInMenu, IPopupMenu::Item * itemChosen) {
		switch ((SystemMenuItems)indexInMenu) {
		case SystemMenuItems::LoadRom: OpenLoadRomDialog(GameboyModel::Auto); break;
		case SystemMenuItems::Reset: ResetSystem(true); break;
		case SystemMenuItems::NewSram: _plug->clearBattery(true); break;
		case SystemMenuItems::LoadSram: OpenLoadSramDialog(); break;
		case SystemMenuItems::SaveSram: _plug->saveBattery(L""); break;
		case SystemMenuItems::SaveSramAs: OpenSaveSramDialog(); break;
		}
	});

	if (loaded) {
		IPopupMenu* settingsMenu = CreateSettingsMenu();

		root->AddItem("Settings", settingsMenu, (int)RootMenuItems::Settings);
		root->AddSeparator((int)RootMenuItems::Sep2);
		root->AddItem("Game Link", (int)RootMenuItems::GameLink, _plug->gameLink() ? IPopupMenu::Item::kChecked : 0);
		root->AddSeparator((int)RootMenuItems::Sep3);

		root->SetFunction([this](int indexInMenu, IPopupMenu::Item* itemChosen) {
			switch ((RootMenuItems)indexInMenu) {
			case RootMenuItems::KeyboardMode: ToggleKeyboardMode(); break;
			case RootMenuItems::GameLink: _plug->setGameLink(!_plug->gameLink()); _manager->updateLinkTargets(); break;
			case RootMenuItems::SendClock: _plug->setMidiSync(!_plug->midiSync()); break;
			}
		});

		settingsMenu->SetFunction([this, settingsMenu](int indexInMenu, IPopupMenu::Item * itemChosen) {
			if (indexInMenu == settingsMenu->NItems() - 1) {
				ShellExecute(NULL, NULL, getContentPath().c_str(), NULL, NULL, SW_SHOWNORMAL);
			}
		});

		Lsdj& lsdj = _plug->lsdj();
		if (lsdj.found) {
			IPopupMenu* syncMenu = createSyncMenu(_plug->gameLink(), lsdj.autoPlay);
			root->AddItem("LSDj Sync", syncMenu, (int)RootMenuItems::LsdjModes);

			std::vector<LsdjSongName> songNames;
			_plug->saveBattery(lsdj.saveData);
			lsdj.getSongNames(songNames);

			if (!songNames.empty()) {
				IPopupMenu* songMenu = new IPopupMenu();
				songMenu->AddItem("Import (and reset)...");
				songMenu->AddItem("Export All...");
				songMenu->AddSeparator();

				root->AddItem("LSDj Songs", songMenu, (int)RootMenuItems::LsdjSongs);

				for (size_t i = 0; i < songNames.size(); i++) {
					IPopupMenu* songItemMenu = createSongMenu(songNames[i].projectId == -1);
					songMenu->AddItem(songNames[i].name.c_str(), songItemMenu);

					songItemMenu->SetFunction([=](int indexInMenu, IPopupMenu::Item* itemChosen) {
						int id = songNames[i].projectId;
						switch ((SongMenuItems)indexInMenu) {
						case SongMenuItems::Export: ExportSong(songNames[i]); break;
						case SongMenuItems::Load: LoadSong(id); break;
						case SongMenuItems::Delete: DeleteSong(id); break;
						}
					});
				}

				songMenu->SetFunction([=](int idx, IPopupMenu::Item*) {
					switch (idx) {
					case 0: OpenLoadSongsDialog(); break;
					case 1: ExportSongs(songNames); break;
					}
				});
			}
			
			root->AddItem("Keyboard Shortcuts", (int)RootMenuItems::KeyboardMode, lsdj.keyboardShortcuts ? IPopupMenu::Item::kChecked : 0);

			int selectedMode = GetLsdjModeMenuItem(lsdj.syncMode);
			syncMenu->CheckItem(selectedMode, true);
			syncMenu->SetFunction([this](int indexInMenu, IPopupMenu::Item* itemChosen) {
				LsdjSyncModeMenuItems menuItem = (LsdjSyncModeMenuItems)indexInMenu;
				if (menuItem <= LsdjSyncModeMenuItems::KeyboardModeArduinoboy) {
					_plug->lsdj().syncMode = GetLsdjModeFromMenu(menuItem);
				} else {
					_plug->lsdj().autoPlay = !_plug->lsdj().autoPlay;
				}
			});
		} else {
			root->AddItem("Send MIDI Clock", (int)RootMenuItems::SendClock, _plug->midiSync() ? IPopupMenu::Item::kChecked : 0);
		}
	}
}

IPopupMenu* EmulatorView::CreateSettingsMenu() {
	IPopupMenu* settingsMenu = new IPopupMenu();

	// TODO: These should be moved in to the SameBoy wrapper
	std::map<std::string, std::vector<std::string>> settings;
	settings["Color Correction"] = {
		"Off",
		"Correct Curves",
		"Emulate Hardware",
		"Preserve Brightness"
	};

	settings["High-pass Filter"] = {
		"Off",
		"Accurate",
		"Remove DC Offset"
	};

	for (auto& setting : settings) {
		const std::string& name = setting.first;
		IPopupMenu* settingMenu = new IPopupMenu(0, true);
		for (size_t i = 0; i < setting.second.size(); i++) {
			auto& option = setting.second[i];
			settingMenu->AddItem(option.c_str(), i);
		}

		settingMenu->CheckItem(_settings[name], true);
		settingsMenu->AddItem(name.c_str(), settingMenu);
		settingMenu->SetFunction([this, name](int indexInMenu, IPopupMenu::Item* itemChosen) {
			_settings[name] = indexInMenu;
			_plug->setSetting(name, indexInMenu);
		});
	}

	settingsMenu->AddSeparator();
	settingsMenu->AddItem("Open Settings Folder...");

	return settingsMenu;
}

IPopupMenu* EmulatorView::CreateSystemMenu() {
	IPopupMenu* loadAsModel = createModelMenu(true);
	IPopupMenu* resetAsModel = createModelMenu(false);

	IPopupMenu* menu = new IPopupMenu();
	menu->AddItem("Load ROM...", (int)SystemMenuItems::LoadRom);
	menu->AddItem("Load ROM As", loadAsModel, (int)SystemMenuItems::LoadRomAs);
	menu->AddItem("Reset", (int)SystemMenuItems::Reset);
	menu->AddItem("Reset As", resetAsModel, (int)SystemMenuItems::ResetAs);
	menu->AddSeparator((int)SystemMenuItems::Sep1);
	menu->AddItem("New .sav", (int)SystemMenuItems::NewSram);
	menu->AddItem("Load .sav...", (int)SystemMenuItems::LoadSram);
	menu->AddItem("Save .sav", (int)SystemMenuItems::SaveSram);
	menu->AddItem("Save .sav As...", (int)SystemMenuItems::SaveSramAs);

	resetAsModel->SetFunction([=](int idx, IPopupMenu::Item*) {
		_plug->reset((GameboyModel)(idx + 1), true);
	});

	loadAsModel->SetFunction([=](int idx, IPopupMenu::Item*) {
		OpenLoadRomDialog((GameboyModel)(idx + 1));
	});

	return menu;
}

void EmulatorView::ToggleKeyboardMode() {
	_plug->lsdj().keyboardShortcuts = !_plug->lsdj().keyboardShortcuts;
}

void EmulatorView::ExportSong(const LsdjSongName& songName) {
	std::vector<FileDialogFilters> types = {
		{ L"LSDj Songs", L"*.lsdsng" }
	};

	std::wstring path = BasicFileSave(types, s2ws(songName.name + "." + std::to_string(songName.version)));
	if (path.size() == 0) {
		return;
	}

	Lsdj& lsdj = _plug->lsdj();
	if (lsdj.found) {
		lsdj.saveData.clear();
		_plug->saveBattery(lsdj.saveData);

		if (lsdj.saveData.size() > 0) {
			std::vector<std::byte> songData;
			lsdj.exportSong(songName.projectId, songData);

			if (songData.size() > 0) {
				writeFile(path, songData);
			}
		}
	}
}

void EmulatorView::ExportSongs(const std::vector<LsdjSongName>& songNames) {
	std::vector<std::wstring> paths = BasicFileOpen({}, false, true);
	if (paths.size() > 0) {
		Lsdj& lsdj = _plug->lsdj();
		if (lsdj.found) {
			lsdj.saveData.clear();
			_plug->saveBattery(lsdj.saveData);
			
			if (lsdj.saveData.size() > 0) {
				std::vector<LsdjSongData> songData;
				lsdj.exportSongs(songData);

				for (auto& song : songData) {
					std::filesystem::path p(paths[0]);
					p /= song.name + ".lsdsng";
					writeFile(p.wstring(), song.data);
				}
			}
		}
	}
}

void EmulatorView::LoadSong(int index) {
	Lsdj& lsdj = _plug->lsdj();
	if (lsdj.found) {
		lsdj.loadSong(index);
		_plug->loadBattery(lsdj.saveData, true);
	}
}

void EmulatorView::DeleteSong(int index) {
	Lsdj& lsdj = _plug->lsdj();
	if (lsdj.found) {
		lsdj.deleteSong(index);
		_plug->loadBattery(lsdj.saveData, false);
	}
}

void EmulatorView::ResetSystem(bool fast) {
	_plug->reset(_plug->model(), fast);
}

void EmulatorView::OpenLoadSongsDialog() {
	std::vector<FileDialogFilters> types = {
		{ L"LSDj Songs", L"*.lsdsng" }
	};

	std::vector<std::wstring> paths = BasicFileOpen(types, true);
	Lsdj& lsdj = _plug->lsdj();
	if (lsdj.found) {
		std::string error;
		if (lsdj.importSongs(paths, error)) {
			_plug->loadBattery(lsdj.saveData, false);
		} else {
			_graphics->ShowMessageBox(error.c_str(), "Import Failed", kMB_OK);
		}
	}
}

void EmulatorView::OpenLoadRomDialog(GameboyModel model) {
	std::vector<FileDialogFilters> types = {
		{ L"GameBoy Roms", L"*.gb;*.gbc" }
	};

	std::vector<std::wstring> paths = BasicFileOpen(types, false);
	if (paths.size() > 0) {
		_plug->init(paths[0], model, false);
		_plug->disableRendering(false);
		HideText();
	}
}

void EmulatorView::DisableRendering(bool disable) {
	if (_plug->active()) {
		_plug->disableRendering(disable);
	}
}

void EmulatorView::LoadRom(const std::wstring & path) {
	_plug->init(path, GameboyModel::Auto, false);
	_plug->disableRendering(false);
	HideText();
}

void EmulatorView::OpenLoadSramDialog() {
	std::vector<FileDialogFilters> types = {
		{ L"GameBoy Saves", L"*.sav" }
	};

	std::vector<std::wstring> paths = BasicFileOpen(types, false);
	if (paths.size() > 0) {
		_plug->loadBattery(paths[0], true);
	}
}

void EmulatorView::OpenSaveSramDialog() {
	std::vector<FileDialogFilters> types = {
		{ L"GameBoy Saves", L"*.sav" }
	};

	std::wstring path = BasicFileSave(types);
	if (path.size() > 0) {
		_plug->saveBattery(path);
	}
}
