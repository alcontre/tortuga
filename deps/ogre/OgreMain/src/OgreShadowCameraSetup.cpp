/*
-----------------------------------------------------------------------------
This source file is part of OGRE
(Object-oriented Graphics Rendering Engine)
For the latest info, see http://www.ogre3d.org/

Copyright (c) 2006  Torus Knot Software Ltd
Also see acknowledgements in Readme.html

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place - Suite 330, Boston, MA 02111-1307, USA, or go to
http://www.gnu.org/copyleft/lesser.txt.
-----------------------------------------------------------------------------
*/

#include "OgreStableHeaders.h"
#include "OgreCommon.h"
#include "OgreSceneManager.h"
#include "OgreLight.h"
#include "OgreShadowCameraSetup.h"
#include "OgreNumerics.h"
#include "OgreCamera.h"
#include "OgreViewport.h"


namespace Ogre 
{
	/// Default constructor
	DefaultShadowCameraSetup::DefaultShadowCameraSetup()  {}
	
	/// Destructor
	DefaultShadowCameraSetup::~DefaultShadowCameraSetup() {}
	
	/// Default shadow camera setup implementation
	void DefaultShadowCameraSetup::getShadowCamera (const SceneManager *sm, const Camera *cam, 
		const Viewport *vp, const Light *light, Camera *texCam) const
	{
		Vector3 pos, dir;

		// get the shadow frustum's far distance
		Real shadowDist = sm->getShadowFarDistance();
		if (!shadowDist)
		{
			// need a shadow distance, make one up
			shadowDist = cam->getNearClipDistance() * 300;
		}
		Real shadowOffset = shadowDist * (sm->getShadowDirLightTextureOffset());

		// Directional lights 
		if (light->getType() == Light::LT_DIRECTIONAL)
		{
			// set up the shadow texture
			// Set ortho projection
			texCam->setProjectionType(PT_ORTHOGRAPHIC);
			// set easy FOV and near dist so that texture covers far dist
			texCam->setFOVy(Degree(90));
			texCam->setNearClipDistance(shadowDist);

			// Calculate look at position
			// We want to look at a spot shadowOffset away from near plane
			// 0.5 is a litle too close for angles
			Vector3 target = cam->getDerivedPosition() + 
				(cam->getDerivedDirection() * shadowOffset);

			// Calculate direction, which same as directional light direction
			dir = - light->getDerivedDirection(); // backwards since point down -z
			dir.normalise();

			// Calculate position
			// We want to be in the -ve direction of the light direction
			// far enough to project for the dir light extrusion distance
			pos = target + dir * sm->getShadowDirectionalLightExtrusionDistance();

			// Round local x/y position based on a world-space texel; this helps to reduce
			// jittering caused by the projection moving with the camera
			// Viewport is 2 * near clip distance across (90 degree fov)
			Real worldTexelSize = (texCam->getNearClipDistance() * 20) / vp->getActualWidth();
			pos.x -= fmod(pos.x, worldTexelSize);
			pos.y -= fmod(pos.y, worldTexelSize);
			pos.z -= fmod(pos.z, worldTexelSize);
		}
		// Spotlight
		else if (light->getType() == Light::LT_SPOTLIGHT)
		{
			// Set perspective projection
			texCam->setProjectionType(PT_PERSPECTIVE);
			// set FOV slightly larger than the spotlight range to ensure coverage
			texCam->setFOVy(light->getSpotlightOuterAngle()*1.2);
			// set near clip the same as main camera, since they are likely
			// to both reflect the nature of the scene
			texCam->setNearClipDistance(cam->getNearClipDistance());

			// Calculate position, which same as spotlight position
			pos = light->getDerivedPosition();

			// Calculate direction, which same as spotlight direction
			dir = - light->getDerivedDirection(); // backwards since point down -z
			dir.normalise();
		}
		// Point light
		else
		{
			// Set perspective projection
			texCam->setProjectionType(PT_PERSPECTIVE);
			// Use 120 degree FOV for point light to ensure coverage more area
			texCam->setFOVy(Degree(120));
			// set near clip the same as main camera, since they are likely
			// to both reflect the nature of the scene
			texCam->setNearClipDistance(cam->getNearClipDistance());

			// Calculate look at position
			// We want to look at a spot shadowOffset away from near plane
			// 0.5 is a litle too close for angles
			Vector3 target = cam->getDerivedPosition() + 
				(cam->getDerivedDirection() * shadowOffset);

			// Calculate position, which same as point light position
			pos = light->getDerivedPosition();

			dir = (pos - target); // backwards since point down -z
			dir.normalise();
		}

		// Finally set position
		texCam->setPosition(pos);

		// Calculate orientation based on direction calculated above
		/*
		// Next section (camera oriented shadow map) abandoned
		// Always point in the same direction, if we don't do this then
		// we get 'shadow swimming' as camera rotates
		// As it is, we get swimming on moving but this is less noticeable

		// calculate up vector, we want it aligned with cam direction
		Vector3 up = cam->getDerivedDirection();
		// Check it's not coincident with dir
		if (up.dotProduct(dir) >= 1.0f)
		{
		// Use camera up
		up = cam->getUp();
		}
		*/
		Vector3 up = Vector3::UNIT_Y;
		// Check it's not coincident with dir
		if (Math::Abs(up.dotProduct(dir)) >= 1.0f)
		{
			// Use camera up
			up = Vector3::UNIT_Z;
		}
		// cross twice to rederive, only direction is unaltered
		Vector3 left = dir.crossProduct(up);
		left.normalise();
		up = dir.crossProduct(left);
		up.normalise();
		// Derive quaternion from axes
		Quaternion q;
		q.FromAxes(left, up, dir);
		texCam->setOrientation(q);
	}

