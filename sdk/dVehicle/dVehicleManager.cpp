/* Copyright (c) <2003-2016> <Newton Game Dynamics>
* 
* This software is provided 'as-is', without any express or implied
* warranty. In no event will the authors be held liable for any damages
* arising from the use of this software.
* 
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely
*/

#include "dStdafxVehicle.h"
#include "dVehicleManager.h"


dVehicleManager::dVehicleManager(NewtonWorld* const world)
	:dCustomControllerManager<dVehicleChassis>(world, D_VEHICLE_MANAGER_NAME)
{

}


dVehicleManager::~dVehicleManager()
{

}


