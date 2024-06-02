#include "AdjustmentHandler.h"
#include "Offsets.h"
#include "Settings.h"
#include "Utils.h"

#include <algorithm>

using namespace RE;
using namespace SKSE;


//-----------------------
//	Get Controller Data
//-----------------------

//Get Controller Data From an Actor Handle
std::shared_ptr<AdjustmentHandler::ControllerData> AdjustmentHandler::GetControllerData(ActorHandle Handle) {
	ReadLocker locker(ControllersLock);

	if (auto Actor = Handle.get()) {
		if (auto Controller = Actor->GetCharController()) {
			if (auto Search = ControllerMap.find(Controller); Search != ControllerMap.end()) {
				return Search->second;
			}
		}
	}
	return nullptr;
}

//Get Controller Data From a Char Controller
std::shared_ptr<AdjustmentHandler::ControllerData> AdjustmentHandler::GetControllerData(bhkCharacterController* CharController) {
	ReadLocker locker(ControllersLock);

	if (auto Search = ControllerMap.find(CharController); Search != ControllerMap.end()) {
		return Search->second;
	}

	return nullptr;
}

//-----------------------
//	Get Shapes
//-----------------------

bool AdjustmentHandler::GetShapes(bhkCharacterController* CharController, const hkpConvexVerticesShape*& OutConvexShape, std::vector<hkpCapsuleShape*>& OutColisionShape) {
	const auto readShape = [&](const auto& self, const RE::hkpShape* a_shape) -> bool {
		if (a_shape) {
			switch (a_shape->type) {
			case RE::hkpShapeType::kCapsule:
				if (auto capsuleShape = const_cast<RE::hkpCapsuleShape*>(skyrim_cast<const RE::hkpCapsuleShape*>(a_shape))) {
					OutColisionShape.emplace_back(capsuleShape);
				}
				break;
			case RE::hkpShapeType::kConvexVertices:
				if (auto convexVerticesShape = const_cast<RE::hkpConvexVerticesShape*>(skyrim_cast<const RE::hkpConvexVerticesShape*>(a_shape))) {
					assert(a_outCollisionConvexVerticesShape == nullptr);
					OutConvexShape = convexVerticesShape;
					return true;
				}
				break;
			case RE::hkpShapeType::kList:
				if (auto listShape = skyrim_cast<RE::hkpListShape*>(const_cast<RE::hkpShape*>(a_shape))) {
					for (auto& childInfo : listShape->childInfo) {
						if (auto child = childInfo.shape) {
							self(self, child);
						}
					}
				}
				break;
			case RE::hkpShapeType::kMOPP:
				if (auto moppShape = const_cast<RE::hkpMoppBvTreeShape*>(skyrim_cast<const RE::hkpMoppBvTreeShape*>(a_shape))) {
					RE::hkpShapeBuffer buf{};
					RE::hkpShapeKey shapeKey = moppShape->child.GetFirstKey();
					for (int i = 0; i < moppShape->child.GetNumChildShapes(); ++i) {
						if (auto child = moppShape->child.GetChildShape(shapeKey, buf)) {
							self(self, child);
						}
						shapeKey = moppShape->child.GetNextKey(shapeKey);
					}
				}
				break;
			}
		}

		return false;
	};

	if (auto proxyController = skyrim_cast<RE::bhkCharProxyController*>(CharController)) {
		if (auto charProxy = static_cast<RE::hkpCharacterProxy*>(proxyController->proxy.referencedObject.get())) {
			readShape(readShape, charProxy->shapePhantom->collidable.shape);

			return OutConvexShape || !OutColisionShape.empty();
		}
	} else if (auto rigidBodyController = skyrim_cast<RE::bhkCharRigidBodyController*>(CharController)) {
		if (auto rigidBody = static_cast<RE::hkpCharacterRigidBody*>(rigidBodyController->rigidBody.referencedObject.get())) {
			readShape(readShape, rigidBody->character->collidable.shape);

			return OutConvexShape || !OutColisionShape.empty();
		}
	}

	return false;
}

bool AdjustmentHandler::GetCapsules(bhkCharacterController* CharController, std::vector<hkpCapsuleShape*>& OutCollisionCapsules) {

	const auto ReadShape = [&](const auto& Self, const hkpShape* Shape) {

		if (Shape == nullptr) return;

		switch (Shape->type) {
			case hkpShapeType::kCapsule:{
				if (hkpCapsuleShape* CapsuleShape = const_cast<hkpCapsuleShape*>(skyrim_cast< const hkpCapsuleShape*>(Shape))) {
					OutCollisionCapsules.emplace_back(CapsuleShape);
				}
				break;
			}

			case hkpShapeType::kList:{
				if (hkpListShape* ListShape = skyrim_cast<hkpListShape*>(const_cast<hkpShape*>(Shape))) {
					for (hkpListShape::ChildInfo& ChildInfo : ListShape->childInfo) {
						if (auto Child = ChildInfo.shape) {
							Self(Self, Child);
						}
					}
				}
				break;
			}

			case hkpShapeType::kMOPP: {
				if (hkpMoppBvTreeShape* MOPPShape = const_cast<hkpMoppBvTreeShape*>(skyrim_cast< const hkpMoppBvTreeShape*>(Shape))) {
					hkpShapeBuffer ShapeBuffer{};
					hkpShapeKey ShapeKey = MOPPShape->child.GetFirstKey();
					for (int i = 0; i < MOPPShape->child.GetNumChildShapes(); ++i) {
						if (auto ChildShape = MOPPShape->child.GetChildShape(ShapeKey, ShapeBuffer)) {
							Self(Self, ChildShape);
						}
						ShapeKey = MOPPShape->child.GetNextKey(ShapeKey);
					}
				}
				break;
			}

			default:{
				break;
			}
		}
	};

	if (bhkCharProxyController* ProxyController = skyrim_cast<bhkCharProxyController*>(CharController)) {
		if (hkpCharacterProxy* CharacterProxy = static_cast<hkpCharacterProxy*>(ProxyController->proxy.referencedObject.get())) {
			ReadShape(ReadShape, CharacterProxy->shapePhantom->collidable.shape);
			return !OutCollisionCapsules.empty();
		}
	}

	else if (bhkCharRigidBodyController*  RigidBodyController = skyrim_cast<bhkCharRigidBodyController*>(CharController)) {
		if (hkpCharacterRigidBody* RigidBody = static_cast<hkpCharacterRigidBody*>(RigidBodyController->rigidBody.referencedObject.get())) {
			ReadShape(ReadShape, RigidBody->character->collidable.shape);
			return !OutCollisionCapsules.empty();
		}
	}

	return false;
}

bool AdjustmentHandler::GetConvexShape(bhkCharacterController* CharController, hkpCharacterProxy*& OutProxy, hkpCharacterRigidBody*& OutRigidBody, hkpListShape*& OutListshape, hkpConvexVerticesShape*& OutConvexShape) {
	if (bhkCharProxyController* proxyController = skyrim_cast<RE::bhkCharProxyController*>(CharController)) {
		OutProxy = static_cast<RE::hkpCharacterProxy*>(proxyController->proxy.referencedObject.get());
		if (OutProxy) {
			OutListshape = skyrim_cast<RE::hkpListShape*>(const_cast<RE::hkpShape*>(OutProxy->shapePhantom->collidable.shape));
			OutConvexShape = skyrim_cast<RE::hkpConvexVerticesShape*>(const_cast<RE::hkpShape*>(OutListshape ? OutListshape->childInfo[0].shape : OutProxy->shapePhantom->collidable.shape));
			return OutListshape && OutConvexShape;
		}
	}
	else if (bhkCharRigidBodyController* rigidBodyController = skyrim_cast<RE::bhkCharRigidBodyController*>(CharController)) {
		OutRigidBody = static_cast<RE::hkpCharacterRigidBody*>(rigidBodyController->rigidBody.referencedObject.get());
		if (OutRigidBody) {
			OutListshape = skyrim_cast<RE::hkpListShape*>(const_cast<RE::hkpShape*>(OutRigidBody->character->collidable.shape));
			OutConvexShape = skyrim_cast<RE::hkpConvexVerticesShape*>(const_cast<RE::hkpShape*>(OutListshape ? OutListshape->childInfo[0].shape : OutRigidBody->character->collidable.shape));
			return OutListshape && OutConvexShape;
		}
	}

	return false;
}

//-----------------------
//	Init
//-----------------------

