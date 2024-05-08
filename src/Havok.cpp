#include "Havok.h"

#include "Offsets.h"

RE::hkMemoryRouter& hkGetMemoryRouter(){
	return *reinterpret_cast<RE::hkMemoryRouter*>(reinterpret_cast<uintptr_t>(TlsGetValue(*g_dwTlsIndex)));
}
