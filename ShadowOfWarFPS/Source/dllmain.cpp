#include "pch.h"

#define input(x) ((GetAsyncKeyState(x) & 0x8000) != 0)

namespace sow
{
	struct Vec3f {
		float x, y, z;

		Vec3f operator+(const Vec3f& other) {
			return { x + other.x, y + other.y, z + other.z };
		}

		Vec3f operator*(float scale) {
			return { x * scale, y * scale, z * scale };
		}
	};

	struct Quaternion {
		float w, x, y, z;

		Quaternion operator*(const Quaternion& q) const {
			return {
				w * q.w - x * q.x - y * q.y - z * q.z,
				w * q.x + x * q.w + y * q.z - z * q.y,
				w * q.y - x * q.z + y * q.w + z * q.x,
				w * q.z + x * q.y - y * q.x + z * q.w
			};
		}

		void normalize() {
			float mag = std::sqrt(w * w + x * x + y * y + z * z);
			if (mag > 0.0f) {
				w /= mag;
				x /= mag;
				y /= mag;
				z /= mag;
			}
		}

		Quaternion conjugate() const {
			return { w, -x, -y, -z };
		}

		float signedAngleOnAxis(const Quaternion& other, float ax, float ay, float az) const {
			Quaternion qDiff = conjugate() * other;
			qDiff.normalize();

			float magAxis = std::sqrt(ax * ax + ay * ay + az * az);
			if (magAxis > 0.0f) {
				ax /= magAxis;
				ay /= magAxis;
				az /= magAxis;
			}

			float angle = 2.0f * std::atan2(qDiff.x * ax + qDiff.y * ay + qDiff.z * az, qDiff.w);
			if (angle > 3.14159) angle -= 2.0f * 3.14159;
			if (angle < -3.14159) angle += 2.0f * 3.14159;

			return angle;
		}

		Vec3f rotateVector(const Vec3f& v) const {
			Quaternion vectorQuat = { 0, v.x, v.y, v.z };
			Quaternion result = (*this) * vectorQuat * conjugate();
			return { result.x, result.y, result.z };
		}

		static Quaternion fromAxisAngle(float ax, float ay, float az, float angle) {
			float halfAngle = angle * 0.5f;
			float sinHalfAngle = std::sin(halfAngle);
			float cosHalfAngle = std::cos(halfAngle);
			return {
				cosHalfAngle,
				ax * sinHalfAngle,
				ay * sinHalfAngle,
				az * sinHalfAngle
			};
		}

		Quaternion rotate(float ax, float ay, float az, float angle) const {
			Quaternion rotation = Quaternion::fromAxisAngle(ax, ay, az, angle);
			Quaternion conjugate = { rotation.w, -rotation.x, -rotation.y, -rotation.z };
			Quaternion result = rotation * (*this) * conjugate;
			return result;
		}

		void setRotation(float ax, float ay, float az, float angle) {
			*this = Quaternion::fromAxisAngle(ax, ay, az, angle);
		}
	};




	class CameraTransform
	{
	private:
		char pad[0xAC];
	public:
		Vec3f position;
		Quaternion rotation;
	};

	struct EntityTransform : public CameraTransform {
		float size;
	};

	class Camera
	{
	private:
		char pad[0x98];
	public:
		CameraTransform* transform;
	};

	class CameraOwner
	{
	private:
		char pad[0x50];
	public:
		Camera* gameplay_camera;

		int get_mode()
		{
			return *(int*)((uintptr_t)this + 0x80);
		}
	};

	class GameClient
	{
	private:
		char pad[0x898];
	public:
		CameraOwner* camera_owner;
	};
}



std::string hex_to_bytes(std::string hex)
{
	std::string bytes;
	hex.erase(std::remove_if(hex.begin(), hex.end(), isspace), hex.end());
	for (uint32_t i = 0; i < hex.length(); i += 2)
	{
		if ((BYTE)hex[i] == '?')
		{
			bytes += '?';
			i -= 1;
			continue;
		}
		BYTE byte = (BYTE)std::strtol(hex.substr(i, 2).c_str(), nullptr, 16);
		bytes += byte;
	}
	return bytes;
}