	// --------------------------------------------------------------------

	Matrix4 PlaneOptimalShadowCameraSetup::computeConstrainedProjection( const Vector4& pinhole, 
																		 const std::vector<Vector4>& fpoint, 
																		 const std::vector<Vector2>& constraint) const
	{
		// NOTE: will assume the z coordinates should be decided such that
		// the first 3 points (in fpoint) will have post projective
		// z coordinates of about +1 and the 4th (in fpoint) will have a 
		// post projective z coordinate of about -1.

		// TODO: could use SVD to avoid arbitrarily choosing one 
		// matrix element to be 1.0 (and thereby fix the scale).

		Matrix4 ret;
		int i;
		bool incrPrecision = false; // use to control numerical solving

		if(fpoint.size() < 4 || constraint.size() < 4) {
			return Matrix4::IDENTITY;
		}

		// allocate memory
		PreciseReal **mat = NULL;
		PreciseReal **backmat = NULL;
		{
			mat = new PreciseReal*[11];
			if(incrPrecision)
				backmat = new PreciseReal*[11];
			for(i=0; i<11; i++) 
			{
				mat[i] = new PreciseReal[11];
				if(incrPrecision)
					backmat[i] = new PreciseReal[11];
			}
		}

		// set up linear system to solve for all rows of projective matrix
		// except for the 3rd which corresponds to mapping of z values

		// we choose a nonzero element of the last row to set to the arbitrary
		// constant 1.0.
		int nzind = 3;
		PreciseReal col[11];
		PreciseReal backcol[11];

		// fill in light position constraints
		mat[0][0] = pinhole.x;
		mat[0][1] = pinhole.y;
		mat[0][2] = pinhole.z;
		mat[0][3] = 1.0;
		for(i=4; i<11; i++)
			mat[0][i] = 0.0;
		col[0] = 0.0;

		for(i=0; i<11; i++)
			mat[1][i] = 0.0;
		mat[1][4] = pinhole.x;
		mat[1][5] = pinhole.y;
		mat[1][6] = pinhole.z;
		mat[1][7] = 1.0;
		col[1] = 0.0;

		PreciseReal larr[4];
		larr[0] = pinhole.x;
		larr[1] = pinhole.y;
		larr[2] = pinhole.z;
		larr[3] = 1.0;
		for(i=0; i<8; i++)
			mat[2][i] = 0.0;
		int ind = 8;
		for(i=0; i<4; i++)
		{
			if(nzind == i)
				continue;
			mat[2][ind++] = larr[i];
		}
		col[2] = -larr[nzind];

		// fill in all the other constraints
		int row=3;
		for(i=0; i<4; i++)
		{
			int j;
			larr[0] = fpoint[i].x;
			larr[1] = fpoint[i].y;
			larr[2] = fpoint[i].z;
			larr[3] = fpoint[i].w;

			// lexel s coordinate constraint
			for(j=0; j<4; j++)
				mat[row][j] = larr[j];
			for(j=4; j<8; j++)
				mat[row][j] = 0.0;
			ind=8;
			for(j=0; j<4; j++)
			{
				if(nzind==j)
					continue;
				mat[row][ind++] = larr[j] * (-constraint[i].x);
			}
			col[row] = larr[nzind] * constraint[i].x;
			++row;

			// lexel t coordinate constraint
			for(j=0; j<4; j++)
				mat[row][j] = 0.0;
			for(j=4; j<8; j++)
				mat[row][j] = larr[j-4];

			ind=8;
			for(j=0; j<4; j++)
			{
				if(nzind==j)
					continue;
				mat[row][ind++] = larr[j] * (-constraint[i].y);
			}
			col[row] = larr[nzind] * constraint[i].y;
			++row;
		}

		// copy the matrix and vector for later computation
		if(incrPrecision)
		{
			for (i=0; i<11; i++)
			{
				for(int j=0; j<11; j++)
					backmat[i][j] = mat[i][j];
				backcol[i] = col[i];
			}
		}

		// solve for the matrix elements
		if(!NumericSolver::solveNxNLinearSysDestr(11, mat, col)) 
		{
			// error solving for projective matrix (rows 1,2,4)
		}

		// get a little more precision
		if(incrPrecision)
		{
			for (int k=0; k<3; k++)
			{
				PreciseReal nvec[11];
				for(i=0; i<11; i++)
				{
					int j;
					nvec[i] = backmat[i][0] * col[0];
					mat[i][0] = backmat[i][0];
					for(j=1; j<11; j++) 
					{
						nvec[i] += backmat[i][j] * col[j];
						mat[i][j] = backmat[i][j];
					}
					nvec[i] -= backcol[i];
				}
				if(!NumericSolver::solveNxNLinearSysDestr(11, mat, nvec)) 
				{
					// error solving for increased precision rows 1,2,4
				}
				for(i=0; i<11; i++)
					col[i] -= nvec[i];
			}
		}

		PreciseReal row4[4];
		ind = 8;
		for(i=0; i<4; i++)
		{
			if (i == nzind)
				row4[i] = 1.0;
			else
				row4[i] = col[ind++];
		}


		// now solve for the 3rd row which affects depth precision
		PreciseReal zrow[4];

		// we want the affine skew such that isoplanes of constant depth are parallel to
		// the world plane of interest
		// NOTE: recall we perturbed the last fpoint off the plane, so we'll again modify
		// this one since we want 3 points on the plane = far plane, and 1 on the near plane
		int nearind = 3;
		for(i=0; i<3; i++)
		{
			mat[i][0] = fpoint[i].x;
			mat[i][1] = fpoint[i].y;
			mat[i][2] = fpoint[i].z;
			mat[i][3] = 1.0;
			zrow[i] = (row4[0] * fpoint[i].x +
				row4[1] * fpoint[i].y +
				row4[2] * fpoint[i].z +
				row4[3]) * 0.99 ;
		}
		mat[3][0] = fpoint[nearind].x;
		mat[3][1] = fpoint[nearind].y;
		mat[3][2] = fpoint[nearind].z;
		mat[3][3] = 1.0;
		zrow[3] =	 -row4[0] * fpoint[nearind].x -
			row4[1] * fpoint[nearind].y -
			row4[2] * fpoint[nearind].z -
			row4[3] ;

		// solve for the z row of the matrix
		if(!NumericSolver::solveNxNLinearSysDestr(4, mat, zrow)) 
		{
			// error solving for projective matrix (row 3)
		}

		// set projective texture matrix
		ret = Matrix4(  col[0],  col[1],  col[2],  col[3],
			col[4],  col[5],  col[6],  col[7], 
			zrow[0], zrow[1], zrow[2], zrow[3],
			row4[0], row4[1], row4[2], row4[3] );


		// check for clip 
		Vector4 testCoord = ret * fpoint[0];
		if(testCoord.w < 0.0) 
			ret = ret *  (-1.0);

		// free memory
		for (i=0; i<11; i++)
		{
			if (mat[i])
				delete [] mat[i];
			if (incrPrecision)
				delete [] backmat[i];
		}
		delete [] mat;
		if(incrPrecision)
			delete [] backmat;

		return ret;

	}

