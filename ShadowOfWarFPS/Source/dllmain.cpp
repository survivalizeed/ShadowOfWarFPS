#include "pch.h"



namespace sow
{
	struct Vec3f {
		float x, y, z;
	};

	struct Quaternion {
		float x, y, z, w;

		Vec3f to_direction() {
			Vec3f tmp{};
			tmp.x = 2 * (x * z - w * y);
			tmp.y = 2 * (y * z + w * x);
			tmp.z = 1 - 2 * (x * x + y * y);
			return tmp;
		}

		Vec3f to_euler() {
			Vec3f euler;

			float sinr_cosp = 2 * (w * x + y * z);
			float cosr_cosp = 1 - 2 * (x * x + y * y);
			euler.x = std::atan2(sinr_cosp, cosr_cosp);

			float sinp = 2 * (w * y - z * x);
			if (std::abs(sinp) >= 1)
				euler.y = std::copysign(3.14159 / 2, sinp);
			else
				euler.y = std::asin(sinp);

			float siny_cosp = 2 * (w * z + x * y);
			float cosy_cosp = 1 - 2 * (y * y + z * z);
			euler.z = std::atan2(siny_cosp, cosy_cosp);

			return euler;
		}

		static Quaternion from_euler(const Vec3f& euler) {
			Quaternion q;
			float cy = std::cos(euler.z * 0.5f);
			float sy = std::sin(euler.z * 0.5f);
			float cp = std::cos(euler.y * 0.5f);
			float sp = std::sin(euler.y * 0.5f);
			float cr = std::cos(euler.x * 0.5f);
			float sr = std::sin(euler.x * 0.5f);
			q.w = cr * cp * cy + sr * sp * sy;
			q.x = sr * cp * cy - cr * sp * sy;
			q.y = cr * sp * cy + sr * cp * sy;
			q.z = cr * cp * sy - sr * sp * cy;

			return q;
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

inline bool isBadReadPtr(void* p)
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

inline uintptr_t calcAddS(uintptr_t ptr, std::vector<unsigned int> offsets, bool& valid)
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


typedef void (__fastcall* SET_TRANSFORM)(sow::EntityTransform*, sow::Vec3f*);

SET_TRANSFORM function;
SET_TRANSFORM dFunction = (SET_TRANSFORM)0x14014554C;

sow::GameClient* game_client = nullptr;

uintptr_t PLAYER_HEAD_BASE_ADDRESS = (uintptr_t)GetModuleHandle(NULL) + 0x02797808;
std::vector<unsigned int> PLAYER_HEAD_OFFSETS = { 0x20, 0x8, 0x28, 0xA0, 0xA0, 0x80, 0x110, 0xF8, 0x44 + 0xc0};

uintptr_t FOV_BASE_ADDRESS = (uintptr_t)GetModuleHandle(NULL) + 0x02671CC8;
std::vector<unsigned int> FOV_OFFSETS = { 0x30, 0x20, 0x18, 0x50, 0x10, 0x8, 0x1E4 };

extern "C" void StoreAllRegisters();
extern "C" void RestoreAllRegisters();

bool enabled = true;

void __fastcall set_transform_detour(sow::EntityTransform* transform, sow::Vec3f* new_position) {
	StoreAllRegisters();
	if (transform == game_client->camera_owner->gameplay_camera->transform && enabled)
		return;
	RestoreAllRegisters();
	function(transform, new_position);

}

DWORD WINAPI MainThread(LPVOID param) {

	AllocConsole();
	(void)freopen("CONOUT$", "w", stdout);

	MH_Initialize();

	if (MH_CreateHook((void**)dFunction, &set_transform_detour, (void**)&function) != MH_OK) {
		MessageBoxA(NULL, "Error", "Error", MB_ICONERROR);
	}

	MH_EnableHook(MH_ALL_HOOKS);


	uintptr_t game_client_address = sigscan("48 8B 0D ? ? ? ? E8 ? ? ? ? C7 47");


	sow::EntityTransform* player_transform = nullptr;
	bool valid;
	for (;;) {
		if (GetAsyncKeyState(VK_NUMPAD0) & 0x8000) {
			enabled = !enabled;
			Sleep(500);
		}
		if (!enabled)
			continue;
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
			player_transform = (sow::EntityTransform*)calcAddS(*(uintptr_t*)(PLAYER_HEAD_BASE_ADDRESS), PLAYER_HEAD_OFFSETS, valid);
		}

		if (valid) {
			auto* ct = game_client->camera_owner->gameplay_camera->transform;
			ct->position.x = player_transform->position.x;
			ct->position.y = player_transform->position.y;
			ct->position.z = player_transform->position.z;

			sow::Vec3f player_rotation = player_transform->rotation.to_euler();
			sow::Vec3f camera_rotation = ct->rotation.to_euler();
			camera_rotation.z = player_rotation.z;
			camera_rotation.y = player_rotation.y;
			ct->rotation.from_euler(camera_rotation);
		}
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