void AdjustmentHandler::ControllerData::Initialize() {

	NiPointer<Actor> NiActor = ActorHandle.get();
	if (!NiActor) return;

	hkpCharacterProxy* CharProxy = nullptr;
	hkpCharacterRigidBody* CharRigidBody = nullptr;
	hkpListShape* ListShape = nullptr;
	hkpConvexVerticesShape* ConvexShape = nullptr;

	TESObjectCELL* Cell = NiActor->GetParentCell();
	if (!Cell) return;
	

	NiPointer<bhkWorld> World = NiPointer(Cell->GetbhkWorld());
	if (!World) return;

	Sneaking = NiActor->IsSneaking();
	VActorScale = NiActor->GetScale();
	ActorScale = Utils::GetScale(NiActor.get());

	BSWriteLockGuard lock(World->worldLock);

	// save convex shape values
	if (GetConvexShape(CharController, CharProxy, CharRigidBody, ListShape, ConvexShape)) {
		hkArray<hkVector4> Verteces{};
		hkpConvexVerticesShape_getOriginalVertices(ConvexShape, Verteces);
		for (auto& Vertex : Verteces) {
			OriginalVerts.emplace_back(Vertex);
		}

		NiPoint3 Vertex = Utils::HkVectorToNiPoint(Verteces[0]);
		Vertex.z = 0.f;
		OriginalConvexRadius = Vertex.Length();
	}

	bool* BumperEnabled = reinterpret_cast<bool*>(&CharController->unk320);
	*BumperEnabled = true;
	Utils::ToggleCharacterBumper(NiActor.get(), false);

	//logger::info("Actor OK [0x{:X}] {}", NiActor->formID, NiActor->GetName());
	IsInitialized = true;
}

//-----------------------
//	CapsuleShape
//-----------------------

//In Ctor, For every actor
void AdjustmentHandler::ControllerData::SetupProxyCapsule() {
	NiPointer<Actor> NiActor = ActorHandle.get();
	if (!NiActor) return;

	TESObjectCELL* Cell = NiActor->GetParentCell();
	if (!Cell) return;

	NiPointer<bhkWorld> World = NiPointer(Cell->GetbhkWorld());
	if (!World) return;

	if (!CharController) return;

	TESRace* Race = NiActor->GetRace();
	if (!Race) return;

	IsCreature = !NiActor->HasKeyword(Settings::kywd_NPC) && !Race->HasKeyword(Settings::kywd_NPC);


	int8_t shapeIdx = 1;
	if (!CharController->shapes[shapeIdx]) shapeIdx = 0;
	if (!CharController->shapes[shapeIdx]) return;

	//logger::info("Setup Controller");

	BSReadLockGuard WorldLock(World->worldLock);

	//Setup Player
	if(bhkCharProxyController* ProxyController = skyrim_cast<bhkCharProxyController*>(CharController)){

		//Player does not need a clone
		std::vector<hkpCapsuleShape*> Capsules{};
		
		//Store Original Shape
		GetCapsules(ProxyController, Capsules);

		for (hkpCapsuleShape* Capsule : Capsules) {
			OriginalCapsuleRadius.emplace_back(Capsule->radius);
			OriginalCapsuleA.emplace_back(Capsule->vertexA);
			OriginalCapsuleB.emplace_back(Capsule->vertexB);
		}
	}

	//Setup NPC's
	else if (bhkCharRigidBodyController* RigidBodyController = skyrim_cast<bhkCharRigidBodyController*>(CharController)) {
		hkRefPtr RigidBody(static_cast<hkpCharacterRigidBody*>(RigidBodyController->rigidBody.referencedObject.get()));
		//Only Required for creatures, I hope whoever implemented creature havok at bethesda got fired.
		if (RigidBody && IsCreature) {
			NiPointer Wrapper(RigidBody->character->collidable.shape->userData);
			if (Wrapper) {
				//hkpClone = NiPointer(Utils::Clone<hkpShape>((hkpShape*)(Wrapper2.get()), { ActorScale, ActorScale, ActorScale }));
				auto NewClone = NiPointer(Utils::Clone<bhkShape>(Wrapper.get(), { ActorScale, ActorScale, ActorScale }));
				RigidBody->character->SetShape(static_cast<hkpShape*>(NewClone->referencedObject.get()));
		 		CharController->shapes[shapeIdx] -> DecRefCount();
				CharController->shapes[shapeIdx] = NewClone;
			}
		}

		OldActorScale = ActorScale;
		std::vector<hkpCapsuleShape*> Capsules{};
		GetCapsules(RigidBodyController, Capsules);

		for (hkpCapsuleShape* Capsule : Capsules) {
			OriginalCapsuleRadius.emplace_back(Capsule->radius);
			OriginalCapsuleA.emplace_back(Capsule->vertexA);
			OriginalCapsuleB.emplace_back(Capsule->vertexB);
		}
	}
}

//Player & Followers
void AdjustmentHandler::ControllerData::AdjustProxyCapsule() {
	NiPointer<Actor> NiActor = ActorHandle.get();
	if (!NiActor) return;

	TESObjectCELL* Cell = NiActor->GetParentCell();
	if (!Cell) return;

	NiPointer<bhkWorld> World = NiPointer(Cell->GetbhkWorld());
	if (!World) return;

	if (!CharController) return;

	int8_t shapeIdx = 1;
	if (!CharController->shapes[shapeIdx]) shapeIdx = 0;
	if (!CharController->shapes[shapeIdx]) return;
	if (IsCreature) return;

	//The Player as the ProxyController.
	if(bhkCharProxyController* ProxyController = skyrim_cast<bhkCharProxyController*>(CharController)){

		std::vector<hkpCapsuleShape*> Capsules{};
		GetCapsules(ProxyController, Capsules);
		if (Capsules.empty()) return;
		if (Capsules.size() != OriginalCapsuleRadius.size()) return;
		if (Capsules.size() != OriginalCapsuleA.size()) return;
		if (Capsules.size() != OriginalCapsuleB.size()) return;

		//So This Should Not Work but it does. Theoretically We should be setting the PhantomShape And not its bhkCapsule,
		//However This just appears to work so we're doing it this way.
		//Fyi The Bumper does not appear to actually be used. It looks like the convex shape acts as the bumping shape as well.
		//Eh better safe than sorry.
		for (size_t i = 0ull; i < Capsules.size(); i++) {
			//Set Sphere Radius, This In Reality is the Scale.
			//The 0.8f is arbitrairy, i find the default shape gets stupidly large so just offset its scale a bit.
			Capsules[i] -> radius = OriginalCapsuleRadius[i] * ActorScale * 0.8f;

			//Get The Ground Offset for VertexB So It does not go through the floor
			float GroundOffset = Utils::SphereOffset(OriginalCapsuleRadius[i], Capsules[i]->radius);
			//Set Head, GetQuad Needs a fixes 1.45x offset compares to the ConvexShape, Subtract Original Z Value From Vertex
			Capsules[i]-> vertexB.quad.m128_f32[2] = OriginalCapsuleB[i].quad.m128_f32[2] + GroundOffset;

			float TargetZ = Utils::GetHeadQuad(NiActor.get(), 1.45f).quad.m128_f32[2] - OriginalCapsuleA[i].quad.m128_f32[2];
			float BottomZ = Capsules[i]->vertexB.quad.m128_f32[2];	     //The the bottom Vertex We Just Set
			float RealZ = TargetZ < BottomZ ? BottomZ + 0.01f : TargetZ; //If Target is lower than bottom Get Bottom + 0.01 else get Top;

			//Set The Top Vertex Z-Pos
			Capsules[i]->vertexA.quad.m128_f32[2] = RealZ;

			//Move The Shape forward Depending On Scale Index 1 is Y Pos
			Capsules[i] -> vertexA.quad.m128_f32[1] = OriginalCapsuleA[i].quad.m128_f32[1] * ActorScale;
			Capsules[i] -> vertexB.quad.m128_f32[1] = OriginalCapsuleB[i].quad.m128_f32[1] * ActorScale;
		}

	}

	//NPC's Get the RigidBodyController.
	else if (bhkCharRigidBodyController* RigidBodyController = skyrim_cast<bhkCharRigidBodyController*>(CharController)) {
		std::vector<hkpCapsuleShape*> CapsulesNPC{};
		GetCapsules(RigidBodyController, CapsulesNPC);
		if (CapsulesNPC.empty()) return;
		if (CapsulesNPC.size() != OriginalCapsuleRadius.size()) return;
		if (CapsulesNPC.size() != OriginalCapsuleA.size()) return;
		if (CapsulesNPC.size() != OriginalCapsuleB.size()) return;

		for (size_t i = 0ull; i < CapsulesNPC.size(); i++) {
		 	//Set Sphere Radius, This In Reality is the Scale.
		 	//The 0.8f is arbitrairy, i find the default shape gets stupidly large so just offset its scale a bit.
		 	CapsulesNPC[i]->radius = OriginalCapsuleRadius[i] * ActorScale * 0.8f;
	
		 	//Get The Ground Offset for VertexB So It does not go through the floor
		 	const float GroundOffset = Utils::SphereOffset(OriginalCapsuleRadius[i], CapsulesNPC[i]->radius);
		 	//Set Head, GetQuad Needs a fixed 1.45x offset compared to the ConvexShape, Subtract Original Z Value From Vertex
		 	CapsulesNPC[i]->vertexB.quad.m128_f32[2] = OriginalCapsuleB[i].quad.m128_f32[2] + GroundOffset;
	
		 	float TargetZ = Utils::GetHeadQuad(NiActor.get(), 1.45f).quad.m128_f32[2] - OriginalCapsuleA[i].quad.m128_f32[2];
		 	float BottomZ = CapsulesNPC[i]->vertexB.quad.m128_f32[2];        //The the bottom Vertex We Just Set
		 	float RealZ = TargetZ < BottomZ ? BottomZ + 0.01f : TargetZ;     //If Target is lower than bottom Get Bottom + 0.01 else get Top;
	
		 	//Set The Top Vertex Z-Pos
		 	CapsulesNPC[i]->vertexA.quad.m128_f32[2] = RealZ;
	
		 	//Move The Shape forward Depending On Scale Index 1 is Y Pos
		 	CapsulesNPC[i]->vertexA.quad.m128_f32[1] = OriginalCapsuleA[i].quad.m128_f32[1] * ActorScale;
		 	CapsulesNPC[i]->vertexB.quad.m128_f32[1] = OriginalCapsuleB[i].quad.m128_f32[1] * ActorScale;
		}
	}
}

