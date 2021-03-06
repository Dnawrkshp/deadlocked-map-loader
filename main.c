/***************************************************
 * FILENAME :		main.c
 * 
 * DESCRIPTION :
 * 		Manages and applies all Deadlocked patches.
 * 
 * NOTES :
 * 		Each offset is determined per app id.
 * 		This is to ensure compatibility between versions of Deadlocked/Gladiator.
 * 		
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */

#include "common.h"
#include <tamtypes.h>
#include <sifrpc.h>
#include "loadfile.h"
#include <sifcmd.h>
#include <iopheap.h>
#include <iopcontrol.h>
#include <sbv_patches.h>
#include <slib.h>
#include <smod.h>

#define NEWLIB_PORT_AWARE
#include <io_common.h>

#include "rpc.h"

// We put this outside the bounds of the binary in case we update the binary
// If we didn't do this, it'd try to load the IRX modules twice
// Maybe in the future we can check if the modules are loaded before try to load them
#define HAS_LOADED_MODULES			(*(int*)0x000CE000)
#define LEVEL_ID					(*(u8*)0x00212694)

int hasPatchedSif = 0;
u8 membuffer[1024];

// wad file descriptor and file size
int level_fd = -1;
int level_loading_id = -1;
int level_file_size = 0;

int usb_mass_id = 0;
int usbserv_id = 0;

// paths for level specific wad and toc
char * fWad = "dl/level%d.wad";
char * fToc = "dl/level%d.toc";
char * fBg = "dl/level%d.bg";
char * fMap = "dl/level%d.map";

// 
extern int _SifLoadModuleBuffer(void *ptr, int arg_len, const char *args, int *modres);

//
void Hook(void);

//------------------------------------------------------------------------------
void patch(void * funcPtr, u32 addr)
{
	// 
	*(u32*)funcPtr = 0x08000000 | (addr / 4);
	*((u32*)funcPtr + 1) = 0;
}

//------------------------------------------------------------------------------
void patchSifRefs(void)
{
	// 
	patch(&SifInitRpc, 0x00129D90);
	patch(&SifExitRpc, 0x00129F30);
	patch(&SifRpcGetOtherData, 0x0012A270);
	patch(&SifBindRpc, 0x0012A538);
	patch(&SifCallRpc, 0x0012A718);
	patch(&SifCheckStatRpc, 0x0012A918);
	patch(&SifSetRpcQueue, 0x0012A958);
	patch(&SifInitIopHeap, 0x0012C1B8);
	patch(&SifAllocIopHeap, 0x0012C240);
	patch(&SifFreeIopHeap, 0x0012C3A8);
	patch(&SifIopReset, 0x0012CC30);
	patch(&SifIopSync, 0x0012CDB0);

	patch(&_SifLoadModuleBuffer, 0x0012CBC0);

	hasPatchedSif = 1;
	printf("map loader initialized\n");
}

//------------------------------------------------------------------------------
void unload(void)
{
	if (usb_mass_id > 0)
		SifUnloadModule(usb_mass_id);

	if (usbserv_id > 0)
		SifUnloadModule(usbserv_id);

	HAS_LOADED_MODULES = 0;
}

//------------------------------------------------------------------------------
void loadModules(void)
{
	if (HAS_LOADED_MODULES)
		return;

	// Patch 
	patchSifRefs();

	//
	SifInitRpc(0);

	// Load modules
	printf("Loading MASS: %d\n", usb_mass_id = SifExecModuleBuffer(&usb_mass_irx, size_usb_mass_irx, 0, NULL, NULL));
	printf("Loading USBSERV: %d\n", usbserv_id = SifExecModuleBuffer(&usbserv_irx, size_usbserv_irx, 0, NULL, NULL));

	// 
	printf("rpcUSBInit: %d\n", rpcUSBInit());

	HAS_LOADED_MODULES = 1;
}

//------------------------------------------------------------------------------
u64 loadHookFunc(u64 a0, u64 a1)
{
	// Load our usb modules
	loadModules();

	// Loads sound driver
	return ((u64 (*)(u64, u64))0x001518C8)(a0, a1);
}