	// --------------------------------------------------------------------

	/// Construct object to consider a specified plane of interest
	PlaneOptimalShadowCameraSetup::PlaneOptimalShadowCameraSetup(MovablePlane* plane)
	{
		m_plane = plane;
	}

	/// Destructor
	PlaneOptimalShadowCameraSetup::~PlaneOptimalShadowCameraSetup() {}
	
	/// Implements the plane optimal shadow camera setup algorithm
	void PlaneOptimalShadowCameraSetup::getShadowCamera (const SceneManager *sm, const Camera *cam, 
														 const Viewport *vp, const Light *light, Camera *texCam) const
	{
		// get the plane transformed by the parent node(s)
		// Also, make sure the normal is normalized and the length scale of d is as well
		const Plane& refWorldPlane = m_plane->_getDerivedPlane();
		Plane worldPlane = refWorldPlane;
		Vector3 planeNormal  = worldPlane.normal;
		Real normFactor = planeNormal.normalise();
		normFactor = (normFactor != 0.0) ? 1.0 / normFactor : 0.0;
		worldPlane.normal *= normFactor;
		worldPlane.d *= normFactor;

		// get camera's projection matrix
		Matrix4 camProjection = cam->getProjectionMatrix() * cam->getViewMatrix();

		// get the world points to constrain
		std::vector<Vector4> vhull;
		std::vector<Vector4> extra;
		cam->forwardIntersect(worldPlane, &vhull);
		if (vhull.size() < 4)
			return;
		while(vhull.size() > 4)
		{
			extra.push_back(vhull[vhull.size()-1]);
			vhull.pop_back();
		}
		// make sure the last point is a finite point (not point at infinity)
		int finiteIndex = -1;
		int loopIndex;
		for (loopIndex=0; loopIndex<4; loopIndex++)
		{
			if (vhull[loopIndex].w != 0.0)
				finiteIndex = loopIndex;
		}
		if (finiteIndex == -1)
		{
			// need to look through extra list for a finite point
			for (loopIndex=0; loopIndex<extra.size(); loopIndex++)
			{
				if (extra[loopIndex].w != 0.0)
					finiteIndex = loopIndex;
			}
			if (finiteIndex == -1)
			{
				// there are no finite points, which means camera doesn't see plane of interest.
				// so we don't care what the shadow map matrix is
				// We'll map points off the shadow map so they aren't even stored
				Matrix4 crazyMat(0.0, 0.0, 0.0, 5.0,
								 0.0, 0.0, 0.0, 5.0,
								 0.0, 0.0, 0.0, 5.0,
								 0.0, 0.0, 0.0, 1.0 );
				texCam->setCustomViewMatrix(true, Matrix4::IDENTITY);
				texCam->setCustomProjectionMatrix(true, crazyMat);	
				return;
			}
			// copy the finite point to the last element of vhull
			vhull.pop_back();
			vhull.push_back(extra[finiteIndex]);
		}
		else
		{
			if (finiteIndex != 3)
			{
				extra.clear();
				for(loopIndex=0; loopIndex<4; loopIndex++)
				{
					if (loopIndex != finiteIndex)
						extra.push_back(vhull[loopIndex]);
				}
				extra.push_back(vhull[finiteIndex]);
				vhull.clear();
				vhull = extra;
			}
		}

		// get the post-projective coordinate constraints
		std::vector<Vector2> constraint;
		for (int i=0; i<4; i++)
		{
			Vector4 postProjPt = camProjection * vhull[i];
			postProjPt *= 1.0 / postProjPt.w;
			constraint.push_back(Vector2(postProjPt.x, postProjPt.y));
		}

		// perturb one point so we don't have coplanarity
		Vector3 tempPos = light->getDerivedPosition(); 
		Vector4 pinhole = Vector4(tempPos.x, tempPos.y, tempPos.z, 1.0);
		Vector4 oldPt = vhull[vhull.size()-1];
		Vector4 displacement = oldPt - pinhole;
		Vector3 displace3    = Vector3(displacement.x, displacement.y, displacement.z);
		Real dotProd = fabs(displace3.dotProduct(planeNormal));
		vhull.pop_back();
		static const Real NEAR_FACTOR = 0.05;
		Vector4 newPt = pinhole + (displacement * (cam->getNearClipDistance() * NEAR_FACTOR / dotProd));
		vhull.push_back(newPt);

		// solve for the matrix that stabilizes the plane
		Matrix4 customMatrix = computeConstrainedProjection(pinhole, vhull, constraint);

		// factor into view and projection pieces
		Matrix4    translation(1.0, 0.0, 0.0,  tempPos.x,
							   0.0, 1.0, 0.0,  tempPos.y,
							   0.0, 0.0, 1.0,  tempPos.z,
							   0.0, 0.0, 0.0,  1.0);
		Matrix4 invTranslation(1.0, 0.0, 0.0, -tempPos.x,
							   0.0, 1.0, 0.0, -tempPos.y,
							   0.0, 0.0, 1.0, -tempPos.z,
							   0.0, 0.0, 0.0,  1.0);
		Matrix4 tempMatrix = customMatrix * translation;
		Vector3 zRow(-tempMatrix[3][0], -tempMatrix[3][1], -tempMatrix[3][2]);
		zRow.normalise();
		Vector3 up;
		if (zRow.y == 1.0)
			up = Vector3(1,0,0);
		else
			up = Vector3(0,1,0);
		Vector3 xDir = up.crossProduct(zRow);
		xDir.normalise();
		up = zRow.crossProduct(xDir);
		Matrix4 rotation(xDir.x, up.x, zRow.x, 0.0,
						 xDir.y, up.y, zRow.y, 0.0,
						 xDir.z, up.z, zRow.z, 0.0,
						    0.0,  0.0,    0.0, 1.0 );
		Matrix4 customProj = tempMatrix * rotation;
		Matrix4 customView = rotation.transpose() * invTranslation;
		// note: now customProj * (0,0,0,1)^t = (0, 0, k, 0)^t for k some constant
		// note: also customProj's 4th row is (0, 0, c, 0) for some negative c.


		// set the shadow map camera
		texCam->setCustomViewMatrix(true, customView);
		texCam->setCustomProjectionMatrix(true, customProj);
	}

}
