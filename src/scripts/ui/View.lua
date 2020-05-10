local Model = require("Model")
local KeyFilter = require("KeyFilter")
local Menu = require("Menu")
local mainMenu = require("MainMenu")
local createNativeMenu = require("MenuHelper")
local dialog = require("dialog")
local fs = require("fs")

local class = require("class")
local View = class()
function View:init()
	self._keyFilter = KeyFilter()
	self._menuLookup = nil
end

function View:setup(view, audioContext)
	self.view = view;
	self.model = Model(audioContext)
	fs.__setup(audioContext:getFileManager())
	dialog.__setup(view)
end

function View:onKey(key, down)
	if self._keyFilter:onKey(key, down) == true then
		self.model.project:onKey(key, down)
	end
end

function View:onDoubleClick(x, y, mod)
	if #self.model.project.systems == 0 then
		mainMenu.loadProjectOrRom(self.model.project)()
	end
end

function View:onMouseDown(x, y, mod)
	--self:selectViewAtPos(x, y)

	if mod.right == true then
		local menu = Menu()
		mainMenu.generateMenu(menu, self.model.project)

		-- Get component menus
		-- Get audio context menus

		self._menuLookup = {}
		local nativeMenu = createNativeMenu(menu, nil, LUA_MENU_ID_OFFSET, self._menuLookup, true)
		self.view:requestMenu(nativeMenu)
	end
end

function View:onMenuResult(idx)
	if self._menuLookup ~= nil then
		local callback = self._menuLookup[idx]
		if callback ~= nil then
			callback()
		end

		self._menuLookup = nil
	end
end

function View:onDialogResult(paths)
	dialog.__onResult(paths)
end

function View:onPadButton(button, down)
	print(button, down)
end

function View:onDrop(x, y, items)
	print(x, y, items)
end

return View