//--------------------------------------------------------------
int readUsb(char *filename, u8 * buf, int size)
{
	int r, fd, done, fileLen;
	if (size < 0)
		return 0;
	
	done = 0;

	rpcUSBopen(filename, FIO_O_RDONLY);
	rpcUSBSync(0, NULL, &fd);
	
	if (fd < 0)
	{
		printf("error opening file (%s): %d\n", filename, fd);
		return 0;									
	}

	// Get length of file
	rpcUSBseek(fd, 0, SEEK_END);
	rpcUSBSync(0, NULL, &fileLen);

	if (fileLen <= 0)
	{
		printf("error seeking file (%s): %d\n", filename, fileLen);
		return fileLen;
	}

	// Get length of file
	rpcUSBseek(fd, 0, SEEK_SET);
	rpcUSBSync(0, NULL, NULL);

	printf("%s is %d byte long.\n", filename, fileLen);

	// Clamp
	if (size > fileLen)
		size = fileLen;

	if (rpcUSBread(fd, buf, size) != 0)
	{
		printf("error reading from file.\n");
		goto finish;
	}
	rpcUSBSync(0, NULL, &r);

finish:
	rpcUSBclose(fd);
	rpcUSBSync(0, NULL, &r);		
				
	return r;
}

//--------------------------------------------------------------
int getLevelSizeUsb(int levelId)
{
	int fd = -1;

	// Generate wad filename
	sprintf(membuffer, fWad, levelId);

	// open wad file
	rpcUSBopen(membuffer, FIO_O_RDONLY);
	rpcUSBSync(0, NULL, &fd);
	
	// Ensure wad successfully opened
	if (fd < 0)
	{
		printf("error opening file (%s): %d\n", membuffer, fd);
		return 0;									
	}

	// Get length of file
	rpcUSBseek(fd, 0, SEEK_END);
	rpcUSBSync(0, NULL, &level_file_size);

	// Check the file has a valid size
	if (level_file_size <= 0)
	{
		printf("error seeking file (%s): %d\n", membuffer, level_file_size);
		rpcUSBclose(fd);
		rpcUSBSync(0, NULL, NULL);
		fd = -1;
		return 0;
	}

	// Close toc
	rpcUSBclose(fd);
	rpcUSBSync(0, NULL, NULL);

	return level_file_size;
}

//--------------------------------------------------------------
int readLevelMapUsb(int levelId, u8 * buf, int size)
{
	int r, fd, fSize;

	// Generate toc filename
	sprintf(membuffer, fMap, levelId);

	// Open
	rpcUSBopen(membuffer, FIO_O_RDONLY);
	rpcUSBSync(0, NULL, &fd);

	// Ensure the toc was opened successfully
	if (fd < 0)
	{
		printf("error opening file (%s): %d\n", membuffer, fd);
		return 0;	
	}
	
	// Get length of file
	rpcUSBseek(fd, 0, SEEK_END);
	rpcUSBSync(0, NULL, &fSize);

	// Check the file has a valid size
	if (fSize <= 0)
	{
		printf("error seeking file (%s): %d\n", membuffer, fSize);
		rpcUSBclose(fd);
		rpcUSBSync(0, NULL, NULL);
		return 0;
	}

	// Go back to start of file
	rpcUSBseek(fd, 0, SEEK_SET);
	rpcUSBSync(0, NULL, NULL);

	// clamp size to be no larger than file size
	if (size > fSize)
		size = fSize;

	// Read map
	if (rpcUSBread(fd, buf, size) != 0)
	{
		printf("error reading from file.\n");
		rpcUSBclose(fd);
		rpcUSBSync(0, NULL, NULL);
		return 0;
	}
	rpcUSBSync(0, NULL, &r);

	// Close toc
	rpcUSBclose(fd);
	rpcUSBSync(0, NULL, NULL);

	return 1;
}

//--------------------------------------------------------------
int readLevelBgUsb(int levelId, u8 * buf, int size)
{
	int r, fd;

	// Ensure a wad isn't already open
	if (level_fd >= 0)
	{
		printf("openLevelUsb(%d) called but a file is already open (%d)!", levelId, level_fd);
		return 0;
	}

	// Generate toc filename
	sprintf(membuffer, fBg, levelId);

	// Open
	rpcUSBopen(membuffer, FIO_O_RDONLY);
	rpcUSBSync(0, NULL, &fd);

	// Ensure the toc was opened successfully
	if (fd < 0)
	{
		printf("error opening file (%s): %d\n", membuffer, fd);
		return 0;	
	}

	// Read bg
	if (rpcUSBread(fd, buf, size) != 0)
	{
		printf("error reading from file.\n");
		rpcUSBclose(fd);
		rpcUSBSync(0, NULL, NULL);
		return 0;
	}
	rpcUSBSync(0, NULL, &r);

	// Close toc
	rpcUSBclose(fd);
	rpcUSBSync(0, NULL, NULL);

	return 1;
}

