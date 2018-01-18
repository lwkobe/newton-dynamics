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


#include "toolbox_stdafx.h"
#include "SkyBox.h"
#include "DemoEntityManager.h"
#include "DemoCamera.h"
#include "PhysicsUtils.h"
#include "DemoMesh.h"
#include "OpenGlUtil.h"

/*
void PrecessingTops (DemoEntityManager* const scene)
{
	scene->CreateSkyBox();

	dMatrix offsetMatrix (dGetIdentityMatrix());

	CreateLevelMesh (scene, "flatPlane.ngd", 1);

	dVector location (0.0f);
	dVector size (3.0f, 2.0f, 0.0f, 0.0f);

	NewtonWorld* const world = scene->GetNewton();
	int defaultMaterialID = NewtonMaterialGetDefaultGroupID(world);
	NewtonMaterialSetDefaultFriction(world, defaultMaterialID, defaultMaterialID, 1.0f, 1.0f);
	NewtonMaterialSetDefaultElasticity(world, defaultMaterialID, defaultMaterialID, 0.1f);

	// create an array of cones 
//	const int count = 10;
const int count = 1;

	// all shapes use the x axis as the  axis of symmetry, to make an upright cone we apply a 90 degree rotation local matrix
	dMatrix shapeOffsetMatrix (dRollMatrix(-3.141592f/2.0f));
	AddPrimitiveArray(scene, 10.0f, location, size, count, count, 5.0f, _CONE_PRIMITIVE, 0, shapeOffsetMatrix);

	// till the cont 30 degrees, and apply a local high angular velocity
	dMatrix matrix (dRollMatrix (-25.0f * 3.141592f / 180.0f));
	dVector omega (10.0f, 50.0f, 0.0f);
	omega = matrix.RotateVector (omega);
	dVector damp (0.0f);

	int topscount = 0;
	NewtonBody* array[count * count];
	for (NewtonBody* body = NewtonWorldGetFirstBody(world); body; body = NewtonWorldGetNextBody(world, body)) {
		NewtonCollision* const collision = NewtonBodyGetCollision(body);
		if (NewtonCollisionGetType (collision) == SERIALIZE_ID_CONE) {
			array[topscount] = body;
			topscount ++;
		}
	}

	for (int i = 0; i < topscount ; i ++) {
		dMatrix bodyMatrix;
		NewtonBody* const body = array[i];
		NewtonBodyGetMatrix(body, &bodyMatrix[0][0]);
		matrix.m_posit = bodyMatrix.m_posit;
		matrix.m_posit.m_y += 1.0f; 
		NewtonBodySetMatrix(body, &matrix[0][0]);
		NewtonBodySetForceAndTorqueCallback(body, PhysicsApplyGravityForce);

		NewtonCollision* const shape = NewtonBodyGetCollision(body);
		NewtonBodySetMassProperties (body, 5.0f, shape);

		dFloat m;
		dFloat x;
		dFloat y;
		dFloat z;
		NewtonBodyGetMass(body, &m, &x, &y, &z);
		x = 1.0f;
		z = 1.0f;
		y = 3.0f;
//		NewtonBodySetMassMatrix(body, m, x, y, z);

		NewtonBodySetOmega (body, &omega[0]);
		NewtonBodySetAutoSleep (body, 0);
		NewtonBodySetLinearDamping(body, 0.0f);
		NewtonBodySetAngularDamping (body, &damp[0]);
	}

	// place camera into position
	dMatrix camMatrix (dGetIdentityMatrix());
	dQuaternion rot (camMatrix);
	dVector origin (-40.0f, 5.0f, 0.0f, 0.0f);
	scene->SetCameraMatrix(rot, origin);
}
*/

static NewtonBody* CreateFryWheel (DemoEntityManager* const scene, const dVector& posit, dFloat speed, dFloat radius, dFloat lenght)
{
	NewtonWorld* const world = scene->GetNewton();

	dMatrix offset(dGetIdentityMatrix());
	offset.m_posit.m_x = lenght * 0.5f;
	NewtonCollision* const rod = NewtonCreateCapsule(world, 0.0625f * 0.5f, 0.0625f * 0.5f, lenght, 0, NULL);
	NewtonCollision* const wheel = NewtonCreateCylinder(world, radius, radius, 0.125f, 0, &offset[0][0]);

	NewtonCollision* const flyWheelShape = NewtonCreateCompoundCollision(world, 0);
	NewtonCompoundCollisionBeginAddRemove(flyWheelShape);

	NewtonCompoundCollisionAddSubCollision(flyWheelShape, rod);
	NewtonCompoundCollisionAddSubCollision(flyWheelShape, wheel);
	NewtonCompoundCollisionEndAddRemove(flyWheelShape);

	dMatrix matrix(dGetIdentityMatrix());
	matrix.m_posit = posit;
	matrix.m_posit.m_x += lenght * 0.5f;
	matrix.m_posit.m_w = 1.0f;

	DemoMesh* const geometry = new DemoMesh("primitive", flyWheelShape, "smilli.tga", "smilli.tga", "smilli.tga");
	NewtonBody* const wheelBody = CreateSimpleSolid(scene, geometry, 10.0f, matrix, flyWheelShape, 0);
	NewtonBodySetMassProperties(wheelBody, 10.0f, flyWheelShape);

	dFloat m;
	dFloat x;
	dFloat y;
	dFloat z;
	NewtonBodyGetMass(wheelBody, &m, &x, &y, &z);

	dVector damp(0.0f);
	NewtonBodySetLinearDamping(wheelBody, 0.0f);
	NewtonBodySetAngularDamping(wheelBody, &damp[0]);

	dVector omega(speed, 0.0f, 0.0f);
	NewtonBodySetOmega(wheelBody, &omega[0]);

	matrix.m_posit.m_x -= lenght * 0.5f;

	geometry->Release();
	NewtonDestroyCollision(flyWheelShape);
	NewtonDestroyCollision(wheel);
	NewtonDestroyCollision(rod);

	return wheelBody;
}


static void CreateBicycleWheel(DemoEntityManager* const scene, const dVector& posit, dFloat speed, dFloat radius, dFloat lenght)
{
	NewtonBody* const flyWheel = CreateFryWheel(scene, posit, speed, radius, lenght);

	dMatrix matrix(dGetIdentityMatrix());
	matrix.m_posit = posit;

	dCustom6dof* const fixPoint = new dCustom6dof(matrix, flyWheel, NULL);
	fixPoint->DisableRotationX();
	fixPoint->DisableRotationY();
	fixPoint->DisableRotationZ();
}


void GyroscopyPrecession(DemoEntityManager* const scene)
{
	scene->CreateSkyBox();
	dMatrix offsetMatrix(dGetIdentityMatrix());

	CreateLevelMesh(scene, "flatPlane.ngd", 1);

	// should spins very slowly
	CreateBicycleWheel(scene, dVector (0.0f, 5.0f, 0.0f, 1.0f), 100.0f, 0.6f, 0.3f);

	// spin twice as fast
	CreateBicycleWheel(scene, dVector (0.0f, 5.0f, -2.0f, 1.0f), 50.0f, 0.6f, 0.3f);

	// should just flops
	CreateBicycleWheel(scene, dVector (0.0f, 5.0f, 2.0f, 1.0f), 0.0f, 0.6f, 0.3f);

	// place camera into position
	dMatrix camMatrix(dGetIdentityMatrix());
	dQuaternion rot(camMatrix);
	dVector origin(-10.0f, 5.0f, 0.0f, 0.0f);
	scene->SetCameraMatrix(rot, origin);
}



