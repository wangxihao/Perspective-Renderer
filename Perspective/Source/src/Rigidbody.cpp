#include "stdafx.h"
#include "Rigidbody.h"

#include <Physics.h>

#include "MyMath.h"

void Rigidbody::SolverWorkArea::Clear()
{
	delta_linear_velocity = {};
	delta_angular_momentum = {};
	delta_angular_velocity = {};
}

void Rigidbody::UpdateOrientation(float dt)
{
	// Integrate orientation

#if 1
	orientation = orientation + glm::quat(0.0f, angularVelocity * dt * 0.5f) * orientation;
#else
	const glm::vec3 axis = glm::normalize(angularVelocity);
	if (glm::all(glm::isnan(axis)))
		return;
	const float angle = glm::length(angularVelocity) * dt;
	orientation = glm::rotate(orientation, angle, axis);
#endif

	orientation = glm::normalize(orientation);
	inverseOrientation = glm::inverse(orientation);
}

void Rigidbody::AddCollider(Collider& newCollider)
{
	collider = newCollider;
	collider.rb = this;
	collider.aabb.collider = &collider;

	mass = collider.mass;
	position = collider.aabb.center;

	inverseMass = 1.f / mass;

	halfExtent = newCollider.aabb.halfExtents;

	glm::mat3 localInertiaTensor = collider.localInertiaTensor;

	localInverseInertiaTensor = glm::inverse(localInertiaTensor);
}

const glm::vec3 Rigidbody::LocalToGlobal(const glm::vec3& p) const
{
	return orientation * p + position;
}

const glm::vec3 Rigidbody::GlobalToLocal(const glm::vec3& p) const
{
	return inverseOrientation * p + position;
}

const glm::vec3 Rigidbody::LocalToGlobalVec(const glm::vec3& v) const
{
	return orientation * v;
}

const glm::vec3 Rigidbody::GlobalToLocalVec(const glm::vec3& v) const
{
	return inverseOrientation * v;
}

void Rigidbody::ApplyForce(const glm::vec3& f)
{
	if(fixed)
		return;
	forceAccumulator += f;
}

void Rigidbody::CorrectVelocity()
{
	if (fixed)
		return;

	linearVelocity += solver_work_area.delta_linear_velocity;
	angularMomentum += solver_work_area.delta_angular_momentum;
	angularVelocity += solver_work_area.delta_angular_velocity;
}

void Rigidbody::ApplyForce(const glm::vec3& f, const glm::vec3& at)
{
	if (fixed)
		return;
	forceAccumulator += f;
	torqueAccumulator += glm::cross(at - position, f);
}

void Rigidbody::ApplyImpulse(const glm::vec3& impulse, const glm::vec3 relativePosition)
{
	if (fixed)
		return;
	solver_work_area.delta_linear_velocity += inverseMass * impulse;
	const glm::vec3 L = glm::cross(relativePosition, impulse);
	solver_work_area.delta_angular_momentum += L;
	solver_work_area.delta_angular_velocity += globalInverseInertiaTensor * L;
}

void Rigidbody::UpdateVelocity(float dt)
{
	if (fixed)
		return;
	constexpr glm::vec3 gravity(0, -9.8, 0);

	glm::vec3 acceleration;
	if(useGravity)
		acceleration = gravity + inverseMass * forceAccumulator;
	else
		acceleration = inverseMass * forceAccumulator;

	switch(Physics::mode)
	{
		default:
		case RK4:
		// Treat force and torque as constant through the frame
		// Given that assumption, F = MA are all constant as well
		const glm::vec3 intergratedAcceleration = MyMath::runge_kutta4(
		linearVelocity, 0.0f, dt, acceleration, 
		[](glm::vec3 x, float t, float dt, glm::vec3 y)->glm::vec3
		{

			// v = v + a * dt;
			return x + y * dt;
		});

		linearVelocity += intergratedAcceleration * dt;
		break;
		case Semi_Euler:
		linearVelocity += acceleration * dt;
		break;
		case RK2:
			const glm::vec3 intergratedAcceleration2 = MyMath::runge_kutta2(
				linearVelocity, 0.0f, dt, acceleration,
				[](glm::vec3 x, float t, float dt, glm::vec3 y)->glm::vec3
				{

					// v = v + a * dt;
					return x + y * dt;
				});

			linearVelocity += intergratedAcceleration2 * dt;
			break;
		break;
	}
	lastLinearVelocity = linearVelocity;
	angularMomentum += dt * torqueAccumulator;
	angularVelocity = globalInverseInertiaTensor * angularMomentum;
}

void Rigidbody::UpdatePosition(float dt)
{
	if (fixed)
		return;

	switch (Physics::mode)
	{
	default:
	case RK4:
		const glm::vec3 integratedVelocity = MyMath::runge_kutta4(
			position, 0.0f, dt, linearVelocity,
			[](glm::vec3 x, float t, float dt, glm::vec3 y)->glm::vec3
			{
				// q = q + v * dt;
				return x + y * dt;
			});
		position += integratedVelocity * dt;
		break;
	case Semi_Euler:
		position += linearVelocity * dt;
		break;
	case RK2:
		const glm::vec3 integratedVelocity2 = MyMath::runge_kutta2(
			position, 0.0f, dt, linearVelocity,
			[](glm::vec3 x, float t, float dt, glm::vec3 y)->glm::vec3
			{
				// q = q + v * dt;
				return x + y * dt;
			});

		position += integratedVelocity2 * dt;
		break;
	}

	// Todo: hack this away later
	collider.aabb.center = position;

}

void Rigidbody::Reset()
{
	linearVelocity = glm::vec3(0);
	angularVelocity = glm::vec3(0);
	angularMomentum = glm::vec3(0);
	forceAccumulator = glm::vec3(0);
	torqueAccumulator = glm::vec3(0);

	position = glm::vec3(0);
	orientation = glm::vec3(0);
}