//NPC's
void AdjustmentHandler::ControllerData::AdjustProxyCapsuleSimple() {
	NiPointer<Actor> NiActor = ActorHandle.get();
	if (!NiActor)return;

	TESObjectCELL* Cell = NiActor->GetParentCell();
	if (!Cell) return;

	NiPointer<bhkWorld> World = NiPointer(Cell->GetbhkWorld());
	if (!World) return;

	if (!CharController) return;
	if (IsCreature) return;

	TESRace* Race = NiActor->GetRace();
	if (!Race) return;

	Actor* ActorPtr = NiActor.get();
	if (!ActorPtr) return;

	int8_t shapeIdx = 1;
	if (!CharController->shapes[shapeIdx]) shapeIdx = 0;
	if (!CharController->shapes[shapeIdx]) return;

	//NPC's Get the RigidBodyController.
	bhkCharRigidBodyController* RigidBodyController = skyrim_cast<bhkCharRigidBodyController*>(CharController);
	if (!RigidBodyController) return;

	std::vector<hkpCapsuleShape*> CapsulesNPC{};
	GetCapsules(RigidBodyController, CapsulesNPC);
	if (CapsulesNPC.empty()) return;
	if (CapsulesNPC.size() != OriginalCapsuleRadius.size()) return;
	if (CapsulesNPC.size() != OriginalCapsuleA.size()) return;
	if (CapsulesNPC.size() != OriginalCapsuleB.size()) return;

	for (size_t i = 0ull; i < CapsulesNPC.size(); i++) {
		CapsulesNPC[i]->radius = OriginalCapsuleRadius[i] * ActorScale * 0.8f;

		//Get The Ground Offset for VertexB So It does not go through the floor
		const float GroundOffset = Utils::SphereOffset(OriginalCapsuleRadius[i], CapsulesNPC[i]->radius);
		CapsulesNPC[i]->vertexB.quad.m128_f32[2] = OriginalCapsuleB[i].quad.m128_f32[2] + GroundOffset;

		//Set The Top Vertex Z-Pos
		CapsulesNPC[i]->vertexA.quad.m128_f32[2] = OriginalCapsuleA[i].quad.m128_f32[2] * ActorScale;

		//Move The Shape forward Depending On Scale
		CapsulesNPC[i]->vertexA.quad.m128_f32[1] = OriginalCapsuleA[i].quad.m128_f32[1] * ActorScale;
		CapsulesNPC[i]->vertexB.quad.m128_f32[1] = OriginalCapsuleB[i].quad.m128_f32[1] * ActorScale;
	}
}

//Creatures, Skip Creatures. Im too dumb to fix them
void AdjustmentHandler::ControllerData::AdjustProxyCapsuleCreature(){
	std::vector<hkpCapsuleShape*> CapsulesTest{};

	NiPointer<Actor> NiActor = ActorHandle.get();
	if (!NiActor) return;

	//logger::info("Handle");
	TESObjectCELL* Cell = NiActor->GetParentCell();
	if (!Cell) return;
	
	NiPointer<bhkWorld> World = NiPointer(Cell->GetbhkWorld());
	if (!World) return;

	//logger::info("World");
	if (!CharController) return;

	int8_t shapeIdx = 1;
	if (!CharController->shapes[shapeIdx]) shapeIdx = 0;
	if (!CharController->shapes[shapeIdx]) return;

	//readShape(readShape, static_cast<hkpShape*>(bhkClone->referencedObject.get()));

	if (!bhkClone){
		//logger::info("Clone was null");
		return;
	}
	
	//auto hkpTest = static_cast<hkpShape*>(bhkClone->referencedObject.get());

	//logger::info("Prerigid");
	//if (bhkCharRigidBodyController* RigidBodyController = skyrim_cast<bhkCharRigidBodyController*>(CharController)) {
		// std::vector<hkpCapsuleShape*> CapsulesCreature{};
		// GetCapsules(RigidBodyController, CapsulesCreature);
		// if (CapsulesCreature.empty()) return;
		//
		// if (CapsulesCreature.size() != OriginalCapsuleRadius.size()) return;
		// if (CapsulesCreature.size() != OriginalCapsuleA.size()) return;
		// if (CapsulesCreature.size() != OriginalCapsuleB.size()) return;
	
		//logger::info("PostRigid");
		//
		//hkRefPtr RigidBody(static_cast<hkpCharacterRigidBody*>(RigidBodyController->rigidBody.referencedObject.get()));
		//NiPointer Wrapper(RigidBody->character->collidable.shape->userData);
		// if (Wrapper) {
		// 	bhkClone = NiPointer(Utils::bhkClone<bhkShape>(Wrapper.get(), { ActorScale, ActorScale, ActorScale }));
		// 	RigidBody->character->SetShape(static_cast<hkpShape*>(bhkClone->referencedObject.get()));
		// 	CharController->shapes[shapeIdx]->DecRefCount();
		// 	CharController->shapes[shapeIdx] = bhkClone;
		// }
		//auto x = fmt::ptr(&bhkClone);
		//logger::info("Ptr of NIclone {:p}", (void*)fmt::ptr(&bhkClone));
		//logger::info("Ptr of clone {p:}", std::addressof(bhkClone.get());
		//logger::info("Ptr of cloneref {p:}", (void*)fmt::ptr(&(bhkClone->referencedObject)));

		hkpListShape* orig_capsule = static_cast<hkpListShape*>(bhkClone->referencedObject.get());
		//const hkpListShape*
	
		// hkTransform identity;
		// identity.rotation.col0 = hkVector4(1.0, 0.0, 0.0, 0.0);
		// identity.rotation.col1 = hkVector4(0.0, 1.0, 0.0, 0.0);
		// identity.rotation.col2 = hkVector4(0.0, 0.0, 1.0, 0.0);
		// identity.translation = hkVector4(0.0, 0.0, 0.0, 1.0);
		// hkAabb out;
		// orig_capsule->GetAabbImpl(identity, 1e-3f, out);
		// float min[4];
		// float max[4];
		// _mm_store_ps(&min[0], out.min.quad);
		// _mm_store_ps(&max[0], out.max.quad);
		//logger::info(" - Current bounds: {},{},{}<{},{},{}", min[0], min[1], min[2], max[0], max[1], max[2]);
		// Here be dragons
		//auto capsuleList = const_cast<hkpListShape*>(orig_capsule);
		//auto shapescnt = capsuleList->GetNumChildShapes();
		//logger::info("shapescnt? {}", shapescnt);
		hkpShapeBuffer buff;
		//
		// if (auto child = const_cast<hkpShape*>(childInfo.shape)) {
		// 	self(self, child);
		// }
	
		auto yy = (hkpShape*)orig_capsule->GetChildShape(0, buff);
		//logger::info("  - Child0 found: {}", typeid(*yy).name());
		auto capsule = (hkpCapsuleShape*)yy;
	
		//logger::info("  - Capsule found: {}", typeid(*orig_capsule).name());
		//logger::info("Scale? {}, {}", ActorScale, OldActorScale);
	
		float scale_factor = ActorScale / OldActorScale;
		OldActorScale = ActorScale;
		//RigidBodyController->scale = ActorScale;
		 
		 // NiActor->inGameFormFlags |= TESForm::InGameFormFlag::kForcedPersistent;
		 // NiActor->formFlags |= TESForm::RecordFlags::RecordFlag::kPersistent | TESForm::RecordFlags::RecordFlag::kDisableFade;
	
		//hkVector4 vec_scale = hkVector4(scale_factor);
		capsule->vertexA.quad.m128_f32[2] = capsule->vertexA.quad.m128_f32[2] * scale_factor;
		//capsule->vertexB = capsule->vertexB * scale_factor;
		capsule->radius *= scale_factor;


		if (bhkCharRigidBodyController* RigidBodyController = skyrim_cast<bhkCharRigidBodyController*>(CharController))
		{
			hkRefPtr RigidBody(static_cast<hkpCharacterRigidBody*>(RigidBodyController->rigidBody.referencedObject.get()));
			if (RigidBody) {
				//NiPointer Wrapper(RigidBody->character->collidable.shape->userData);
				//NiPointer Wrapper2(RigidBody->character->collidable.shape);
				//if (Wrapper) {
					//hkpClone = NiPointer(Utils::Clone<hkpShape>((hkpShape*)(Wrapper2.get()), { ActorScale, ActorScale, ActorScale }));
					//bhkClone = NiPointer(Utils::Clone<bhkShape>(Wrapper.get(), { ActorScale, ActorScale, ActorScale }));
					RigidBody->character->SetShape(static_cast<hkpShape*>(bhkClone->referencedObject.get()));
					CharController->shapes[shapeIdx]->SetReferencedObject(bhkClone->referencedObject.get());
					CharController->shapes[shapeIdx] = bhkClone;
				//}
			}
	}
		 // capsule->GetAabbImpl(identity, 1e-3f, out);
		 // _mm_store_ps(&max[0], out.max.quad);
		 //
		//logger::info(" - New bounds: {},{},{}<{},{},{}", min[0], min[1], min[2], max[0], max[1], max[2]);
		//logger::info(" - pad28: {}", orig_capsule->pad);
		//logger::info(" - pad2C: {}", orig_capsule->pad2C);
		//logger::info(" - float(pad28): {}", static_cast<float>(orig_capsule->pad28));
		//logger::info(" - float(pad2C): {}", static_cast<float>(orig_capsule->pad2C));
	
	
		//const NiPointer Wrap(orig_capsule->userData);
		//BSWriteLockGuard lock(World->worldLock);
	
		//RigidBody->character->SetShape((orig_capsule));
		//CharController->shapes[shapeIdx] -> DecRefCount();
		//CharController->shapes[shapeIdx] = Wrap;
		//RigidBody->character->sh(capsule);
	
	
		// // // //TODO RigidBody->character->collidable.shape->userData Contains all of the child chapes we need to scale
		// if (RigidBody) {
		// 	NiPointer Wrapper(RigidBody->character->collidable.shape->userData);
		// 	if (Wrapper) {
		// 		auto Clone2 = NiPointer(Utils::bhkClone<bhkShape>(Wrapper.get(), { 1.0f, 1.0f, 1.0f }));
		// 		RigidBody->character->SetShape(static_cast<hkpShape*>(Clone2->referencedObject.get()));
		// 		CharController->shapes[shapeIdx] -> DecRefCount();
		// 		CharController->shapes[shapeIdx] = Clone2;
		// 	}
		// }
	
		//TODO So the only way to make this work is to...
		//Make a copy of the creatures parent hkplistshape the one that contains both the bumper and collider
		//SCale the bumper.
		//and reset it.
	
		// //logger::info("Actor Apply On [0x{:X}] {}", NiActor->formID, NiActor->GetName());
		// for (size_t i = 0ull; i < CapsulesTest.size(); i++) {
		// 	//Set Sphere Radius, This In Reality is the Scale.
		// 	//The 0.8f is arbitrairy, i find the default shape gets stupidly large so just offset its scale a bit.
		// 	//logger::info("CreatureUpdate, OrigRad {} Scale {} OldRad ", OriginalCapsuleRadius[i], ActorScale, CapsulesCreature[i]->radius);
		// 	CapsulesTest[i]->radius = OriginalCapsuleRadius[i] * ActorScale;
		// 	//CapsulesCreature[i]->vertexA.quad.m128_f32[2] = OriginalCapsuleA[i].quad.m128_f32[2] * ActorScale;
		// 	//RigidBody->character->SetShape(static_cast<hkpShape*>(CapsulesCreature[0]->userData->referencedObject.get()));
		//
		// 	//logger::info("NewRad {} ", CapsulesCreature[i]->radius);
		//
		// 	//Get The Ground Offset for VertexB So It does not go through the floor
		// 	//float GroundOffset = Utils::SphereOffset(OriginalCapsuleRadius[i], CapsulesCreature[i]->radius);
		// 	//Set Head, GetQuad Needs a fixed 1.5x offset compared to the ConvexShape, Subtract Original Z Value From Vertex
		// 	//CapsulesCreature[i]->vertexB.quad.m128_f32[2] = OriginalCapsuleB[i].quad.m128_f32[2] + GroundOffset;  // / (ActorScale * 1.2f);
		//
		// 	//float TargetZ = Utils::GetHeadQuad(NiActor.get(), 1.45f).quad.m128_f32[2] - OriginalCapsuleA[i].quad.m128_f32[2];
		// 	//float BottomZ = CapsulesCreature[i]->vertexB.quad.m128_f32[2];     //The the bottom Vertex We Just Set
		// 	//float RealZ = TargetZ < BottomZ ? BottomZ + 0.01f : TargetZ;  //If Target is lower than bottom Get Bottom + 0.01 else get Top;
		//
		// 	//Set The Top Vertex Z-Pos
		//
		//
		// 	//Move The Shape forward Depending On Scale Index 1 is Y Pos
		// 	//CapsulesCreature[i]->vertexA.quad.m128_f32[1] = OriginalCapsuleA[i].quad.m128_f32[1] * ActorScale;
		// 	//CapsulesCreature[i]->vertexB.quad.m128_f32[1] = OriginalCapsuleB[i].quad.m128_f32[1] * ActorScale;
		// }
		// RigidBody->character->SetShape(static_cast<hkpShape*>(bhkClone->referencedObject.get()));
	//}
}

