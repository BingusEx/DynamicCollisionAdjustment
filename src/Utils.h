#pragma once

#include "Offsets.h"

namespace Utils
{
	[[nodiscard]] inline RE::NiPoint3 HkVectorToNiPoint(const RE::hkVector4& vec, bool bConvertScale = false) 
	{ 
		RE::NiPoint3 ret = { vec.quad.m128_f32[0], vec.quad.m128_f32[1], vec.quad.m128_f32[2] };
		if (bConvertScale) {
			ret *= *g_worldScaleInverse;
		}
		return ret;
	}

	inline RE::NiPoint2 Vec2Rotate(const RE::NiPoint2& vec, float angle)
	{
		RE::NiPoint2 ret;
		ret.x = vec.x * cos(angle) - vec.y * sin(angle);
		ret.y = vec.x * sin(angle) + vec.y * cos(angle);
		return ret;
	}

	inline float Vec2Length(const RE::NiPoint2& vec)
	{
		return std::sqrtf(vec.x * vec.x + vec.y * vec.y);
	}

	inline RE::NiPoint2 Vec2Normalize(RE::NiPoint2& vec)
	{
		RE::NiPoint2 ret(0.f, 0.f);
		float vecLength = Vec2Length(vec);
		if (vecLength == 0) {
			return ret;
		}
		float invlen = 1.0f / vecLength;
		ret.x = vec.x * invlen;
		ret.y = vec.y * invlen;
		return ret;
	}

	[[nodiscard]] inline RE::NiPoint3 RotateAngleAxis(const RE::NiPoint3& vec, const float angle, const RE::NiPoint3& axis)
	{
		float S = sin(angle);
		float C = cos(angle);

		const float XX = axis.x * axis.x;
		const float YY = axis.y * axis.y;
		const float ZZ = axis.z * axis.z;

		const float XY = axis.x * axis.y;
		const float YZ = axis.y * axis.z;
		const float ZX = axis.z * axis.x;

		const float XS = axis.x * S;
		const float YS = axis.y * S;
		const float ZS = axis.z * S;

		const float OMC = 1.f - C;

		return RE::NiPoint3(
			(OMC * XX + C) * vec.x + (OMC * XY - ZS) * vec.y + (OMC * ZX + YS) * vec.z,
			(OMC * XY + ZS) * vec.x + (OMC * YY + C) * vec.y + (OMC * YZ - XS) * vec.z,
			(OMC * ZX - YS) * vec.x + (OMC * YZ + XS) * vec.y + (OMC * ZZ + C) * vec.z);
	}

	inline float DotProduct(RE::NiPoint2& a, RE::NiPoint2& b)
	{
		return a.x * b.x + a.y * b.y;
	}

	inline float CrossProduct(RE::NiPoint2& a, RE::NiPoint2& b)
	{
		return a.x * b.y - a.y * b.x;
	}

	inline float GetAngle(RE::NiPoint2& a, RE::NiPoint2& b)
	{
		return atan2(CrossProduct(a, b), DotProduct(a, b));
	}

	inline RE::NiMatrix3 MakeRotationMatrixFromX(const RE::NiPoint3& a_xAxis)
	{
		// try to use up if possible
		RE::NiPoint3 upVector = (fabs(a_xAxis.z) < (1.f - 1.e-4f)) ? RE::NiPoint3{ 0.f, 0.f, 1.f } : RE::NiPoint3{ 1.f, 0.f, 0.f };

		RE::NiPoint3 newY = upVector.UnitCross(a_xAxis);
		RE::NiPoint3 newZ = a_xAxis.Cross(newY);

		return RE::NiMatrix3(a_xAxis, newY, newZ);
	}

	inline RE::NiMatrix3 MakeRotationMatrixFromY(const RE::NiPoint3& a_yAxis)
	{
		// try to use up if possible
		RE::NiPoint3 upVector = (fabs(a_yAxis.z) < (1.f - 1.e-4f)) ? RE::NiPoint3{ 0.f, 0.f, 1.f } : RE::NiPoint3{ 1.f, 0.f, 0.f };

		RE::NiPoint3 newZ = upVector.UnitCross(a_yAxis);
		RE::NiPoint3 newX = a_yAxis.Cross(newZ);

		return RE::NiMatrix3(newX, a_yAxis, newZ);
	}