uintptr_t sigscan(const char* pattern)
{
	BYTE* base = (BYTE*)GetModuleHandle(nullptr);
	std::string signature = hex_to_bytes(pattern);
	BYTE first = (BYTE)signature.at(0);
	/* Get module size */
	MODULEINFO info;
	GetModuleInformation(GetCurrentProcess(), (HMODULE)base, &info, sizeof(info));
	size_t memory_size = info.SizeOfImage;
	/* Scan for signature */
	BYTE* end = (base + memory_size) - signature.length();
	for (; base < end; ++base)
	{
		if (*base != first)
			continue;
		BYTE* bytes = base;
		BYTE* sig = (BYTE*)signature.c_str();
		for (; *sig; ++sig, ++bytes)
		{
			if (*sig == '?')
				continue;
			if (*bytes != *sig)
				goto end;
		}
		return (uintptr_t)base;
	end:;
	}
	return NULL;
}

bool isBadReadPtr(void* p)
{
	MEMORY_BASIC_INFORMATION mbi = { 0 };
	if (::VirtualQuery(p, &mbi, sizeof(mbi)))
	{
		DWORD mask = (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY);
		bool b = !(mbi.Protect & mask);
		if (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) b = true;
		return b;
	}
	return true;
}

uintptr_t calcAddS(uintptr_t ptr, std::vector<unsigned int> offsets, bool& valid)
{
	uintptr_t addr = ptr;
	for (unsigned int i = 0; i < offsets.size() - 1; ++i) {
		if (isBadReadPtr((void*)(addr + offsets[i]))) {
			valid = false;
			return 0;
		}
		addr = *(uintptr_t*)(addr + offsets[i]);
	}
	valid = true;
	return addr + offsets[offsets.size() - 1];
}

void patch(void* addr, std::vector<BYTE> bytes) {
	DWORD oldProtect;
	VirtualProtect(addr, bytes.size(), PAGE_EXECUTE_READWRITE, &oldProtect);
	for (int i = 0; i < bytes.size(); ++i) {
		*((BYTE*)addr + i) = bytes[i];
	}
	VirtualProtect(addr, bytes.size(), oldProtect, &oldProtect);
}

typedef void (__fastcall* SET_TRANSFORM)(sow::EntityTransform*, sow::Vec3f*);

SET_TRANSFORM function;
SET_TRANSFORM dFunction = (SET_TRANSFORM)0x14014554C;

sow::GameClient* game_client = nullptr;

uintptr_t PLAYER_BASE_ADDRESS = (uintptr_t)GetModuleHandle(NULL) + 0x26FFB70;
std::vector<unsigned int> PLAYER_OFFSETS = { 0x28, 0x468, 0x20, 0x8, 0x38, 0x90, 0x0 };

uintptr_t PLAYER_HEAD_BASE_ADDRESS = (uintptr_t)GetModuleHandle(NULL) + 0x02797808;
std::vector<unsigned int> PLAYER_HEAD_OFFSETS = { 0x20, 0x8, 0x28, 0xA0, 0xA0, 0x80, 0x110, 0xF8, 0x44 + 0xc0};

uintptr_t FOV_BASE_ADDRESS = (uintptr_t)GetModuleHandle(NULL) + 0x02671CC8;
std::vector<unsigned int> FOV_OFFSETS = { 0x30, 0x20, 0x18, 0x50, 0x10, 0x8, 0x1E4 };

extern "C" void StoreAllRegisters();
extern "C" void RestoreAllRegisters();


void __fastcall set_transform_detour(sow::EntityTransform* transform, sow::Vec3f* new_position) {
	StoreAllRegisters();
	if (transform == game_client->camera_owner->gameplay_camera->transform)
		return;
	RestoreAllRegisters();
	function(transform, new_position);

}