//--------------------------------------------------------------
int readLevelTocUsb(int levelId, void * tocDest)
{
	int r, fdToc;

	// Ensure a wad isn't already open
	if (level_fd >= 0)
	{
		printf("openLevelUsb(%d) called but a file is already open (%d)!", levelId, level_fd);
		return 0;
	}

	// Generate toc filename
	sprintf(membuffer, fToc, levelId);

	// Open
	rpcUSBopen(membuffer, FIO_O_RDONLY);
	rpcUSBSync(0, NULL, &fdToc);

	// Ensure the toc was opened successfully
	if (fdToc < 0)
	{
		printf("error opening file (%s): %d\n", membuffer, fdToc);
		return 0;	
	}

	// Read toc
	if (rpcUSBread(fdToc, tocDest, 0x1000) != 0)
	{
		printf("error reading from file.\n");
		rpcUSBclose(fdToc);
		rpcUSBSync(0, NULL, NULL);
		return 0;
	}
	rpcUSBSync(0, NULL, &r);

	// Close toc
	rpcUSBclose(fdToc);
	rpcUSBSync(0, NULL, NULL);

	return 1;
}

//--------------------------------------------------------------
int openLevelUsb(int levelId)
{
	// Ensure a wad isn't already open
	if (level_fd >= 0)
	{
		printf("openLevelUsb(%d) called but a file is already open (%d)!", levelId, level_fd);
		return 0;
	}

	// Generate wad filename
	sprintf(membuffer, fWad, levelId);

	// open wad file
	rpcUSBopen(membuffer, FIO_O_RDONLY);
	rpcUSBSync(0, NULL, &level_fd);
	
	// Ensure wad successfully opened
	if (level_fd < 0)
	{
		printf("error opening file (%s): %d\n", membuffer, level_fd);
		return 0;									
	}

	// Get length of file
	rpcUSBseek(level_fd, 0, SEEK_END);
	rpcUSBSync(0, NULL, &level_file_size);

	// Check the file has a valid size
	if (level_file_size <= 0)
	{
		printf("error seeking file (%s): %d\n", membuffer, level_file_size);
		rpcUSBclose(level_fd);
		rpcUSBSync(0, NULL, NULL);
		level_fd = -1;
		level_loading_id = -1;
		return 0;
	}

	// Go back to start of file
	// The read will be called later
	rpcUSBseek(level_fd, 0, SEEK_SET);
	rpcUSBSync(0, NULL, NULL);

	printf("%s is %d byte long.\n", membuffer, level_file_size);
	
	return level_file_size;
}

//--------------------------------------------------------------
int readLevelUsb(u8 * buf)
{
	// Ensure the wad is open
	if (level_fd < 0 || level_file_size <= 0)
	{
		printf("error opening file: %d\n", level_fd);
		return 0;									
	}

	// Try to read from usb
	if (rpcUSBread(level_fd, buf, level_file_size) != 0)
	{
		printf("error reading from file.\n");
		rpcUSBclose(level_fd);
		rpcUSBSync(0, NULL, NULL);
		level_fd = -1;
		return 0;
	}
				
	return 1;
}

//------------------------------------------------------------------------------
void hookedLoad(u64 arg0, void * dest, u32 sectorStart, u32 sectorSize, u64 arg4, u64 arg5, u64 arg6)
{
	int levelId = LEVEL_ID;

	void (*cdvdLoad)(u64, void*, u32, u32, u64, u64, u64) = (void (*)(u64, void*, u32, u32, u64, u64, u64))0x00163840;

	// Check if loading MP map
	if (levelId == level_loading_id && level_loading_id > 0x28 && HAS_LOADED_MODULES && hasPatchedSif)
	{
		int fSize = openLevelUsb(levelId);
		if (fSize)
		{
			if (readLevelUsb(dest) > 0)
				return;
		}
	}

	// Default to cdvd load if the usb load failed
	cdvdLoad(arg0, dest, sectorStart, sectorSize, arg4, arg5, arg6);
}

//------------------------------------------------------------------------------
u32 hookedCheck(void)
{
	u32 (*check)(void) = (u32 (*)(void))0x00163928;
	int r, cmd;

	// If the wad is not open then we're loading from cdvd
	if (level_fd < 0)
		return check();

	// Otherwise check to see if we've finished loading the data from USB
	if (rpcUSBSyncNB(0, &cmd, &r))
	{
		// If the command is USBREAD close and return
		if (cmd == 0x04)
		{
			printf("finished reading %d bytes from USB\n", r);
			rpcUSBclose(level_fd);
			rpcUSBSync(0, NULL, NULL);
			level_fd = -1;
			return check();
		}
	}

	// Set bg color to red
	*((vu32*)0x120000e0) = 0x1010B4;

	// Not sure if this is necessary but it doesn't hurt to call the game's native load check
	check();

	// Indicate that we haven't finished loading
	return 1;
}