	inline RE::NiMatrix3 MakeRotationMatrixFromZ(const RE::NiPoint3& a_zAxis)
	{
		// try to use up if possible
		RE::NiPoint3 upVector = (fabs(a_zAxis.z) < (1.f - 1.e-4f)) ? RE::NiPoint3{ 0.f, 0.f, 1.f } : RE::NiPoint3{ 1.f, 0.f, 0.f };

		RE::NiPoint3 newX = upVector.UnitCross(a_zAxis);
		RE::NiPoint3 newY = a_zAxis.Cross(newX);

		return RE::NiMatrix3(newX, newY, a_zAxis);
	}

	inline RE::NiMatrix3 MakeRotationMatrixFromXY(const RE::NiPoint3& a_xAxis, const RE::NiPoint3& a_yAxis)
	{
		RE::NiPoint3 newZ = a_xAxis.UnitCross(a_yAxis);
		RE::NiPoint3 newY = newZ.Cross(a_xAxis);

		return RE::NiMatrix3(a_xAxis, newY, newZ);
	}

	inline RE::NiMatrix3 MakeRotationMatrixFromXZ(const RE::NiPoint3& a_xAxis, const RE::NiPoint3& a_zAxis)
	{
		RE::NiPoint3 newY = a_zAxis.UnitCross(a_xAxis);
		RE::NiPoint3 newZ = a_xAxis.Cross(newY);

		return RE::NiMatrix3(a_xAxis, newY, newZ);
	}

	[[nodiscard]] inline RE::NiQuaternion MatrixToQuaternion(const RE::NiMatrix3& m)
	{
		RE::NiQuaternion q;
		NiMatrixToNiQuaternion(q, m);
		return q;
	}

	void FillCloningProcess(RE::NiCloningProcess& a_cloningProcess, const RE::NiPoint3& a_scale);

	template <class T>
	T* Clone(T* a_object, const RE::NiPoint3& a_scale)
	{
		if (!a_object) {
			return nullptr;
		}

		RE::NiCloningProcess cloningProcess{};
		FillCloningProcess(cloningProcess, a_scale);

		auto clone = reinterpret_cast<T*>(NiObject_Clone(a_object, cloningProcess));

		/*CleanupProcessMap(cloningProcess.processMap);
		CleanupCloneMap(cloningProcess.cloneMap);*/

		return clone;
	}

	void ToggleCharacterBumper(RE::Actor* a_actor, bool a_bEnable);

	//TODO All of this should really be an API call into th gts dll..

	[[nodiscard]] RE::hkVector4 GetBoneQuad(const RE::Actor* a_actor, const char* a_boneStr, bool a_invert, bool a_worldtranslate);

	[[nodiscard]] RE::NiAVObject* FindBoneNode(const RE::Actor* a_actorptr, std::string_view a_nodeName, bool a_isFirstPerson);

	[[nodiscard]] inline float soft_power(const float x, const float k, const float n, const float s, const float o, const float a)
	{
		return pow(1.0f + pow(k * (x), n * s), 1.0f / s) / pow(1.0f + pow(k * o, n * s), 1.0f / s) + a;
	}

	[[nodiscard]] inline float soft_core(const float x, const float k, const float n, const float s, const float o, const float a)
	{
		return 1.0f / soft_power(x, k, n, s, o, 0.0) + a;
	}

	[[nodiscard]] inline RE::NiPoint3 GetNiPoint3(RE::hkVector4 a_hkVector4)
	{
		float quad[4];
		_mm_store_ps(quad, a_hkVector4.quad);
		return RE::NiPoint3{ quad[0], quad[1], quad[2] };
	}

