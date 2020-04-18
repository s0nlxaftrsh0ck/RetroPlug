#include "LuaHelpers.h"

#include "platform/Logger.h"
#include "model/Project.h"
#include "config/config.h"
#include "view/Menu.h"
#include "model/ButtonStream.h"

bool validateResult(const sol::protected_function_result& result, const std::string& prefix, const std::string& name) {
	if (!result.valid()) {
		sol::error err = result;
		std::string what = err.what();
		consoleLog(prefix);
		if (!name.empty()) {
			consoleLog(" " + name);
		}

		consoleLogLine(": " + what);
		return false;
	}

	return true;
}

void setupCommon(sol::state& s) {
	s.new_enum("MenuItemType",
		"None", MenuItemType::None,
		"SubMenu", MenuItemType::SubMenu,
		"Select", MenuItemType::Select,
		"MultiSelect", MenuItemType::MultiSelect,
		"Separator", MenuItemType::Separator,
		"Action", MenuItemType::Action,
		"Title", MenuItemType::Title
	);

	s.new_enum("EmulatorInstanceState",
		"Uninitialized", EmulatorInstanceState::Uninitialized,
		"Initialized", EmulatorInstanceState::Initialized,
		"RomMissing", EmulatorInstanceState::RomMissing,
		"Running", EmulatorInstanceState::Running
	);

	s.new_enum("EmulatorType",
		"Unknown", EmulatorType::Unknown,
		"Placeholder", EmulatorType::Placeholder,
		"SameBoy", EmulatorType::SameBoy
	);

	s.new_enum("AudioChannelRouting",
		"StereoMixDown", AudioChannelRouting::StereoMixDown,
		"TwoChannelsPerChannel", AudioChannelRouting::TwoChannelsPerChannel,
		"TwoChannelsPerInstance", AudioChannelRouting::TwoChannelsPerInstance
	);

	s.new_enum("MidiChannelRouting",
		"FourChannelsPerInstance", MidiChannelRouting::FourChannelsPerInstance,
		"OneChannelPerInstance", MidiChannelRouting::OneChannelPerInstance,
		"SendToAll", MidiChannelRouting::SendToAll
	);

	s.new_enum("InstanceLayout",
		"Auto", InstanceLayout::Auto,
		"Column", InstanceLayout::Column,
		"Grid", InstanceLayout::Grid,
		"Row", InstanceLayout::Row
	);

	s.new_enum("SaveStateType",
		"Sram", SaveStateType::Sram,
		"State", SaveStateType::State
	);

	s.new_enum("GameboyModel",
		"Auto", GameboyModel::Auto,
		"Agb", GameboyModel::Agb,
		"CgbC", GameboyModel::CgbC,
		"CgbE", GameboyModel::CgbE,
		"DmgB", GameboyModel::DmgB
	);

	s.new_enum("DialogType",
		"Load", DialogType::Load,
		"Save", DialogType::Save
	);

	s.new_usertype<SameBoySettings>("SameBoySettings",
		"model", &SameBoySettings::model,
		"gameLink", &SameBoySettings::gameLink
	);

	s.new_usertype<Project>("Project",
		"path", &Project::path,
		"instances", &Project::instances,
		"settings", &Project::settings
	);

	s.new_usertype<Project::Settings>("ProjectSettings",
		"audioRouting", &Project::Settings::audioRouting,
		"midiRouting", &Project::Settings::midiRouting,
		"layout", &Project::Settings::layout,
		"zoom", &Project::Settings::zoom,
		"saveType", &Project::Settings::saveType
	);

	s.new_usertype<GameboyButtonStream>("GameboyButtonStream",
		"hold", &GameboyButtonStream::hold,
		"release", &GameboyButtonStream::release,
		"releaseAll", &GameboyButtonStream::releaseAll,
		"delay", &GameboyButtonStream::delay,
		"press", &GameboyButtonStream::press,

		"holdDuration", &GameboyButtonStream::holdDuration,
		"releaseDuration", &GameboyButtonStream::releaseDuration,
		"releaseAllDuration", &GameboyButtonStream::releaseAllDuration,

		"streamId", &GameboyButtonStream::getStreamId
	);

	s.new_usertype<Select>("Select", sol::base_classes, sol::bases<MenuItemBase>());
	s.new_usertype<Action>("Action", sol::base_classes, sol::bases<MenuItemBase>());
	s.new_usertype<MultiSelect>("MultiSelect", sol::base_classes, sol::bases<MenuItemBase>());
	s.new_usertype<Title>("Title", sol::base_classes, sol::bases<MenuItemBase>());
	s.new_usertype<Separator>("Separator", sol::base_classes, sol::bases<MenuItemBase>());
	s.new_usertype<Menu>("Menu", "addItem", &Menu::addItem, sol::base_classes, sol::bases<MenuItemBase>());

	s.create_named_table("_menuAlloc",
		"menu", [](const std::string& name, Menu* parent) { return new Menu(name, true, parent); },
		"title", [](const std::string& name) { return new Title(name); },
		"select", [](const std::string& name, bool checked, bool active, int id) { return new Select(name, checked, nullptr, active, id); },
		"action", [](const std::string& name, bool active, int id) { return new Action(name, nullptr, active, id); },
		"multiSelect", [](const sol::as_table_t<std::vector<std::string>>& items, int value, bool active, int id) { return new MultiSelect(items.value(), value, nullptr, active, id); },
		"separator", []() { return new Separator(); }
	);

	s.new_usertype<DataBuffer<char>>("DataBuffer",
		"get", &DataBuffer<char>::get,
		"set", &DataBuffer<char>::set,
		"slice", &DataBuffer<char>::slice,
		"toString", &DataBuffer<char>::toString,
		"hash", &DataBuffer<char>::hash,
		"size", &DataBuffer<char>::size,
		"clear", &DataBuffer<char>::clear,
		"resize", &DataBuffer<char>::resize,
		"reserve", &DataBuffer<char>::reserve
	);

	s["_RETROPLUG_VERSION"].set(PLUG_VERSION_STR);
	s["_consolePrint"].set_function(consoleLog);
}