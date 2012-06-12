function parseMaxSpeed(maxspeedTag)
	print_string('parsing: ' .. maxspeedTag)
	return 1;
end

function parseWay(currentWay)
	highway = currentWay:getTag('highway')
	name = currentWay:getTag('name')
	ref = currentWay:getTag('ref')
	junction= currentWay:getTag('junction')
	route = currentWay:getTag('route')
	
	if highway ~= '' then
		print_string(highway)
		--return true
	end
	
	if name ~= '' then
		print_string(' '..name)
	else
		print_string(' '..'unbekannt')
	end
	
	maxspeed = 0
	maxSpeedTag = currentWay:getTag('maxspeed')
	if not maxSpeedTag == nil then
		print(maxSpeedTag)
		if string.len(maxSpeedTag) > 1 then
			print_string(maxSpeedTag)
			maxspeed = parseMaxspeed(maxSpeedTag)
		end
	end
	access= currentWay:getTag('access')
	accessTag= currentWay:getTag('motorcar')
	man_made= currentWay:getTag('man_made')
	barrier= currentWay:getTag('barrier')
	oneway= currentWay:getTag('oneway')
	duration = currentWay:getTag('duration')
	service = currentWay:getTag('service')
	area = currentWay:getTag('area')
	print_string('finished parsing way')

	return false

end