	[[nodiscard]] inline float Remap(const float a_oldValue, const float a_oldMin, const float a_oldMax, const float a_newMin, const float a_newMax)
	{
		return (((a_oldValue - a_oldMin) * (a_newMax - a_newMin)) / (a_oldMax - a_oldMin)) + a_newMin;
	}

	[[nodiscard]] inline float GetRefScale(RE::Actor* actor)
	{
		// This function reports same values as GetScale() in the console, so it is a value from SetScale() command
		return static_cast<float>(actor->GetReferenceRuntimeData().refScale) / 100.0F;
	}

	[[nodiscard]] inline float GetModelScale(const RE::Actor* a_actor)
	{
		if (!a_actor)
			return 1.0;

		if (!a_actor->Is3DLoaded()) {
			return 1.0;
		}

		if (const auto model = a_actor->Get3D(false)) {
			return model->local.scale;
		}

		if (const auto first_model = a_actor->Get3D(true)) {
			return first_model->local.scale;
		}

		return 1.0;
	}

	[[nodiscard]] inline float GetNodeScale(const RE::Actor* a_actor, const std::string_view a_boneName)
	{
		if (!a_actor)
			return 1.0f;

		if (const auto Node = FindBoneNode(a_actor, a_boneName, false)) {
			return Node->local.scale;
		}
		if (const auto FPNode = FindBoneNode(a_actor, a_boneName, true)) {
			return FPNode->local.scale;
		}
		return 1.0;
	}

	[[nodiscard]] inline RE::hkVector4 NiPointToHkVector(const RE::NiPoint3& a_point, bool a_convertScale = false)
	{
		RE::hkVector4 ret = { a_point.x, a_point.y, a_point.z, 0 };
		if (a_convertScale) {
			ret = ret * *g_worldScale;
			logger::trace("(NiPointToHKVector) Worldscale: {} Resulting Output: X:{}, Y:{}, Z:{},", *g_worldScale, ret.quad.m128_f32[0], ret.quad.m128_f32[1], ret.quad.m128_f32[2]);
		}
		return ret;
	}

	[[nodiscard]] inline float GetScale(const RE::Actor* a_actor)
	{
		if (!a_actor)
			return 1.f;

		float TargetScale = 1.f;
		TargetScale *= GetModelScale(a_actor);                    //Model scale, Scaling done by game
		TargetScale *= GetNodeScale(a_actor, "NPC");              // NPC bone, Racemenu uses this.
		TargetScale *= GetNodeScale(a_actor, "NPC Root [Root]");  //Child bone of "NPC" some other mods scale this bone instead

		if (TargetScale < 0.15f)
			TargetScale = 0.15f;
		//Stop scaling past 20 as a safety measure. If an npc gets stuck due to colission it will murder the framerate.
		if (TargetScale > 20)
			TargetScale = 20;
		return TargetScale;
	}

	[[nodiscard]] inline bool FloatsEqual(const float a, const float b)
	{
		constexpr float epsilon = std::numeric_limits<float>::epsilon();
		return std::abs(a - b) < epsilon;
	}

	[[nodiscard]] inline bool FloatsEqualDelta(const float a, const float b, const float delta)
	{
		//constexpr float epsilon = std::numeric_limits<float>::epsilon();
		return std::abs(a - b) < delta;
	}

	[[nodiscard]] inline RE::hkVector4 GetHeadQuad(const RE::Actor* ActorPtr, const float CorrectionScale)
	{
		RE::hkVector4 Correction;
		Correction.quad.m128_f32[2] = (CorrectionScale * 0.32f) - 1.f;
		return GetBoneQuad(ActorPtr, "NPC Head [Head]", false,true) + Correction;
	}

	[[nodiscard]] inline RE::hkVector4 GetRootQuad(const RE::Actor* ActorPtr)
	{
		return GetBoneQuad(ActorPtr, "NPC Root [Root]", true, true);
	}

	// Get The needed offset for a spheres ground point
	[[nodiscard]] inline float SphereOffset(const float Original, const float Scaled)
	{
		return -Original + Scaled;
	}
}
