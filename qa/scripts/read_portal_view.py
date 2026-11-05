# Reads the local player's view origin/angles from the running Portal (read-only, ReadProcessMemory).
# Offsets from LESSONS.md (Benja), verified in client.dll TimeDateStamp 0x68362D89 (Portal 19017868).
# Prints: x y z pitch yaw roll
import ctypes
import ctypes.wintypes as wt
import struct
import sys

VIEW_OFFSET = 0x0050F484  # MainViewOrigin (12 bytes) followed by MainViewAngles (12 bytes)

k32 = ctypes.WinDLL("kernel32", use_last_error=True)
psapi = ctypes.WinDLL("psapi")


class PROCESSENTRY32(ctypes.Structure):
    _fields_ = [("dwSize", wt.DWORD), ("cntUsage", wt.DWORD), ("th32ProcessID", wt.DWORD),
                ("th32DefaultHeapID", ctypes.c_size_t), ("th32ModuleID", wt.DWORD), ("cntThreads", wt.DWORD),
                ("th32ParentProcessID", wt.DWORD), ("pcPriClassBase", ctypes.c_long), ("dwFlags", wt.DWORD),
                ("szExeFile", ctypes.c_char * 260)]


def find_pid():
    snap = k32.CreateToolhelp32Snapshot(2, 0)
    entry = PROCESSENTRY32()
    entry.dwSize = ctypes.sizeof(entry)
    ok = k32.Process32First(snap, ctypes.byref(entry))
    while ok:
        if entry.szExeFile.lower() == b"hl2.exe":
            return entry.th32ProcessID
        ok = k32.Process32Next(snap, ctypes.byref(entry))
    return None


pid = find_pid()
if not pid:
    sys.exit("hl2.exe no esta corriendo")
handle = k32.OpenProcess(0x0410, False, pid)  # PROCESS_QUERY_INFORMATION | PROCESS_VM_READ
modules = (wt.HMODULE * 1024)()
needed = wt.DWORD()
psapi.EnumProcessModulesEx(handle, modules, ctypes.sizeof(modules), ctypes.byref(needed), 3)
base = None
for m in modules[: needed.value // ctypes.sizeof(wt.HMODULE)]:
    name = ctypes.create_unicode_buffer(260)
    psapi.GetModuleBaseNameW(handle, wt.HMODULE(m), name, 260)
    if name.value.lower() == "client.dll":
        base = m
if not base:
    sys.exit("client.dll no cargado")
buf = (ctypes.c_char * 24)()
read = ctypes.c_size_t()
k32.ReadProcessMemory(handle, ctypes.c_void_p(base + VIEW_OFFSET), buf, 24, ctypes.byref(read))
print("%.1f %.1f %.1f %.1f %.1f %.1f" % struct.unpack("<6f", bytes(buf)))