DWORD WINAPI MainThread(LPVOID param) {

	AllocConsole();
	(void)freopen("CONOUT$", "w", stdout);

	MH_Initialize();

	if (MH_CreateHook((void**)dFunction, &set_transform_detour, (void**)&function) != MH_OK) {
		MessageBoxA(NULL, "Unable to hook the camera!", "Error", MB_ICONERROR);
	}

	MH_EnableHook(MH_ALL_HOOKS);


	uintptr_t game_client_address = sigscan("48 8B 0D ? ? ? ? E8 ? ? ? ? C7 47");

	uintptr_t camera_clipping_func = sigscan(R"(48 8B C4 48 89 58 08 48 89 68 10 48 89 70 18 48 89 78 20 41 56 
												   48 83 EC 30 80 3D ? ? ? ? ? 41 8A E9 49 8B F8 4C 8B F2 48 8B 
												   D9 0F 84 ? ? ? ?")");

	if (camera_clipping_func != NULL) {
		patch((void*)camera_clipping_func, { 0xC3, 0x90, 0x90 });
	}

	sow::EntityTransform* player_head_transform = nullptr, *player_transform = nullptr;
	float* fov = nullptr;
	bool valid;
	for (;;) {
		valid = false;
		if (isBadReadPtr((void*)(game_client_address + 3)))
			continue;
		int32_t rip_relative = *(int32_t*)(game_client_address + 3);
		uintptr_t game_client_offset = game_client_address + 7 + rip_relative;
		if (isBadReadPtr((void*)game_client_offset))
			continue;
		game_client = *(sow::GameClient**)(game_client_offset);
		if (isBadReadPtr(game_client))
			continue;
		if (isBadReadPtr(game_client->camera_owner))
			continue;
		if (isBadReadPtr(game_client->camera_owner->gameplay_camera))
			continue;
		if (isBadReadPtr(game_client->camera_owner->gameplay_camera->transform))
			continue;


		if (!isBadReadPtr((void*)PLAYER_HEAD_BASE_ADDRESS)) {
			player_head_transform = (sow::EntityTransform*)calcAddS(*(uintptr_t*)(PLAYER_HEAD_BASE_ADDRESS), PLAYER_HEAD_OFFSETS, valid);
		}
		if (!valid) player_head_transform = nullptr;

		if (!isBadReadPtr((void*)PLAYER_BASE_ADDRESS)) {
			player_transform = (sow::EntityTransform*)calcAddS(*(uintptr_t*)(PLAYER_BASE_ADDRESS), PLAYER_OFFSETS, valid);
		}
		if (!valid) player_transform = nullptr;

		//if (!isBadReadPtr((void*)FOV_BASE_ADDRESS)) {
		//	fov = (float*)calcAddS(*(uintptr_t*)(FOV_BASE_ADDRESS), FOV_OFFSETS, valid);
		//}
		//if (!valid) fov = nullptr;


		if (player_head_transform != nullptr && player_transform != nullptr) {
			auto* ct = game_client->camera_owner->gameplay_camera->transform;
			sow::Vec3f forward = { 0, 0, 1 };

			// head rotation is not correctly set... this is probably not the actual "head" but rather a pivot inside of the player...
			sow::Vec3f direction = player_transform->rotation.rotateVector(forward);

			ct->position = player_head_transform->position + (direction * 3.f);
			ct->position.y += 3.f;


			if (!input('W') && !input('A') && !input('S') && !input('D')) {
				while (player_transform->rotation.signedAngleOnAxis(ct->rotation, 0.f, 1.f, 0.f) * 180 / 3.14159 > 65) {
					player_transform->rotation = player_transform->rotation.rotate(0.f, 1.f, 0.f, -0.001);
				}
				while (player_transform->rotation.signedAngleOnAxis(ct->rotation, 0.f, 1.f, 0.f) * 180 / 3.14159 < -65) {
					player_transform->rotation = player_transform->rotation.rotate(0.f, 1.f, 0.f, 0.001);
				}
			}

		}

		// Below actual fps mod, so if it doesnt work for some reason the fps still operates well

		if (isBadReadPtr((void*)(0x142A34C68)))
			continue;
		if (isBadReadPtr((void*)*(uintptr_t*)(0x142A34C68)))
			continue;
		uintptr_t fov_calc = **(uintptr_t**)(0x142A34C68);
		if (isBadReadPtr((void*)(fov_calc + 0x2f90)))
			continue;
		fov_calc = *(uintptr_t*)(fov_calc + 0x2f90);
		if (isBadReadPtr((void*)(fov_calc + 0x274)))
			continue;
		fov = (float*)(fov_calc + 0x274);
		
		if (input(VK_NUMPAD0)) {
			std::cout << fov << "\n";
			Sleep(100);
		}

		*fov = 2.5f;

	}

	MH_DisableHook(MH_ALL_HOOKS);
	FreeLibraryAndExitThread((HMODULE)param, 0);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpvReserved)
{
	switch (fdwReason) {
	case DLL_PROCESS_ATTACH:
		CreateThread(NULL, NULL, MainThread, hModule, NULL, NULL);
		break;
	default:
		break;
	}
	return TRUE;
}

