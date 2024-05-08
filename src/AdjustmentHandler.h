#pragma once

#include "Havok.h"

#include <shared_mutex>

class AdjustmentHandler {

	public:

	struct ControllerData {
		ControllerData(RE::bhkCharacterController* Controller, RE::ActorHandle& Handle) : CharController(Controller), ActorHandle(Handle) {
			Initialize();
			SetupProxyCapsule();
		}

		void Initialize();
		void AdjustProxyCapsule();
		void AdjustProxyCapsuleSimple();
		void AdjustProxyCapsuleCreature();
		void AdjustProxyCapsuleCreature_Hack();
		void AdjustConvexShape();
		void AdjustConvexShapeSimple();
		void SetupProxyCapsule();

		RE::bhkCharacterController* CharController;
		RE::ActorHandle ActorHandle;

		//Game Scale
		float VActorScale = 1.f;

		//Calculated Scale (GTS)
		float ActorScale = 1.f;
		float OldActorScale = 1.f;

		bool Sneaking = false;
		bool IsInitialized = false;

		RE::NiPointer<RE::bhkShape> bhkClone = nullptr;
		RE::hkpCharacterStateType CharacterState = RE::hkpCharacterStateType::kOnGround;

		//ConvexShape
		float OriginalConvexRadius;
		bool IsCreature;
		std::vector<RE::hkVector4> OriginalVerts{};

		//CapsuleShape
		std::vector<float> OriginalCapsuleRadius{};
		std::vector<RE::hkVector4> OriginalCapsuleA{};
		std::vector<RE::hkVector4> OriginalCapsuleB{};
		RE::hkVector4 CachedColliderHeight;
	};

	static AdjustmentHandler* GetSingleton() {
		static AdjustmentHandler Singleton;
		return std::addressof(Singleton);
	}

	void ActorSneakStateChanged(RE::ActorHandle ActorHandle, bool Sneaking);
	void CharacterControllerStateChanged(RE::bhkCharacterController* Controller, RE::hkpCharacterStateType CurrentState);
	void CharacterControllerUpdate(RE::bhkCharacterController* Controller);
	static bool CheckEnoughSpaceToStand(RE::ActorHandle ActorHandle);

	void DebugDraw();
	void Update();

	static void AddControllerToMap(RE::bhkCharacterController* Controller, RE::ActorHandle Handle);
	static void RemoveControllerFromMap(RE::bhkCharacterController* Controller);
	static void ForEachController(std::function<void(std::shared_ptr<ControllerData>)> Func);
	static bool CheckSkeletonForCollisionShapes(RE::NiAVObject* Object);

	private:

	using Lock = std::shared_mutex;
	using ReadLocker = std::shared_lock<Lock>;
	using WriteLocker = std::unique_lock<Lock>;
	static inline Lock ControllersLock;
	
	
	AdjustmentHandler() = default;
	AdjustmentHandler(const AdjustmentHandler&) = delete;
	AdjustmentHandler(AdjustmentHandler&&) = delete;
	virtual ~AdjustmentHandler() = default;

	AdjustmentHandler& operator=(const AdjustmentHandler&) = delete;
	AdjustmentHandler& operator=(AdjustmentHandler&&) = delete;
	static bool GetShapes(RE::bhkCharacterController* CharController, const RE::hkpConvexVerticesShape*& OutConvexShape, std::vector<RE::hkpCapsuleShape*>& OutColisionShape);
	static bool GetConvexShape(RE::bhkCharacterController* CharController, RE::hkpCharacterProxy*& OutProxy, RE::hkpCharacterRigidBody*& OutRigidBody, RE::hkpListShape*& OutListshape, RE::hkpConvexVerticesShape*& OutConvexShape);
	static bool GetCapsules(RE::bhkCharacterController* CharController, std::vector<RE::hkpCapsuleShape*>& OutCollisionCapsules);

	static std::shared_ptr<ControllerData> GetControllerData(RE::ActorHandle Handle);
	static std::shared_ptr<ControllerData> GetControllerData(RE::bhkCharacterController* CharController);

	static inline std::unordered_map<RE::bhkCharacterController*, std::shared_ptr<ControllerData>> ControllerMap{};
	
};
