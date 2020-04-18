#include "RetroPlugController.h"
#include "config.h"
#include "platform/Path.h"
#include "platform/Resource.h"
#include "resource.h"

namespace AxisButtons {
	enum AxisButton {
		LeftStickLeft = 0,
		LeftStickRight = 1,
		LeftStickDown = 2,
		LeftStickUp = 3,
		RightStickLeft = 4,
		RightStickRight = 5,
		RightStickDown = 6,
		RightStickUp = 7,
		COUNT
	};
}
using AxisButtons::AxisButton;

const float AXIS_BUTTON_THRESHOLD = 0.5f;

RetroPlugController::RetroPlugController(iplug::ITimeInfo* timeInfo, double sampleRate)
	: _listener(&_uiLua, &_proxy), _audioController(timeInfo, sampleRate)
{
	_bus.addCall<calls::LoadRom>(4);
	_bus.addCall<calls::SwapInstance>(4);
	_bus.addCall<calls::TakeInstance>(4);
	_bus.addCall<calls::DuplicateInstance>(1);
	_bus.addCall<calls::ResetInstance>(4);
	_bus.addCall<calls::TransmitVideo>(16);
	_bus.addCall<calls::UpdateSettings>(4);
	_bus.addCall<calls::PressButtons>(32);
	_bus.addCall<calls::FetchState>(1);
	_bus.addCall<calls::ContextMenuResult>(1);
	_bus.addCall<calls::SwapLuaContext>(4);
	_bus.addCall<calls::SetActive>(4);
	_bus.addCall<calls::SetSram>(4);
	_bus.addCall<calls::SetRom>(4);

	_proxy.setNode(_bus.createNode(NodeTypes::Ui, { NodeTypes::Audio }));
	_audioController.setNode(_bus.createNode(NodeTypes::Audio, { NodeTypes::Ui }));

	_bus.start();

	memset(_padButtons, 0, sizeof(_padButtons));
	_padManager = new gainput::InputManager();
	_padId = _padManager->CreateDevice<gainput::InputDevicePad>();

	// Make sure the config script exists
	fs::path configDir = getContentPath(tstr(PLUG_VERSION_STR));
	fs::path configPath = configDir.string() + "\\config.lua";
	if (!fs::exists(configPath)) {
		Resource res(IDR_DEFAULT_CONFIG, "LUA");
		std::string_view data = res.getData();

		fs::create_directories(configDir);
		std::ofstream s(configPath, std::ios::binary);
		assert(s.good());
		s.write(data.data(), data.size());
		s.close();
	}

	fs::path scriptPath = fs::path(__FILE__).parent_path().parent_path() / "scripts";
	_uiLua.init(&_proxy, configDir.string(), scriptPath.string());

	_proxy.setScriptDirs(configDir.string(), scriptPath.string());
	//_audioController.getLuaContext()->init(configDir.string(), scriptPath.string());

	if (fs::exists(scriptPath)) {
		_scriptWatcher.addWatch(scriptPath.string(), &_listener, true);
	}

	_scriptWatcher.addWatch(configDir.string(), &_listener, true);
}

RetroPlugController::~RetroPlugController() {
	delete _padManager;
}

void RetroPlugController::update(float delta) {
	processPad();
	_scriptWatcher.update();
}

void RetroPlugController::init(iplug::igraphics::IGraphics* graphics, iplug::EHost host) {

	//pGraphics->AttachCornerResizer(kUIResizerScale, false);
	graphics->AttachPanelBackground(COLOR_BLACK);
	graphics->HandleMouseOver(true);
	graphics->LoadFont("Roboto-Regular", GAMEBOY_FN);
	graphics->LoadFont("Early-Gameboy", GAMEBOY_FN);

	graphics->SetKeyHandlerFunc([&](const IKeyPress& key, bool isUp) {
		return _uiLua.onKey(key, !isUp);
	});

	_view = new RetroPlugView(graphics->GetBounds(), &_uiLua, &_proxy, &_audioController);
	graphics->AttachControl(_view);

	_view->onFrame = [&](double delta) {
		update(delta);
	};
}

void RetroPlugController::processPad() {
	_padManager->Update();

	for (int i = 0; i < AxisButtons::COUNT / 2; ++i) {
		float val = _padManager->GetDevice(_padId)->GetFloat(i);
		int l = i * 2;
		int r = i * 2 + 1;

		if (val < -AXIS_BUTTON_THRESHOLD) {
			if (_padButtons[l] == false) {
				if (_padButtons[r] == true) {
					_padButtons[r] = false;
					_uiLua.onPadButton(r, false);
				}

				_padButtons[l] = true;
				_uiLua.onPadButton(l, true);
			}
		} else if (val > AXIS_BUTTON_THRESHOLD) {
			if (_padButtons[r] == false) {
				if (_padButtons[l] == true) {
					_padButtons[l] = false;
					_uiLua.onPadButton(l, false);
				}

				_padButtons[r] = true;
				_uiLua.onPadButton(r, true);
			}
		} else {
			if (_padButtons[l] == true) {
				_padButtons[l] = false;
				_uiLua.onPadButton(l, false);
			}

			if (_padButtons[r] == true) {
				_padButtons[r] = false;
				_uiLua.onPadButton(r, false);
			}
		}
	}

	for (int i = 0; i < gainput::PadButtonCount_; ++i) {
		int idx = gainput::PadButtonStart + i;
		bool down = _padManager->GetDevice(_padId)->GetBool(idx);
		if (_padButtons[idx] != down) {
			_padButtons[idx] = down;
			_uiLua.onPadButton(idx, down);
		}
	}
}