//------------------------------------------------------------------------------
void hookedLoadingScreen(u64 a0, void * a1, u64 a2, u64 a3, u64 t0, u64 t1, u64 t2)
{
	int levelId = LEVEL_ID;

	if (readLevelBgUsb(levelId, a1, a3 * 0x800) > 0)
	{

	}
	else
	{
		((void (*)(u64, void *, u64, u64, u64, u64, u64))0x00163808)(a0, a1, a2, a3, t0, t1, t2);
	}
}

//------------------------------------------------------------------------------
void hookedGetTable(u32 startSector, u32 sectorCount, u8 * dest, u32 levelId)
{
	// reset
	level_loading_id = -1;

	// Check if loading MP map
	if (levelId >= 0x28 && HAS_LOADED_MODULES && hasPatchedSif)
	{
		if (readLevelTocUsb(levelId, dest))
		{
			level_loading_id = levelId;
			return;
		}
	}
	
	// load table
	((void (*)(u32,u32,void*))0x00159A00)(startSector, sectorCount, dest);
}

//------------------------------------------------------------------------------
void hookedGetMap(u64 a0, void * dest, u32 startSector, u32 sectorCount, u64 t0, u64 t1, u64 t2)
{
	int levelId = LEVEL_ID;

	// Check if loading MP map
	if (levelId >= 0x28 && HAS_LOADED_MODULES && hasPatchedSif)
	{
		// We hardcode the size because that's the max that deadlocked can hold
		if (readLevelMapUsb(levelId, dest, 0x27400))
			return;
	}

	((void (*)(u64, void*,u32,u32,u64,u64,u64))0x00163808)(a0, dest, startSector, sectorCount, t0, t1, t2);
}

//------------------------------------------------------------------------------
void hookedGetAudio(u64 a0, void * dest, u32 startSector, u32 sectorCount, u64 t0, u64 t1, u64 t2)
{
	((void (*)(u64, void*,u32,u32,u64,u64,u64))0x00163808)(a0, dest, startSector, sectorCount, t0, t1, t2);
}

//------------------------------------------------------------------------------
u64 hookedLoadCdvd(u64 a0, u64 a1, u64 a2, u64 a3, u64 t0, u64 t1, u64 t2)
{
	u64 result = ((u64 (*)(u64,u64,u64,u64,u64,u64,u64))0x00163840)(a0, a1, a2, a3, t0, t1, t2);

	// We try and hook here to just to make sure that after tha game loads
	// We can still load our custom minimap
	Hook();

	return result;
}

//------------------------------------------------------------------------------
void Hook(void)
{
	u32 * hookloadAddr = (u32*)0x005CFB48;
	u32 * hookcheckAddr = (u32*)0x005CF9B0;
	u32 * loadModulesAddr = (u32*)0x00161364;
	u32 * hookloadingScreenAddr = (u32*)0x00705554;
	u32 * getTableAddr = (u32*)0x00159B20;
	u32 * getMapAddr = (u32*)0x00557580;
	u32 * getAudioAddr = (u32*)0x0053F970;
	u32 * loadCdvdAddr = (u32*)0x00163814;

	// For some reason we can't load the IRX modules whenever we want
	// So here we hook into when the game uses rpc calls
	// This triggers when entering the online profile select, leaving profile select, and logging out.
	if (*loadModulesAddr == 0x0C054632)
		*loadModulesAddr = 0x0C000000 | ((u32)(&loadHookFunc) / 4);

	if (*hookloadAddr == 0x0C058E10)
	{
		*getTableAddr = 0x0C000000 | ((u32)(&hookedGetTable) / 4);
		*hookloadAddr = 0x0C000000 | ((u32)(&hookedLoad) / 4);
		*hookcheckAddr = 0x0C000000 | ((u32)(&hookedCheck) / 4);
		*hookloadingScreenAddr = 0x0C000000 | ((u32)&hookedLoadingScreen / 4);
		*loadCdvdAddr = 0x0C000000 | ((u32)&hookedLoadCdvd / 4);
	}

	// These get hooked after the map loads but before the game starts
	if (*getMapAddr == 0x0C058E02)
		*getMapAddr = 0x0C000000 | ((u32)(&hookedGetMap) / 4);
}

//------------------------------------------------------------------------------
int main (void)
{
	// 
	if (!hasPatchedSif)
		patchSifRefs();

	// Install level loading hooks
	Hook();

	return 0;
}
