local _factory = { global = {}, instance = {} }
local _globalComponents = {}

local _allComponents = {}

local function getComponentNamesMap()
	local names = {}
	for _, componentType in ipairs(_factory.instance) do
		names[componentType.__desc.name] = true
	end

	return names
end

local function loadComponent(name)
	local component = require(name)
	if component ~= nil then
		print("Registered component: " .. component.__desc.name)
		if component.__desc.global == true then
			table.insert(_factory.global, component)
		else
			table.insert(_factory.instance, component)
		end
	else
		print("Failed to load " .. name .. ": Script does not return a component")
	end
end

local function createComponent(system, name)
	for _, componentType in ipairs(_factory.instance) do
		local d = componentType.__desc
		if d.name == name then
			local valid, ret = pcall(componentType.new, system)
			if valid then
				table.insert(_allComponents, ret)
				return ret
			else
				print("Failed to load component: " .. ret)
			end
		end
	end
end

local function createComponents(system)
	local desc = system:desc()
	local components = {}
	for _, componentType in ipairs(_factory.instance) do
		local d = componentType.__desc
		if d.romName ~= nil and desc.romName:find(d.romName) ~= nil then
			print("Attaching component " .. d.name)
			local valid, ret = pcall(componentType.new, system)
			if valid then
				table.insert(components, ret)
				table.insert(_allComponents, ret)
			else
				print("Failed to load component: " .. ret)
			end
		end
	end

	return components
end

local function runComponentHandlers(target, components, ...)
	local handled = false
	if components ~= nil then
		for _, v in ipairs(components) do
			local found = v[target]
			if found ~= nil then
				found(v, ...)
				handled = true
			end
		end
	end

	return handled
end

local function runGlobalHandlers(target, ...)
    return runComponentHandlers(target, _globalComponents, ...)
end

local function runAllHandlers(target, components, ...)
    local handled = runGlobalHandlers(target, ...)
    if components ~= nil then handled = runComponentHandlers(target, components, ...) end
    return handled
end

local function createGlobalComponents()
    for _, v in ipairs(_factory.global) do
		table.insert(_globalComponents, v.new())
	end
end

return {
	getComponentNamesMap = getComponentNamesMap,
	loadComponent = loadComponent,
	createComponent = createComponent,
    createComponents = createComponents,
    runComponentHandlers = runComponentHandlers,
    runGlobalHandlers = runGlobalHandlers,
    runAllHandlers = runAllHandlers,
	createGlobalComponents = createGlobalComponents,
	allComponents = _allComponents
}