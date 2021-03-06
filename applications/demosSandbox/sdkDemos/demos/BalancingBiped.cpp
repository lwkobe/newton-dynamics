/* Copyright (c) <2003-2019> <Newton Game Dynamics>
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
#include "DemoMesh.h"
#include "DemoCamera.h"
#include "PhysicsUtils.h"
#include "TargaToOpenGl.h"
#include "DemoEntityManager.h"
#include "DebugDisplay.h"
#include "HeightFieldPrimitive.h"

//#define D_BIPED_TEXTURE_NAME "marble.tga"
#define D_BIPED_MASS			80.0f
#define D_BIPED_TEXTURE_NAME	"metal_30.tga"

class dBalacingCharacterEffector: public dCustomKinematicController
{
	public:
	dBalacingCharacterEffector(NewtonBody* const body, NewtonBody* const referenceBody, const dMatrix& attachmentMatrixInGlobalSpace, dFloat modelMass)
		:dCustomKinematicController(body, attachmentMatrixInGlobalSpace, referenceBody)
		,m_origin(GetTargetMatrix())
	{
		// set the joint as exact solver
		SetSolverModel(1);
		//SetControlMode(dCustomKinematicController::m_linearAndTwist);
		//SetControlMode(dCustomKinematicController::m_linear);
		SetControlMode(dCustomKinematicController::m_full6dof);

		dFloat gravity = 10.0f;
		SetMaxAngularFriction(modelMass * 100.0f * gravity);
		SetMaxLinearFriction(modelMass * gravity * 1.2f);

		dVector euler0;
		dVector euler1;
		m_origin.GetEulerAngles(euler0, euler1);
		m_pitch = euler0.m_x;
		m_yaw = euler0.m_y;
		m_roll = euler0.m_z;
	}

	void SetMatrix(dFloat x, dFloat y, dFloat z, dFloat pitch)
	{
		dMatrix matrix(dPitchMatrix(m_pitch + pitch) * dYawMatrix(m_yaw) * dRollMatrix(m_roll));
		matrix.m_posit = m_origin.TransformVector(dVector(x, y, z, dFloat(1.0f)));
		SetTargetMatrix(matrix);
	}

	dMatrix m_origin;
	dFloat m_pitch;
	dFloat m_yaw;
	dFloat m_roll;
};

class dBalancingBiped: public dModelRootNode
{
	public:
	class dController
	{
		public:
		dController (dBalancingBiped* const biped)
			:m_biped(biped)
		{
		}

		virtual ~dController()
		{
		}

		virtual bool Update (dFloat timestep) = 0;

		dBalancingBiped* m_biped;
	};

	class dBalanceController: public dController
	{
		public:
		dBalanceController (dBalancingBiped* const biped)
			:dController(biped)
		{
		}

		virtual bool Update (dFloat timestep)
		{
			//int count = m_biped->CalculateSupportPolygon();
			m_biped->CalculateSupportPolygon();
			return true;
		}
	};

	dBalancingBiped(NewtonWorld* const world, const dMatrix& coordinateSystem, const dMatrix& location)
		:dModelRootNode(NULL, dGetIdentityMatrix())
		,m_comFrame(coordinateSystem)
		,m_localComFrame(coordinateSystem)
		,m_localGravity(0.0f)
		,m_localComVeloc(0.0f)
		,m_localSuportCenter(0.0f)
		,m_leftFoot(NULL)
		,m_rightFoot(NULL)
		,m_balanceController(this)
		,m_supportPolygonPoints(0)
		,m_polygonIsValid(false)
	{
		MakeHip(world, location);
		AddUpperBody (world);
		m_leftFoot = AddLeg (world, 0.15f);
		m_rightFoot = AddLeg (world, -0.15f);

		// normalize weight to 90 kilogram (about a normal human)
		NormalizeWeight (D_BIPED_MASS);

//NewtonBodySetMassMatrix(GetBody(), 0.0f, 0.0f, 0.0f, 0.0f);
	}

	void Debug(dCustomJoint::dDebugDisplay* const debugContext) 
	{
		// draw biped center of mass frame
		dMatrix matrix;
		NewtonBodyGetMatrix(GetBody(), &matrix[0][0]);
		debugContext->DrawFrame(m_comFrame);

		// draw support polygon
		if (m_supportPolygonPoints) {
			int i0 = m_supportPolygonPoints - 1;
			debugContext->SetColor (dVector (1.0f, 1.0f, 0.0f, 0.0f));
			for (int i = 0; i < m_supportPolygonPoints; i ++) {
				dVector p0 (m_comFrame.TransformVector (m_supportPolygon[i0]));
				dVector p1 (m_comFrame.TransformVector (m_supportPolygon[i]));
				p0.m_y += 0.02f;
				p1.m_y += 0.02f;
				debugContext->DrawLine(p0, p1);
				i0 = i;
			}
			dVector center(m_localSuportCenter);
			center.m_y += 0.02;
			center = m_comFrame.TransformVector(center);
			debugContext->DrawPoint(center, 4.0f);
		}

		//if (m_leftFoot) {
		//	m_leftFoot->Debug(debugContext);
		//}
		//if (m_rightFoot) {
		//	m_rightFoot->Debug(debugContext);
		//}
	}

	void Update(dFloat timestep)
	{
		// initialize data
		m_polygonIsValid = false;
		m_supportPolygonPoints = 0;
		CaculateComAndVelocity();

		m_balanceController.Update(timestep);
	}
	
	private:

	int GetContactPoints (NewtonBody* const body, dVector* const points) const
	{
		dVector point(0.0f);
		dVector normal(0.0f);
		int count = 0;
		for (NewtonJoint* contactJoint = NewtonBodyGetFirstContactJoint(body); contactJoint; contactJoint = NewtonBodyGetNextContactJoint(body, contactJoint)) {
			if (NewtonJointIsActive(contactJoint)) {
				for (void* contact = NewtonContactJointGetFirstContact(contactJoint); contact; contact = NewtonContactJointGetNextContact(contactJoint, contact)) {
					NewtonMaterial* const material = NewtonContactGetMaterial(contact);
					NewtonMaterialGetContactPositionAndNormal(material, body, &point.m_x, &normal.m_x);
					points[count] = point;
					count ++;
				}
			}
		}
		return count;
	}

	int CalculateSupportPolygon ()
	{
		if (m_polygonIsValid) {
			return 0; 
		}
		m_polygonIsValid = true;
		m_supportPolygonPoints = 0;

		int count = 0;
		count += GetContactPoints (m_leftFoot->GetBody0(), &m_supportPolygon[count]);
		count += GetContactPoints (m_rightFoot->GetBody0(), &m_supportPolygon[count]);
		if (count) {
			count = Calculate2dConvexHullProjection (count, m_supportPolygon);
			dVector center (0.0f);
			for (int i = 0; i < count; i ++) {
				m_supportPolygon[i] = m_comFrame.UntransformVector(m_supportPolygon[i]);
				center += m_supportPolygon[i];
			}
			m_localSuportCenter = center.Scale (1.0f / count);
		}

		m_supportPolygonPoints = count;
		return count;
	}

	void CaculateComAndVelocity()
	{
		dMatrix matrix;
		NewtonBodyGetMatrix(GetBody(), &matrix[0][0]);

		// calculate the local frame of center of mass
		m_comFrame = m_localComFrame * matrix;

		m_localComVeloc = dVector(0.0f);
		m_comFrame.m_posit = dVector(0.0f);
		ForEachNode((dModelNode::Callback)&dBalancingBiped::CaculateComAndVelocity, NULL);
		
		m_comFrame.m_posit = m_comFrame.m_posit.Scale (1.0f / D_BIPED_MASS);
		m_comFrame.m_posit.m_w = 1.0f;
		m_localComFrame.m_posit = matrix.UntransformVector(m_comFrame.m_posit);
		m_localComVeloc = m_comFrame.UnrotateVector(m_localComVeloc.Scale (1.0f / D_BIPED_MASS));
		m_localGravity = m_comFrame.UnrotateVector(dVector (0.0f, -1.0f, 0.0f, 0.0f));
	}

	void NormalizeWeight (dFloat mass)
	{
		dFloat totalMass = 0.0f;
		ForEachNode((dModelNode::Callback)&dBalancingBiped::NormalizeMassCallback, &totalMass);

		dFloat normalizeMassScale = mass / totalMass;
		ForEachNode((dModelNode::Callback)&dBalancingBiped::ApplyNormalizeMassCallback, &normalizeMassScale);
	}

	void CaculateComAndVelocity(const dModelNode* const node, void* const context)
	{
		dMatrix matrix;
		dVector com (0.0f);
		dVector veloc (0.0f);
		dFloat Ixx;
		dFloat Iyy;
		dFloat Izz;
		dFloat mass;
		NewtonBody* const body = node->GetBody();

		NewtonBodyGetVelocity(body, &veloc[0]);
		NewtonBodyGetCentreOfMass(body, &com[0]);
		NewtonBodyGetMatrix(body, &matrix[0][0]);
		NewtonBodyGetMass(body, &mass, &Ixx, &Iyy, &Izz);
		
		com.m_w = 1.0f;
		m_localComVeloc += veloc.Scale (mass);
		m_comFrame.m_posit += matrix.TransformVector(com).Scale(mass);
	}

	void NormalizeMassCallback (const dModelNode* const node, void* const context) const
	{
		dFloat Ixx;
		dFloat Iyy;
		dFloat Izz;
		dFloat mass;
		dFloat* const totalMass = (dFloat*) context;
		NewtonBodyGetMass(node->GetBody(), &mass, &Ixx, &Iyy, &Izz);
		*totalMass += mass;
	}

	void ApplyNormalizeMassCallback (const dModelNode* const node, void* const context) const
	{
		dFloat Ixx;
		dFloat Iyy;
		dFloat Izz;
		dFloat mass;
		dFloat scale = *((dFloat*) context);
		NewtonBodyGetMass(node->GetBody(), &mass, &Ixx, &Iyy, &Izz);

		mass *= scale;
		Ixx *= scale;
		Iyy *= scale;
		Izz *= scale;
		NewtonBodySetMassMatrix(node->GetBody(), mass, Ixx, Iyy, Izz);
	}

	void MakeHip(NewtonWorld* const world, const dMatrix& location)
	{
		DemoEntityManager* const scene = (DemoEntityManager*)NewtonWorldGetUserData(world);

		dVector size (0.25f, 0.2f, 0.25f, 0.0f);
		NewtonCollision* const collision = CreateConvexCollision (world, dGetIdentityMatrix(), size, _CAPSULE_PRIMITIVE, 0);
		DemoMesh* const geometry = new DemoMesh("hip", scene->GetShaderCache(), collision, D_BIPED_TEXTURE_NAME, D_BIPED_TEXTURE_NAME, D_BIPED_TEXTURE_NAME);
	
		m_body = CreateSimpleSolid (scene, geometry, 100.0f, location, collision, 0);

		dMatrix matrix;
		NewtonBodyGetMatrix(GetBody(), &matrix[0][0]);

		// calculate the local frame of center of mass
		m_localComFrame = m_comFrame * matrix.Inverse();
		m_localComFrame.m_posit = dVector (0.0f);
		m_localComFrame.m_posit.m_w = 1.0f;
		m_comFrame = m_localComFrame * matrix;

		geometry->Release();
		NewtonDestroyCollision(collision);
	}

	void AddUpperBody(NewtonWorld* const world)
	{
		dMatrix matrix;
		NewtonBodyGetMatrix (m_body, &matrix[0][0]);

		DemoEntityManager* const scene = (DemoEntityManager*)NewtonWorldGetUserData(world);
		dVector size(0.3f, 0.4f, 0.2f, 0.0f);
		NewtonCollision* const collision = CreateConvexCollision(world, dGetIdentityMatrix(), size, _BOX_PRIMITIVE, 0);
		DemoMesh* const geometry = new DemoMesh("torso", scene->GetShaderCache(), collision, D_BIPED_TEXTURE_NAME, D_BIPED_TEXTURE_NAME, D_BIPED_TEXTURE_NAME);

		dFloat hipRadius = 0.25f * 0.5f;
		dMatrix location (matrix);
		location.m_posit += matrix.m_up.Scale (hipRadius + size.m_y * 0.5f);
		NewtonBody* const torsoBody = CreateSimpleSolid(scene, geometry, 100.0f, location, collision, 0);

		geometry->Release();
		NewtonDestroyCollision(collision);

		dMatrix jointMatrix (matrix);
		jointMatrix.m_posit += matrix.m_up.Scale (hipRadius);
		dCustomHinge* const fixJoint = new dCustomHinge(jointMatrix, torsoBody, GetBody());
		fixJoint->EnableLimits(true);
		fixJoint->SetLimits(0.0f, 0.0f);
		new dModelNode(torsoBody, dGetIdentityMatrix(), this);
	}

	dBalacingCharacterEffector* AddLeg(NewtonWorld* const world, dFloat dist)
	{
		dMatrix matrix;
		NewtonBodyGetMatrix(m_body, &matrix[0][0]);

		// create capsule collision and mesh
		DemoEntityManager* const scene = (DemoEntityManager*)NewtonWorldGetUserData(world);
		dVector size (0.15f, 0.4f, 0.15f, 0.0f);
		NewtonCollision* const collision = CreateConvexCollision(world, dGetIdentityMatrix(), size, _CAPSULE_PRIMITIVE, 0);
		DemoMesh* const geometry = new DemoMesh("leg", scene->GetShaderCache(), collision, D_BIPED_TEXTURE_NAME, D_BIPED_TEXTURE_NAME, D_BIPED_TEXTURE_NAME);

		// create upper leg body and visual entity
		dMatrix location(matrix);
		location.m_posit += matrix.m_front.Scale(dist);
		location.m_posit -= matrix.m_up.Scale (size.m_y - size.m_x * 0.5f);
		location = dRollMatrix (90.0f * dDegreeToRad) * location;

		// flex the leg 10 degree to the front.
		dMatrix tiltLegMatrix (dPitchMatrix(-10.0f * dDegreeToRad));
		// get hip pivot point
		dVector jointPivot (location.m_posit + location.m_front.Scale(size.m_y * 0.5f + 0.2f * 0.5f));
		tiltLegMatrix.m_posit = jointPivot - tiltLegMatrix.RotateVector(jointPivot);
		location = location * tiltLegMatrix;
		NewtonBody* const legBody = CreateSimpleSolid(scene, geometry, 20.0f, location, collision, 0);

		// create leg-hip joint
		dMatrix jointMatrix(location);
		jointMatrix.m_posit = jointPivot;
		new dCustomBallAndSocket(jointMatrix, legBody, GetBody());
		dModelNode* const legBone = new dModelNode(legBody, dGetIdentityMatrix(), this);

		// create shin body and visual entity
		location.m_posit -= location.m_front.Scale (size.m_y + size.m_x * 0.5f);

		// flex shin 20 degrees backward
		dMatrix tiltShinMatrix (dPitchMatrix(20.0f * dDegreeToRad));
		// get the knee pivot
		dVector kneePivot (location.m_posit + location.m_front.Scale(size.m_y * 0.5f + size.m_x * 0.25f));
		tiltShinMatrix.m_posit = kneePivot - tiltShinMatrix.RotateVector(kneePivot);
		location = location * tiltShinMatrix;
		NewtonBody* const shinBody = CreateSimpleSolid(scene, geometry, 15.0f, location, collision, 0);

		// get the knee pivot matrix
		jointMatrix = location;
		jointMatrix = dRollMatrix (90.0f * dDegreeToRad) * jointMatrix;
		jointMatrix.m_posit = kneePivot;
		// create knee joint
		dCustomHinge* const kneeHinge = new dCustomHinge(jointMatrix, shinBody, legBody);
		kneeHinge->EnableLimits(true);
		kneeHinge->SetLimits(-120.0f * dDegreeToRad, 10.0f * dDegreeToRad);
		dModelNode* const shinBone = new dModelNode(shinBody, dGetIdentityMatrix(), legBone);

		// release collision and visual mesh
		geometry->Release();
		NewtonDestroyCollision(collision);

		// create a box to represent the foot
		dVector footSize (0.07f, 0.15f, 0.25f, 0.0f);
		NewtonCollision* const footCollision = CreateConvexCollision(world, dGetIdentityMatrix(), footSize, _BOX_PRIMITIVE, 0);
		DemoMesh* const footGeometry = new DemoMesh("foot", scene->GetShaderCache(), collision, D_BIPED_TEXTURE_NAME, D_BIPED_TEXTURE_NAME, D_BIPED_TEXTURE_NAME);

		// create foot body and visual entity
		location.m_posit -= location.m_front.Scale(size.m_y * 0.5f + size.m_x * 0.5f);
		location.m_posit += location.m_right.Scale(footSize.m_z * 0.25f);

		// get he ankle pivot
		dVector anklePivot (location.m_posit - location.m_right.Scale(footSize.m_z * 0.25f) + location.m_front.Scale(footSize.m_x * 0.5f));
		// flex the foot 10 degrees forward
		dMatrix tiltAnkleMatrix (dPitchMatrix(-10.0f * dDegreeToRad));
		tiltAnkleMatrix.m_posit = anklePivot - tiltAnkleMatrix.RotateVector(anklePivot);
		location = location * tiltAnkleMatrix;
		NewtonBody* const footBody = CreateSimpleSolid(scene, footGeometry, 10.0f, location, footCollision, 0);

		jointMatrix = location;
		jointMatrix.m_posit = anklePivot;
		dMatrix effectorMatrix(jointMatrix);

		jointMatrix = dRollMatrix(90.0f * dDegreeToRad) * jointMatrix;
		jointMatrix = dPitchMatrix(90.0f * dDegreeToRad) * jointMatrix;
		new dCustomDoubleHinge(jointMatrix, footBody, shinBody);
		dModelNode* const footAnkleBone = new dModelNode(footBody, dGetIdentityMatrix(), shinBone);

		// release collision and visual mesh
		footGeometry->Release();
		NewtonDestroyCollision(footCollision);

		// save ankle matrix as the effector pivot 
		//return 0;
		return new dBalacingCharacterEffector(footAnkleBone->GetBody(), m_body, effectorMatrix, D_BIPED_MASS);
	}

	dMatrix m_comFrame;
	dMatrix m_localComFrame;
	dVector m_localGravity;
	dVector m_localComVeloc;
	dVector m_localSuportCenter;
	dVector m_supportPolygon[32];
	dBalacingCharacterEffector* m_leftFoot;
	dBalacingCharacterEffector* m_rightFoot;
	dBalanceController m_balanceController;

	int m_supportPolygonPoints;
	bool m_polygonIsValid;
};


class dBalancingBipedManager: public dModelManager
{
	public:
	dBalancingBipedManager(DemoEntityManager* const scene)
		:dModelManager(scene->GetNewton())
	{
		//scene->SetUpdateCameraFunction(UpdateCameraCallback, this);
		//scene->Set2DDisplayRenderFunction(RenderPlayerHelp, NULL, this);

		// create a material for early collision culling
		NewtonWorld* const world = scene->GetNewton();
		int material = NewtonMaterialGetDefaultGroupID(world);

		NewtonMaterialSetCallbackUserData(world, material, material, this);
		NewtonMaterialSetDefaultElasticity(world, material, material, 0.0f);
		NewtonMaterialSetDefaultFriction(world, material, material, 0.9f, 0.9f);
	}

	dBalancingBiped* CreateBiped (const dMatrix& location)
	{
		dMatrix coordinateSystem (dYawMatrix(-90.0f * dDegreeToRad) * location);
		dBalancingBiped* const biped = new dBalancingBiped(GetWorld(), coordinateSystem, location);
		AddRoot (biped);
		return biped;
	}

	virtual void OnUpdateTransform(const dModelNode* const bone, const dMatrix& globalMatrix) const
	{
		// this function is no called because the the model is hierarchical 
		// but all body parts are in global space, therefore the normal rigid body transform callback
		// set the transform after each update.
	}

	virtual void OnDebug(dModelRootNode* const model, dCustomJoint::dDebugDisplay* const debugContext)
	{
		dBalancingBiped* const biped = (dBalancingBiped*)model;
		biped->Debug (debugContext);
	}

	virtual void OnPreUpdate(dModelRootNode* const model, dFloat timestep) const
	{
		dBalancingBiped* const biped = (dBalancingBiped*)model;
		biped->Update(timestep);
	}
};


void BalancingBiped(DemoEntityManager* const scene)
{
	// load the sky box
	scene->CreateSkyBox();

	CreateLevelMesh(scene, "flatPlane.ngd", true);

	dBalancingBipedManager* const manager = new dBalancingBipedManager(scene);

	dMatrix origin (dYawMatrix(0.0f * dDegreeToRad));
	origin.m_posit.m_y += 1.2f;
	manager->CreateBiped(origin);

	origin.m_posit.m_x = -2.5f;
	origin.m_posit.m_y = 1.5f;
	origin.m_posit.m_z = 2.0f;
	origin = dYawMatrix(45.0f * dDegreeToRad) * origin;
	dQuaternion rot (origin);
	scene->SetCameraMatrix(rot, origin.m_posit);
}
