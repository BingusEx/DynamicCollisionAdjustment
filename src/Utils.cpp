#include "Utils.h"

namespace Utils
{

	void FillCloningProcess(RE::NiCloningProcess& a_cloningProcess, const RE::NiPoint3& a_scale)
	{
		auto cloneMap = reinterpret_cast<uintptr_t>(&a_cloningProcess.cloneMap);
		auto value1 = reinterpret_cast<void**>(cloneMap + 0x18);
		*value1 = g_unkCloneValue1;

		auto processMap = reinterpret_cast<uintptr_t>(&a_cloningProcess.processMap);
		auto value2 = reinterpret_cast<void**>(processMap + 0x18);
		*value2 = g_unkCloneValue2;

		a_cloningProcess.copyType = 1;
		a_cloningProcess.appendChar = '$';

		a_cloningProcess.unk68 = a_scale;
	}

	void ToggleCharacterBumper(RE::Actor* a_actor, bool a_bEnable)
	{
		if (auto characterController = a_actor->GetCharController()) {
			float relevantWaterHeight = TESObjectREFR_GetRelevantWaterHeight(a_actor);
			bhkCharacterController_ToggleCharacterBumper(characterController, a_bEnable);
			if (auto loadedData = a_actor->loadedData) {
				if (relevantWaterHeight > loadedData->relevantWaterHeight) {
					loadedData->relevantWaterHeight = relevantWaterHeight;
				}
			}
		}
	}

	//TODO: this thing is bad and needs fixing
	RE::hkVector4 GetBoneQuad(const RE::Actor* a_actor, const char* a_boneStr, const bool a_invert, const bool a_worldtranslate)
	{
		//Returns Quad Of 0f
		if (!a_actor || !a_boneStr)
			return {};

		//Get TP Bone
		const RE::NiAVObject* Bone = FindBoneNode(a_actor, a_boneStr, false);

		if (!Bone) {
			//Try FP
			Bone = FindBoneNode(a_actor, a_boneStr, true);
			if (!Bone)
				return {};
		}

		if (a_worldtranslate) {
			const RE::NiPoint3 Position = Bone->world.translate;
			logger::trace("Bone WorldPos X:{} Y:{} Z:{}", Position.x, Position.y, Position.z);
			RE::NiPoint3 Distance = Position - a_actor->GetPosition();
			logger::trace("Bone WorldPos Relative to Actor X:{} Y:{} Z:{}", Distance.x, Distance.y, Distance.z);
			if (a_invert)
				Distance = -Distance;
			return NiPointToHkVector(Distance, true);
		}
		return NiPointToHkVector(Bone->local.translate, true);
	}

	RE::NiAVObject* FindBoneNode(const RE::Actor* a_actorptr, const std::string_view a_nodeName, const bool a_isFirstPerson)
	{
		if (!a_actorptr->Is3DLoaded())
			return nullptr;

		const auto model = a_actorptr->Get3D(a_isFirstPerson);

		if (!model)
			return nullptr;

		if (const auto node_lookup = model->GetObjectByName(a_nodeName))
			return node_lookup;

		// Game lookup failed we try and find it manually
		std::deque<RE::NiAVObject*> queue;
		queue.push_back(model);
		int RecursionCheck = 512;

		while (!queue.empty()) {
			const auto currentnode = queue.front();
			queue.pop_front();
			int RecursionCheckFor = 512;
			if (RecursionCheck-- <= 0) {
				//MessageBoxA(NULL, "RECURSION BUG IN FindBoneNode()", "DynamicColisionAdjustment", 0);
				queue.clear();
				return nullptr;
			}
			try {
				if (currentnode) {
					if (const auto ninode = currentnode->AsNode()) {
						for (const auto& child : ninode->GetChildren()) {
							if (RecursionCheckFor-- <= 0) {
								//MessageBoxA(NULL, "RECURSION BUG IN FindBoneNode() For Loop", "DynamicColisionAdjustment", 0);
								queue.clear();
								return nullptr;
							}
							// Bredth first search
							queue.push_back(child.get());
							// Depth first search
							//queue.push_front(child.get());
						}
					}
					// Do smth
					if (currentnode->name.c_str() == a_nodeName) {
						return currentnode;
					}
				}
			} catch (const std::overflow_error& e) {
				SKSE::log::warn("Find Bone Overflow: {}", e.what());
				//MessageBoxA(NULL, "Overflow Exception in FindBoneNode()", "DynamicColisionAdjustment", 0);
				return nullptr;
			}  // this executes if f() throws std::overflow_error (same type rule)
			catch (const std::runtime_error& e) {
				SKSE::log::warn("Find Bone Underflow: {}", e.what());
				//MessageBoxA(NULL, "Underflow Exception in FindBoneNode()", "DynamicColisionAdjustment", 0);
				return nullptr;
			}  // this executes if f() throws std::underflow_error (base class rule)
			catch (const std::exception& e) {
				SKSE::log::warn("Find Bone Exception: {}", e.what());
				//MessageBoxA(NULL, "STD Exception in FindBoneNode()", "DynamicColisionAdjustment", 0);
				return nullptr;
			}  // this executes if f() throws std::logic_error (base class rule)
			catch (...) {
				SKSE::log::warn("Find Bone Exception Other");
				//MessageBoxA(NULL, "Other Exception in FindBoneNode()", "DynamicColisionAdjustment", 0);
				return nullptr;
			}
		}

		return nullptr;
	}



}