//Potentially can cause Memory Leaks. Too Bad
//Creatures need a new clone on every scale change in order to prevent the game scaling similar skeletons together.
void AdjustmentHandler::ControllerData::AdjustProxyCapsuleCreature_Hack(){

	NiPointer<Actor> NiActor = ActorHandle.get();
	if (!NiActor)
		return;
	//logger::info("Handle");
	TESObjectCELL* Cell = NiActor->GetParentCell();
	if (!Cell)
		return;

	//NiPointer<bhkWorld> World = NiPointer(Cell->GetbhkWorld()->GetWorld2()
	NiPointer<bhkWorld> World = NiPointer(Cell->GetbhkWorld());

	if (!World)
		return;


	if (!CharController)
		return;

	int8_t shapeIdx = 1;
	if (!CharController->shapes[shapeIdx])
		shapeIdx = 0;
	if (!CharController->shapes[shapeIdx])
		return;

	//Setup NPC's
	if (bhkCharRigidBodyController* RigidBodyController = skyrim_cast<bhkCharRigidBodyController*>(CharController)) {
		hkRefPtr RigidBody(static_cast<hkpCharacterRigidBody*>(RigidBodyController->rigidBody.referencedObject.get()));
		if (RigidBody) {
			NiPointer Wrapper(RigidBody->character->collidable.shape->userData);
			float scale_factor = ActorScale / OldActorScale;
			OldActorScale = ActorScale;
			if (Wrapper) {
				auto NewClone = NiPointer(Utils::Clone<bhkShape>(Wrapper.get(), { scale_factor, scale_factor, scale_factor }));
				//BSReadWriteLock Lock(World->worldLock);
				//Lock.LockForRead();
				//Lock.LockForWrite();
				RigidBody->character->SetShape(static_cast<hkpShape*>(NewClone->referencedObject.get()));
				//logger::info("WorldOperation Result {}",(int)res);
				CharController->shapes[shapeIdx]->DecRefCount();
				CharController->shapes[shapeIdx] = NewClone;
				//Lock.UnlockForRead();
				//Lock.UnlockForWrite();
			}
		}
	}
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------------------
//	EXPERIMENTS
//------------------------------------------------------------------------------------------------------------------------------------------

// 	std::vector<NiAVObject*> GetAllNodes(Actor* actor) {
// 	if (!actor->Is3DLoaded()) {
// 		return {};
// 	}
// 	auto model = actor->Get3D();
// 	auto name = model->name;
//
// 	std::deque<NiAVObject*> queue;
// 	std::vector<NiAVObject*> nodes = {};
// 	queue.push_back(model);
//
// 	while (!queue.empty()) {
// 		auto currentnode = queue.front();
// 		queue.pop_front();
// 		try {
// 			if (currentnode) {
// 				auto ninode = currentnode->AsNode();
// 				if (ninode) {
// 					for (auto child : ninode->GetChildren()) {
// 						// Bredth first search
// 						nodes.push_back(child.get());
// 						// Depth first search
// 						//queue.push_front(child.get());
// 					}
// 				}
// 				// Do smth
// 				log::trace("Node {}", currentnode->name);
// 			}
// 		} catch (const std::overflow_error& e) {
// 			log::warn("Overflow: {}", e.what());
// 		}  // this executes if f() throws std::overflow_error (same type rule)
// 		catch (const std::runtime_error& e) {
// 			log::warn("Underflow: {}", e.what());
// 		}  // this executes if f() throws std::underflow_error (base class rule)
// 		catch (const std::exception& e) {
// 			log::warn("Exception: {}", e.what());
// 		}  // this executes if f() throws std::logic_error (base class rule)
// 		catch (...) {
// 			log::warn("Exception Other");
// 		}
// 	}
// 	return nodes;
// }
// void walk_nodes(Actor* actor) {
// 	if (!actor->Is3DLoaded()) {
// 		return;
// 	}
// 	auto model = actor->Get3D();
// 	auto name = model->name;
//
// 	std::deque<NiAVObject*> queue;
// 	queue.push_back(model);
//
// 	while (!queue.empty()) {  // Used to be while (!queue.empty())
// 		auto currentnode = queue.front();
// 		queue.pop_front();
// 		try {
// 			if (currentnode) {
// 				auto ninode = currentnode->AsNode();
// 				if (ninode) {
// 					for (auto child : ninode->GetChildren()) {
// 						// Bredth first search
// 						queue.push_back(child.get());
// 						// Depth first search
// 						//queue.push_front(child.get());
// 					}
// 				}
// 				// Do smth
// 				log::trace("Node {}", currentnode->name);
// 			}
// 		} catch (const std::overflow_error& e) {
// 			log::warn("Overflow: {}", e.what());
// 		}  // this executes if f() throws std::overflow_error (same type rule)
// 		catch (const std::runtime_error& e) {
// 			log::warn("Underflow: {}", e.what());
// 		}  // this executes if f() throws std::underflow_error (base class rule)
// 		catch (const std::exception& e) {
// 			log::warn("Exception: {}", e.what());
// 		}  // this executes if f() throws std::logic_error (base class rule)
// 		catch (...) {
// 			log::warn("Exception Other");
// 		}
// 	}
// }
//
// NiAVObject* find_node(Actor* actor, std::string_view node_name, bool first_person) {
// 	if (!actor->Is3DLoaded()) {
// 		return nullptr;
// 	}
// 	auto model = actor->Get3D(first_person);
// 	if (!model) {
// 		return nullptr;
// 	}
// 	auto node_lookup = model->GetObjectByName(node_name);
// 	if (node_lookup) {
// 		return node_lookup;
// 	}
//
// 	// Game lookup failed we try and find it manually
// 	std::deque<NiAVObject*> queue;
// 	queue.push_back(model);
//
// 	while (!queue.empty()) {  // Used to be while (!queue.empty())
// 		auto currentnode = queue.front();
// 		queue.pop_front();
// 		try {
// 			if (currentnode) {
// 				auto ninode = currentnode->AsNode();
// 				if (ninode) {
// 					for (auto child : ninode->GetChildren()) {
// 						// Bredth first search
// 						queue.push_back(child.get());
// 						// Depth first search
// 						//queue.push_front(child.get());
// 					}
// 				}
// 				// Do smth
// 				if (currentnode->name.c_str() == node_name) {
// 					return currentnode;
// 				}
// 			}
// 		} catch (const std::overflow_error& e) {
// 			log::warn("Overflow: {}", e.what());
// 		}  // this executes if f() throws std::overflow_error (same type rule)
// 		catch (const std::runtime_error& e) {
// 			log::warn("Underflow: {}", e.what());
// 		}  // this executes if f() throws std::underflow_error (base class rule)
// 		catch (const std::exception& e) {
// 			log::warn("Exception: {}", e.what());
// 		}  // this executes if f() throws std::logic_error (base class rule)
// 		catch (...) {
// 			log::warn("Exception Other");
// 		}
// 	}
//
// 	return nullptr;
// }
//
// NiAVObject* find_object_node(TESObjectREFR* object, std::string_view node_name) {
// 	auto model = object->GetCurrent3D();
// 	if (!model) {
// 		return nullptr;
// 	}
// 	auto node_lookup = model->GetObjectByName(node_name);
// 	if (node_lookup) {
// 		return node_lookup;
// 	}
//
// 	// Game lookup failed we try and find it manually
// 	std::deque<NiAVObject*> queue;
// 	queue.push_back(model);
//
// 	while (!queue.empty()) {  // Used to be while (!queue.empty())
// 		auto currentnode = queue.front();
// 		queue.pop_front();
// 		try {
// 			if (currentnode) {
// 				auto ninode = currentnode->AsNode();
// 				if (ninode) {
// 					for (auto child : ninode->GetChildren()) {
// 						// Bredth first search
// 						queue.push_back(child.get());
// 						// Depth first search
// 						//queue.push_front(child.get());
// 					}
// 				}
// 				// Do smth
// 				if (currentnode->name.c_str() == node_name) {
// 					return currentnode;
// 				}
// 			}
// 		} catch (const std::overflow_error& e) {
// 			log::warn("Overflow: {}", e.what());
// 		}  // this executes if f() throws std::overflow_error (same type rule)
// 		catch (const std::runtime_error& e) {
// 			log::warn("Underflow: {}", e.what());
// 		}  // this executes if f() throws std::underflow_error (base class rule)
// 		catch (const std::exception& e) {
// 			log::warn("Exception: {}", e.what());
// 		}  // this executes if f() throws std::logic_error (base class rule)
// 		catch (...) {
// 			log::warn("Exception Other");
// 		}
// 	}
//
// 	return nullptr;
// }
//
// NiAVObject* find_node_regex(Actor* actor, std::string_view node_regex, bool first_person) {
// 	if (!actor->Is3DLoaded()) {
// 		return nullptr;
// 	}
// 	auto model = actor->Get3D(first_person);
// 	if (!model) {
// 		return nullptr;
// 	}
//
// 	std::regex the_regex(std::string(node_regex).c_str());
//
// 	// Game lookup failed we try and find it manually
// 	std::deque<NiAVObject*> queue;
// 	queue.push_back(model);
//
// 	while (!queue.empty()) {  // Used to be while (!queue.empty())
// 		auto currentnode = queue.front();
// 		queue.pop_front();
// 		try {
// 			if (currentnode) {
// 				auto ninode = currentnode->AsNode();
// 				if (ninode) {
// 					for (auto child : ninode->GetChildren()) {
// 						// Bredth first search
// 						queue.push_back(child.get());
// 						// Depth first search
// 						//queue.push_front(child.get());
// 					}
// 				}
// 				// Do smth
// 				if (std::regex_match(currentnode->name.c_str(), the_regex)) {
// 					return currentnode;
// 				}
// 			}
// 		} catch (const std::overflow_error& e) {
// 			log::warn("Overflow: {}", e.what());
// 		}  // this executes if f() throws std::overflow_error (same type rule)
// 		catch (const std::runtime_error& e) {
// 			log::warn("Underflow: {}", e.what());
// 		}  // this executes if f() throws std::underflow_error (base class rule)
// 		catch (const std::exception& e) {
// 			log::warn("Exception: {}", e.what());
// 		}  // this executes if f() throws std::logic_error (base class rule)
// 		catch (...) {
// 			log::warn("Exception Other");
// 		}
// 	}
//
// 	return nullptr;
// }
//
// NiAVObject* find_node_any(Actor* actor, std::string_view name) {
// 	NiAVObject* result = nullptr;
// 	for (auto person : { false, true }) {
// 		result = find_node(actor, name, person);
// 		if (result) {
// 			break;
// 		}
// 	}
// 	return result;
// }
//
// NiAVObject* find_node_regex_any(Actor* actor, std::string_view node_regex)
// {
// 	NiAVObject* result = nullptr;
// 	for (auto person : { false, true }) {
// 		result = find_node_regex(actor, node_regex, person);
// 		if (result) {
// 			break;
// 		}
// 	}
// 	return result;
// }
//
// void scale_hkpnodes(Actor* actor, float prev_scale, float new_scale) {
// 	if (!actor->Is3DLoaded()) {
// 		return;
// 	}
// 	auto model = actor->Get3D();
// 	if (!model) {
// 		return;
// 	}
// 	// Game lookup failed we try and find it manually
// 	std::deque<NiAVObject*> queue;
// 	queue.push_back(model);
//
// 	while (!queue.empty()) {  // Used to be while (!queue.empty())
// 		auto currentnode = queue.front();
// 		queue.pop_front();
// 		try {
// 			if (currentnode) {
// 				auto ninode = currentnode->AsNode();
// 				if (ninode) {
// 					for (auto child : ninode->GetChildren()) {
// 						// Bredth first search
// 						queue.push_back(child.get());
// 						// Depth first search
// 						//queue.push_front(child.get());
// 					}
// 				}
// 				// Do smth
// 				auto collision_object = currentnode->GetCollisionObject();
// 				if (collision_object) {
// 					auto bhk_rigid_body = collision_object->GetRigidBody();
// 					if (bhk_rigid_body) {
// 						hkReferencedObject* hkp_rigidbody_ref = bhk_rigid_body->referencedObject.get();
// 						if (hkp_rigidbody_ref) {
// 							hkpRigidBody* hkp_rigidbody = skyrim_cast<hkpRigidBody*>(hkp_rigidbody_ref);
// 							if (hkp_rigidbody) {
// 								auto shape = hkp_rigidbody->GetShape();
// 								if (shape) {
// 									log::trace("Shape found: {} for {}", typeid(*shape).name(), currentnode->name.c_str());
// 									if (shape->type == hkpShapeType::kCapsule) {
// 										const hkpCapsuleShape* orig_capsule = static_cast<const hkpCapsuleShape*>(shape);
// 										hkTransform identity;
// 										identity.rotation.col0 = hkVector4(1.0, 0.0, 0.0, 0.0);
// 										identity.rotation.col1 = hkVector4(0.0, 1.0, 0.0, 0.0);
// 										identity.rotation.col2 = hkVector4(0.0, 0.0, 1.0, 0.0);
// 										identity.translation = hkVector4(0.0, 0.0, 0.0, 1.0);
// 										hkAabb out;
// 										orig_capsule->GetAabbImpl(identity, 1e-3, out);
// 										float min[4];
// 										float max[4];
// 										_mm_store_ps(&min[0], out.min.quad);
// 										_mm_store_ps(&max[0], out.max.quad);
// 										log::trace(" - Current bounds: {},{},{}<{},{},{}", min[0], min[1], min[2], max[0], max[1], max[2]);
// 										// Here be dragons
// 										hkpCapsuleShape* capsule = const_cast<hkpCapsuleShape*>(orig_capsule);
// 										log::trace("  - Capsule found: {}", typeid(*orig_capsule).name());
// 										float scale_factor = new_scale / prev_scale;
// 										hkVector4 vec_scale = hkVector4(scale_factor);
// 										capsule->vertexA = capsule->vertexA * vec_scale;
// 										capsule->vertexB = capsule->vertexB * vec_scale;
// 										capsule->radius *= scale_factor;
//
// 										capsule->GetAabbImpl(identity, 1e-3, out);
// 										_mm_store_ps(&min[0], out.min.quad);
// 										_mm_store_ps(&max[0], out.max.quad);
// 										log::trace(" - New bounds: {},{},{}<{},{},{}", min[0], min[1], min[2], max[0], max[1], max[2]);
// 										log::trace(" - pad28: {}", orig_capsule->pad28);
// 										log::trace(" - pad2C: {}", orig_capsule->pad2C);
// 										log::trace(" - float(pad28): {}", static_cast<float>(orig_capsule->pad28));
// 										log::trace(" - float(pad2C): {}", static_cast<float>(orig_capsule->pad2C));
//
// 										hkp_rigidbody->SetShape(capsule);
// 									}
// 								}
// 							}
// 						}
// 					}
// 				}
// 			}
// 		} catch (const std::overflow_error& e) {
// 			log::warn("Overflow: {}", e.what());
// 		}  // this executes if f() throws std::overflow_error (same type rule)
// 		catch (const std::runtime_error& e) {
// 			log::warn("Underflow: {}", e.what());
// 		}  // this executes if f() throws std::underflow_error (base class rule)
// 		catch (const std::exception& e) {
// 			log::warn("Exception: {}", e.what());
// 		}  // this executes if f() throws std::logic_error (base class rule)
// 		catch (...) {
// 			log::warn("Exception Other");
// 		}
// 	}
//
// 	return;
// }

//-----------------------
//	ConvexShape
//-----------------------

//Player & Followers
//The math here sucks
//TODO Fix Math
void AdjustmentHandler::ControllerData::AdjustConvexShape() {

	NiPointer<Actor> NiActor = ActorHandle.get();
	if (!NiActor) return;

	Actor* ActorPtr = NiActor.get();
	if (!ActorPtr) return;

	TESObjectCELL* Cell = NiActor->GetParentCell();
	if (!Cell) return;

	NiPointer<bhkWorld> World = NiPointer(Cell->GetbhkWorld());
	if (!World) return;

	//const float SwimmingMult = CharacterState == hkpCharacterStateType::kSwimming ? Settings::fSwimmingControllerShapeRadiusMultiplier : 1.f;

	hkpListShape* ListShape;
	hkpCharacterProxy* CharProxy;
	hkpConvexVerticesShape* ConvexShape;
	hkpCharacterRigidBody* CharRigidBody;

	//BSWriteLockGuard lock(World->worldLock);
	if (!GetConvexShape(CharController, CharProxy, CharRigidBody, ListShape, ConvexShape)) return;

	std::vector<hkVector4> NewVerts = OriginalVerts;

	if (OriginalVerts.size() == 18) {
		hkVector4 OrigTop = OriginalVerts[9];
		hkVector4 OrigBottom = OriginalVerts[8];
		hkVector4 NewTop = OrigTop * Utils::GetBoneQuad(ActorPtr, "NPC Head [Head]", false,true);
		hkVector4 Correction;

		CachedColliderHeight = ((OrigTop * 2.f) * ActorScale) + OrigBottom;

		//For Some Reason There is a linear Offset, Probably because i dont take worldscale into account
		Correction.quad.m128_f32[2] = (ActorScale * 0.32f) - 1.f;

		// Move the top vert
		NewVerts[9] = NewTop + Correction;

		if (NewVerts[9].quad.m128_f32[2] <= NewVerts[8].quad.m128_f32[2]) {
			NewVerts[9].quad.m128_f32[2] = NewVerts[8].quad.m128_f32[2] + 0.0003f;
		}

		// Move the top ring
		for (int i : { 1, 3, 4, 5, 7, 11, 13, 16 }) {
			float y = NewVerts[i].quad.m128_f32[2] * Utils::GetBoneQuad(ActorPtr, "NPC R Clavicle [RClv]", false,true).quad.m128_f32[2] + Correction.quad.m128_f32[2];
			float res = (y <= NewVerts[8].quad.m128_f32[2]) ? (NewVerts[8].quad.m128_f32[2] + 0.0002f) : y;
			NewVerts[i].quad.m128_f32[2] = res;
		}

		//Move the Bottom ring
		for (int i : { 0, 2, 6, 10, 12, 14, 15, 17 }) { //Bottom Ring
			float x = (NewVerts[i].quad.m128_f32[2] * Utils::GetBoneQuad(ActorPtr, "NPC R RearCalf [RrClf]", true,true).quad.m128_f32[2] + Correction.quad.m128_f32[2]);
			float res = (x <= NewVerts[8].quad.m128_f32[2]) ? (NewVerts[8].quad.m128_f32[2] + 0.0001f) : x;
			NewVerts[i].quad.m128_f32[2] = res;
		}

		// move the rings' vertices inwards or outwards
		for (int i : {
			     1, 3, 4, 5, 7, 11, 13, 16,   // top ring
				 0, 2, 6, 10, 12, 14, 15, 17  // bottom ring
			 }) {
			NiPoint3 vert = Utils::HkVectorToNiPoint(NewVerts[i]);
			NiPoint3 newVert = vert;
			newVert.z = 0;
			newVert.Unitize();
			newVert *= OriginalConvexRadius * ActorScale; //* SwimmingMult;
			newVert.z = vert.z;

			NewVerts[i] = Utils::NiPointToHkVector(newVert);
		}
	}

	hkStridedVertices StridedVerts(NewVerts.data(), static_cast<int>(NewVerts.size()));
	hkpConvexVerticesShape::BuildConfig BuildConfig{ false, false, true, 0.05f, 0, 0.f, 0.f, -0.1f };

	hkpConvexVerticesShape* NewShape = reinterpret_cast<hkpConvexVerticesShape*>(hkHeapAlloc(sizeof(hkpConvexVerticesShape)));
	hkpConvexVerticesShape_ctor(NewShape, StridedVerts, BuildConfig);  // sets refcount to 1

	// it's actually a hkCharControllerShape not just a hkpConvexVerticesShape
	reinterpret_cast<std::uintptr_t*>(NewShape)[0] = VTABLE_hkCharControllerShape[0].address();

	bhkShape* Wrapper = ConvexShape->userData;
	if (Wrapper) 
		Wrapper->SetReferencedObject(NewShape);
	
	// The listshape does not use a hkRefPtr but it's still setup to add a reference upon construction and remove one on destruction
	if (ListShape) {
		ListShape->childInfo[0].shape = NewShape;
		ConvexShape->RemoveReference();  // this will usually call the dtor on the old shape
	} 
	else {
		if (CharProxy) CharProxy->shapePhantom->SetShape(NewShape);
		else if (CharRigidBody) CharRigidBody->character->SetShape(NewShape);
		NewShape->RemoveReference();
	}
}

//NPC's
void AdjustmentHandler::ControllerData::AdjustConvexShapeSimple(){
	if (Settings::bEnableStateAdjustments) {
		if (auto actor = ActorHandle.get()) {
			RE::hkpCharacterProxy* proxy = nullptr;
			RE::hkpCharacterRigidBody* rigidBody = nullptr;
			RE::hkpListShape* listShape = nullptr;
			RE::hkpConvexVerticesShape* collisionConvexVerticesShape = nullptr;

			auto cell = actor->GetParentCell();
			if (!cell) {
				return;
			}

			auto world = RE::NiPointer<RE::bhkWorld>(cell->GetbhkWorld());
			if (!world) {
				return;
			}

			float sneakMult = (Sneaking && (CharacterState == RE::hkpCharacterStateType::kOnGround)) ? Settings::fSneakControllerShapeHeightMultiplier : 1.f;
			float swimmingHeightMult = CharacterState == RE::hkpCharacterStateType::kSwimming ? Settings::fSwimmingControllerShapeHeightMultiplier : 1.f;
			float swimmingRadiusMult = CharacterState == RE::hkpCharacterStateType::kSwimming ? Settings::fSwimmingControllerShapeRadiusMultiplier : 1.f;

			float heightMult = sneakMult * swimmingHeightMult * ActorScale;
			float heightMultTop = (sneakMult * swimmingHeightMult * ActorScale);
			float radiusMult = ActorScale * swimmingRadiusMult;

			//RE::BSWriteLockGuard lock(world->worldLock);

			if (GetConvexShape(CharController, proxy, rigidBody, listShape, collisionConvexVerticesShape)) {
				std::vector<RE::hkVector4> newVerts = OriginalVerts;
				if (OriginalVerts.size() == 18) {
					RE::hkVector4 topVert = OriginalVerts[9];
					RE::hkVector4 bottomVert = OriginalVerts[8];

					RE::hkVector4 newTopVert = ((topVert * 2.f) * heightMultTop) + bottomVert;
					float distance = topVert.GetDistance3(newTopVert);

					// Move the top vert

					newVerts[9] = newTopVert;
					// Move the top ring
					for (int i : { 1, 3, 4, 5, 7, 11, 13, 16 }) {  // top ring
						if (heightMult < 1.f) {
							newVerts[i].quad.m128_f32[2] = newVerts[i].quad.m128_f32[2] - distance;
						} else {
							newVerts[i].quad.m128_f32[2] = newVerts[i].quad.m128_f32[2] + distance;
						}
					}

					// move the rings' vertices inwards or outwards
					for (int i : {
							 1, 3, 4, 5, 7, 11, 13, 16,   // top ring
							 0, 2, 6, 10, 12, 14, 15, 17  // bottom ring
						 }) {
						RE::NiPoint3 vert = Utils::HkVectorToNiPoint(newVerts[i]);
						RE::NiPoint3 newVert = vert;
						newVert.z = 0;
						newVert.Unitize();
						newVert *= OriginalConvexRadius * radiusMult;
						newVert.z = vert.z;

						newVerts[i] = Utils::NiPointToHkVector(newVert);
					}
				}

				RE::hkStridedVertices stridedVerts(newVerts.data(), static_cast<int>(newVerts.size()));
				RE::hkpConvexVerticesShape::BuildConfig buildConfig{ false, false, true, 0.05f, 0, 0.f, 0.f, -0.1f };

				RE::hkpConvexVerticesShape* newShape = reinterpret_cast<RE::hkpConvexVerticesShape*>(hkHeapAlloc(sizeof(RE::hkpConvexVerticesShape)));
				hkpConvexVerticesShape_ctor(newShape, stridedVerts, buildConfig);  // sets refcount to 1

				// it's actually a hkCharControllerShape not just a hkpConvexVerticesShape
				reinterpret_cast<std::uintptr_t*>(newShape)[0] = RE::VTABLE_hkCharControllerShape[0].address();

				RE::bhkShape* wrapper = collisionConvexVerticesShape->userData;
				if (wrapper) {
					wrapper->SetReferencedObject(newShape);
				}

				// The listshape does not use a hkRefPtr but it's still setup to add a reference upon construction and remove one on destruction
				if (listShape) {
					listShape->childInfo[0].shape = newShape;
					collisionConvexVerticesShape->RemoveReference();  // this will usually call the dtor on the old shape

				// We don't need to remove a ref here, the ctor gave it a refcount of 1 and we assigned it to the listShape which isn't technically a hkRefPtr but still owns it (and the listShape's dtor will decref anyways)
				//newShape->RemoveReference();
				} else {
					if (proxy) {
						proxy->shapePhantom->SetShape(newShape);
					} else if (rigidBody) {
						rigidBody->character->SetShape(newShape);
					}

					newShape->RemoveReference();
				}
			}
		}
	}
}

//-----------------------
//	Controller Events
//-----------------------

//Update Sneak State
void AdjustmentHandler::ActorSneakStateChanged(ActorHandle ActorHandle, bool Sneaking) {
	if (std::shared_ptr<ControllerData> ControllerData = GetControllerData(ActorHandle)) {
		ControllerData->Sneaking = Sneaking;
	}
}

//Update Character State
void AdjustmentHandler::CharacterControllerStateChanged(bhkCharacterController* Controller, hkpCharacterStateType CurrentState) {
	if (std::shared_ptr<ControllerData> ControllerData = GetControllerData(Controller)) {
		ControllerData->CharacterState = CurrentState;
	}
}

//-----------------------
//	Main Update
//-----------------------

void AdjustmentHandler::ForEachController(std::function<void(std::shared_ptr<ControllerData>)> Func) {
	ReadLocker locker(ControllersLock);

	for (std::pair<bhkCharacterController* const, std::shared_ptr<ControllerData>>& Entry : ControllerMap) {
		Func(Entry.second);
	}
}

void AdjustmentHandler::Update() {
	//Dont Run if paused
	if (UI::GetSingleton()->GameIsPaused()) {
		return;
	}

	ActorHandle PlayerHandle = PlayerCharacter::GetSingleton()->GetHandle();
	if (!PlayerHandle) return;

	auto ActorPtr = PlayerHandle.get();
	if (!ActorPtr) return;

	auto Cell = ActorPtr->GetParentCell();
	if (!Cell) return;

	auto World = Cell->GetbhkWorld();
	if (!World) return;

	BSReadWriteLock Lock(World->worldLock);

	Lock.LockForWrite();
	ForEachController([&](std::shared_ptr<ControllerData> Entry) {
		CharacterControllerUpdate(Entry->CharController);
	});
	Lock.UnlockForWrite();
}

void AdjustmentHandler::CharacterControllerUpdate(bhkCharacterController* Controller) {
	if (!Settings::bEnableActorScaleFix) return;

	std::shared_ptr<ControllerData> ControllerData = GetControllerData(Controller);
	if (!ControllerData) return;

	NiPointer<Actor> NiActor = ControllerData->ActorHandle.get();
	if (!NiActor) return;
	if (NiActor->IsDead()) return;

	Actor* ActorPtr = NiActor.get();
	if (!ActorPtr) return;

	float CurrentScale = Utils::GetScale(ActorPtr);
	bool ScaleUnchanged = Utils::FloatsEqual(CurrentScale, ControllerData->ActorScale);
	//bool ScaleUnchancedBigDelta = Utils::FloatsEqualDelta(ControllerData->OldActorScale, ControllerData->ActorScale, 0.1f);

	//Update Scale
	ControllerData->ActorScale = CurrentScale; 

	//The Player And Followers Get Realtime ConvexShape Update Based On Bone Position
	if (ActorPtr->formID == 0x14 || ActorPtr->IsPlayerTeammate()) {
		ControllerData->AdjustConvexShape();
		ControllerData->AdjustProxyCapsule();
		return;
	}
	//Non Creatures NPC's Get A Simpeler Scale Based One. Only Update If Scale Unchanged
	if(!ScaleUnchanged && !ControllerData->IsCreature) {
		ControllerData->AdjustConvexShapeSimple();
		ControllerData->AdjustProxyCapsuleSimple();
		return;
	}

	// if (ControllerData->IsCreature && !ScaleUnchancedBigDelta) {
	// 	ControllerData->AdjustProxyCapsuleCreature_Hack();
	// }

}

//-----------------------
//	Debug Draw
//-----------------------

void DrawLine(hkVector4& Start, hkVector4& End, NiPoint3& ControllerPosition) {
	NiPoint3 S = Utils::HkVectorToNiPoint(Start) * *g_worldScaleInverse;
	NiPoint3 E = Utils::HkVectorToNiPoint(End) * *g_worldScaleInverse;
	S += ControllerPosition;
	E += ControllerPosition;
	Settings::g_trueHUD->DrawLine(S, E, 0.f, 0x00FF00FF,2);
}

void AdjustmentHandler::DebugDraw() {
	TRUEHUD_API::IVTrueHUD4* TrueHUD = Settings::g_trueHUD;
	if (!TrueHUD || Settings::uDisplayDebugShapes == DebugDrawMode::kNone) return;

	if (UI::GetSingleton()->GameIsPaused()) return;

	auto DrawCharController = [&](bhkCharacterController* Controller, ActorHandle Handle) {
		if (!Controller) return;

		NiPointer<Actor> NiActor = Handle.get();
		if (!NiActor) return;
		if (NiActor->IsDead()) return;

		const hkpConvexVerticesShape* CollisionConvexVertexShape = nullptr;
		std::vector<hkpCapsuleShape*> CollisionCapsules{};

		TESObjectCELL* Cell = NiActor->GetParentCell();
		if (!Cell) return;

		NiPointer<bhkWorld> World = NiPointer(Cell->GetbhkWorld());
		if (!World) return;

		{
			BSReadLockGuard WorldLock(World->worldLock);
			GetShapes(Controller, CollisionConvexVertexShape, CollisionCapsules);
		}

		hkVector4 ControllerPosition;
		Controller->GetPosition(ControllerPosition, false);
		NiPoint3 ContollerNiPosition = Utils::HkVectorToNiPoint(ControllerPosition) * *g_worldScaleInverse;

		if (CollisionConvexVertexShape) {
			// The charcontroller shape is composed of two vertically concentric "rings" with a single point above and below the top/bottom ring.
			// verts 0,2,6,10,12,14,15,17 are bottom ring, 8-9 are bottom/top points, 1,3,4,5,7,11,13,16 are top ring
			hkArray<hkVector4> Verts{};
			hkpConvexVerticesShape_getOriginalVertices(CollisionConvexVertexShape, Verts);

			if (Verts.size() == 18) {
				// draw top ring of verts
				std::vector topRing = { 1, 4, 13, 7, 3, 16, 5, 11, 1 };
				for (std::vector<int>::iterator Itter = topRing.begin(); Itter != topRing.end() - 1; ++Itter) {
					DrawLine(Verts[*Itter], Verts[*(Itter + 1)], ContollerNiPosition);
				}

				// draw bottom ring of verts
				std::vector bottomRing = { 0, 2, 12, 6, 15, 17, 14, 10, 0 };
				for (std::vector<int>::iterator Itter = bottomRing.begin(); Itter != bottomRing.end() - 1; ++Itter) {
					DrawLine(Verts[*Itter], Verts[*(Itter + 1)], ContollerNiPosition);
				}

				// draw vertical lines
				std::vector pairs = { 1, 0, 4, 2, 13, 12, 7, 6, 3, 15, 16, 17, 5, 14, 11, 10, 9, 1, 9, 4, 9, 13, 9, 7, 9, 3, 9, 16, 9, 5, 9, 11, 8, 0, 8, 2, 8, 12, 8, 6, 8, 15, 8, 17, 8, 14, 8, 10, 0, 0 };
				for (std::vector<int>::iterator Itter = pairs.begin(); Itter != pairs.end() - 2;) {
					DrawLine(Verts[*Itter], Verts[*(Itter + 1)], ContollerNiPosition);
					Itter += 2;
				}
			}
			else if (Verts.size() == 17) {  // very short - no top vert. 8 is bottom
				// draw top ring of verts
				std::vector topRing = { 1, 4, 12, 7, 3, 15, 5, 10, 1 };
				for (std::vector<int>::iterator Itter = topRing.begin(); Itter != topRing.end() - 1; ++Itter) {
					DrawLine(Verts[*Itter], Verts[*(Itter + 1)], ContollerNiPosition);
				}

				// draw bottom ring of verts
				std::vector bottomRing = { 0, 2, 11, 6, 14, 16, 13, 9, 0 };
				for (std::vector<int>::iterator Itter = bottomRing.begin(); Itter != bottomRing.end() - 1; ++Itter) {
					DrawLine(Verts[*Itter], Verts[*(Itter + 1)], ContollerNiPosition);
				}

				// draw vertical lines
				std::vector pairs = { 1, 0, 4, 2, 12, 11, 7, 6, 3, 14, 15, 16, 5, 13, 10, 9, 8, 0, 8, 2, 8, 11, 8, 6, 8, 14, 8, 16, 8, 13, 8, 9, 0, 0 };
				for (std::vector<int>::iterator Itter = pairs.begin(); Itter != pairs.end() - 2;) {
					DrawLine(Verts[*Itter], Verts[*(Itter + 1)], ContollerNiPosition);
					Itter += 2;
				}
			}
		}

		for (hkpCapsuleShape*& Capsule : CollisionCapsules) {
			constexpr NiPoint3 UpVector{ 0.f, 0.f, 1.f };
			NiQuaternion Rotation{};
			Rotation.w = 1.f;

			//auto collisionCapsuleHeight = capsule->vertexA.GetDistance3(capsule->vertexB) * *g_worldScaleInverse;
			NiPoint3 ColCapsuleOrigin = Utils::HkVectorToNiPoint((Capsule->vertexA + Capsule->vertexB) / 2.f) * *g_worldScaleInverse;
			ColCapsuleOrigin = Utils::RotateAngleAxis(ColCapsuleOrigin, -NiActor->data.angle.z, UpVector);
			ColCapsuleOrigin += ContollerNiPosition;

			float ColCapsuleRadius = Capsule->radius * *g_worldScaleInverse;
			NiPoint3 A = Utils::HkVectorToNiPoint(Capsule->vertexA) * *g_worldScaleInverse;
			NiPoint3 B = Utils::HkVectorToNiPoint(Capsule->vertexB) * *g_worldScaleInverse;

			A = Utils::RotateAngleAxis(A, -NiActor->data.angle.z, UpVector);
			B = Utils::RotateAngleAxis(B, -NiActor->data.angle.z, UpVector);

			A += ContollerNiPosition;
			B += ContollerNiPosition;

			uint32_t Color = 0xFFFF00FF;
			if (bhkShape* Shape = Capsule->userData) {
				// mislabeled in clib, it's the character bumper material
				if (Shape->materialID == MATERIAL_ID::kDragonSkeleton) {
					if (!Settings::bDisplayCharacterBumper) {
						continue;
					}
					Color = 0x004087FF;
				}
			}
			TrueHUD->DrawCapsule(A, B, ColCapsuleRadius, 0.f, Color);
		}
	};

	switch (Settings::uDisplayDebugShapes) {
		case DebugDrawMode::kNone: {
			return;
		}

		case DebugDrawMode::kAdjusted: {
			ForEachController([&](std::shared_ptr<ControllerData> Entry) {
				DrawCharController(Entry->CharController, Entry->ActorHandle);
			});
			break;
		}

		case DebugDrawMode::kAll: {
			ActorHandle PlayerHandle = PlayerCharacter::GetSingleton()->GetHandle();
			bhkCharacterController* PlayerController = PlayerCharacter::GetSingleton()->GetCharController();
			DrawCharController(PlayerController, PlayerHandle);

			for (ActorHandle& ActorHandle : ProcessLists::GetSingleton()->highActorHandles) {
				if (NiPointer<Actor> NiActor = ActorHandle.get()) {
					if (bhkCharacterController* Controller = NiActor->GetCharController()) {
						DrawCharController(Controller, ActorHandle);
					}
				}
			}
			break;
		}
	}
}

//-----------------------
//	Misc
//-----------------------

bool AdjustmentHandler::CheckEnoughSpaceToStand(ActorHandle ActorHandle) {
	NiPointer<Actor> NiActor = ActorHandle.get();
	if (!NiActor) return true;

	TESObjectCELL* Cell = NiActor->parentCell;
	if (!Cell) return true;

	bhkWorld* World = Cell->GetbhkWorld();
	if (!World) return true;

	bhkCharacterController* CharController = NiActor->GetCharController();
	if (!CharController) return true;

	std::shared_ptr<ControllerData> ControllerData = GetControllerData(ActorHandle);
	if (!ControllerData) return true;
	if (ControllerData->OriginalVerts.size() != 18) return true;

	hkVector4 ControllerPos, RayStart, RayEnd;
	CharController->GetPosition(ControllerPos, false);

	RayStart = ControllerPos;
	RayEnd = RayStart + ControllerData->CachedColliderHeight;

	hkpWorldRayCastInput RaycastInput;
	hkpWorldRayCastOutput RaycastOutput;

	uint32_t ColisionfilterInfo = 0;
	NiActor->GetCollisionFilterInfo(ColisionfilterInfo);
	uint16_t ColisionGroup = ColisionfilterInfo >> 16;
	RaycastInput.filterInfo = static_cast<uint32_t>(ColisionGroup) << 16 | static_cast<uint32_t>(COL_LAYER::kCharController);
	RaycastInput.from = RayStart;
	RaycastInput.to = RayEnd;

	{
		//BSReadLockGuard lock(World->worldLock);
		World->GetWorld1()->CastRay(RaycastInput, RaycastOutput);
	}

	if (RaycastOutput.HasHit()) {
		if (Settings::g_trueHUD && Settings::uDisplayDebugShapes != DebugDrawMode::kNone) {
			Settings::g_trueHUD->DrawArrow(Utils::HkVectorToNiPoint(RayStart, true), Utils::HkVectorToNiPoint(RayEnd, true), 10.f, 1.f);
		}
		return false;
	}

	return true;
}

//-----------------------
//	List
//-----------------------

void AdjustmentHandler::AddControllerToMap(bhkCharacterController* Controller, ActorHandle Handle) {
	WriteLocker lock(ControllersLock);
	ControllerMap.emplace(Controller, std::make_shared<ControllerData>(Controller, Handle));
}

void AdjustmentHandler::RemoveControllerFromMap(bhkCharacterController* Controller) {
	WriteLocker lock(ControllersLock);
	ControllerMap.erase(Controller);
}

//-----------------------
//	List
//-----------------------

bool AdjustmentHandler::CheckSkeletonForCollisionShapes(RE::NiAVObject* Object)
{
	if (!Object) {
		return false;
	}

	if (auto& collisionObject = Object->collisionObject) {
		if (auto bhkCollisionObject = collisionObject->AsBhkNiCollisionObject()) {
			if (bhkCollisionObject->body) {
				if (auto& referencedObject = bhkCollisionObject->body->referencedObject) {
					if (auto worldObject = static_cast<RE::hkpWorldObject*>(referencedObject.get())) {
						if (auto collidable = worldObject->GetCollidable()) {
							if (collidable->shape && collidable->shape->userData) {
								auto layer = static_cast<RE::COL_LAYER>(collidable->broadPhaseHandle.collisionFilterInfo & 0x7F);
								if (layer == RE::COL_LAYER::kCharController) {
									//Layer Should be 1574477864 not actually dragon skeleton, this is mislabeled in clib
									if (collidable->shape->userData->materialID != RE::MATERIAL_ID::kDragonSkeleton) {
										return true;
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if (auto node = Object->AsNode()) {
		if (node->children.size() > 0) {
			for (auto& child : node->children) {
				if (CheckSkeletonForCollisionShapes(child.get())) {
					return true;
				}
			}
		}
	}

	return false;
}
