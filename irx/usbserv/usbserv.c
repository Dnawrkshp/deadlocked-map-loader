
#include <loadcore.h>
#include <stdio.h>
#include <sysclib.h>
#include <sysmem.h>
#include <thbase.h>
#include <thevent.h>
#include <intrman.h>
#include <sifcmd.h>
#include <sifman.h>
#include <thsemap.h>
#include <errno.h>
#include <io_common.h>
#include <ioman.h>
#include <intrman.h>



#define USBSERV_BUFSIZE (1024 * 16)
#define USBSERV_RPC_BUFSIZE (USBSERV_BUFSIZE + 8)

#define MODNAME "usbserv"
IRX_ID(MODNAME, 1, 0);

int usbserv_io_sema; 	// IO semaphore

static int rpc_tidS_0A10;

static SifRpcDataQueue_t rpc_qdS_0A10 __attribute__((aligned(64)));
static SifRpcServerData_t rpc_sdS_0A10 __attribute__((aligned(64)));

static u8 usbserv_rpc_buf[USBSERV_RPC_BUFSIZE] __attribute__((aligned(64)));

static struct {		
	int rpc_func_ret;
	int usbserv_version;
} rpc_stat __attribute__((aligned(64)));

int (*rpc_func)(void);

void dummy(void) {}
int  rpcUSBopen(void);
int _rpcUSBopen(void *rpc_buf);
int  rpcUSBwrite(void);
int _rpcUSBwrite(void *rpc_buf);
int  rpcUSBread(void);
int _rpcUSBread(void *rpc_buf);
int  rpcUSBseek(void);
int _rpcUSBseek(void *rpc_buf);
int  rpcUSBclose(void);
int _rpcUSBclose(void *rpc_buf);

// rpc command handling array
void *rpc_funcs_array[16] = {
    (void *)dummy,
    (void *)rpcUSBopen,
    (void *)rpcUSBwrite,
    (void *)rpcUSBclose,
    (void *)rpcUSBread,
    (void *)rpcUSBseek,
    (void *)dummy,
    (void *)dummy,
    (void *)dummy,
    (void *)dummy,
    (void *)dummy,
    (void *)dummy,
    (void *)dummy,
    (void *)dummy,
    (void *)dummy,
    (void *)dummy
};

typedef struct g_openParam {  // size = 1024
	int flags;				  // 
	char filename[1024];	  // 0
	u8 pad[12];
} g_openParam_t;

typedef struct g_writeParam { // size =
	int fd;					  // 0
	u8 buf[USBSERV_BUFSIZE];		 	  // 4
	int size;				  // 
	u8 pad[8];
} g_writeParam_t;

typedef struct g_closeParam { // size = 16
	int fd;	  				  // 0
	u8 pad[12];
} g_closeParam_t;

typedef struct g_readParam { // size =
	int fd;					  // 0
	void * buf;
	int size;				  // 
	u8 pad[4];
} g_readParam_t;

typedef struct g_seekParam { // size =
	int fd;					  // 0
	int offset;
	int whence;
	u8 pad[4];
} g_seekParam_t;

char filepath[1056];

//--------------------------------------------------------------
void *cb_rpc_S_0A10(u32 fno, void *buf, int size)
{
	// Rpc Callback function
	
	if (fno >= 16)
		return (void *)&rpc_stat;

	// Get function pointer
	rpc_func = (void *)rpc_funcs_array[fno];
	
	// Call needed rpc func
	rpc_stat.rpc_func_ret = rpc_func();
	
	return (void *)&rpc_stat;
}

//--------------------------------------------------------------
void thread_rpc_S_0A10(void* arg)
{
	if (!sceSifCheckInit())
		sceSifInit();

	sceSifInitRpc(0);
	sceSifSetRpcQueue(&rpc_qdS_0A10, GetThreadId());
	sceSifRegisterRpc(&rpc_sdS_0A10, 0x80000a10, (void *)cb_rpc_S_0A10, &usbserv_rpc_buf, NULL, NULL, &rpc_qdS_0A10);
	sceSifRpcLoop(&rpc_qdS_0A10);
}

//-------------------------------------------------------------- 
int start_RPC_server(void)
{
	iop_thread_t thread_param;
	register int thread_id;	
			
 	thread_param.attr = TH_C;
 	thread_param.thread = (void *)thread_rpc_S_0A10;
 	thread_param.priority = 0x68;
 	thread_param.stacksize = 0x1000;
 	thread_param.option = 0;
			
	thread_id = CreateThread(&thread_param);
	rpc_tidS_0A10 = thread_id;
		
	StartThread(thread_id, 0);
	
	return 0;
}

//-------------------------------------------------------------- 
int rpcUSBopen(void)
{
	return _rpcUSBopen(&usbserv_rpc_buf);
}

//-------------------------------------------------------------- 
int _rpcUSBopen(void *rpc_buf)
{
	int fd;
	g_openParam_t *eP = (g_openParam_t *)rpc_buf;	

	WaitSema(usbserv_io_sema);
	
	sprintf(filepath, "mass:%s", eP->filename);
	fd = open(filepath, eP->flags);	
	printf("opening %s,%d -> %d\n", filepath, eP->flags, fd);
	
	SignalSema(usbserv_io_sema);
		
	return fd;
}

//-------------------------------------------------------------- 
int rpcUSBwrite(void)
{
	return _rpcUSBwrite(&usbserv_rpc_buf);
}

//-------------------------------------------------------------- 
int _rpcUSBwrite(void *rpc_buf)
{
	int r;
	g_writeParam_t *eP = (g_writeParam_t *)rpc_buf;	
			
	WaitSema(usbserv_io_sema);
	
	r = write(eP->fd, eP->buf, eP->size);
	printf("writing %d,%s,%d -> %d\n", eP->fd, eP->buf, eP->size, r);
	
	SignalSema(usbserv_io_sema);
	
	return r;
}

//-------------------------------------------------------------- 
int rpcUSBclose(void)
{
	return _rpcUSBclose(&usbserv_rpc_buf);
}

//-------------------------------------------------------------- 
int _rpcUSBclose(void *rpc_buf)
{
	int r;
	g_closeParam_t *eP = (g_closeParam_t *)rpc_buf;	

	WaitSema(usbserv_io_sema);
					
	r = close(eP->fd);
	printf("close %d -> %d\n", eP->fd, r);
	
	SignalSema(usbserv_io_sema);
	
	return r;
}

//-------------------------------------------------------------- 
int rpcUSBread(void)
{
	return _rpcUSBread(&usbserv_rpc_buf);
}

//-------------------------------------------------------------- 
int _rpcUSBread(void *rpc_buf)
{
	int r = 0, rPos = 0, blockSize;
	g_readParam_t *eP = (g_readParam_t *)rpc_buf;
	SifDmaTransfer_t dmaStruct;
	void * eedata = eP->buf;
	int intStatus, status = -1;
	int len = eP->size;
	int fd = eP->fd;
			
	WaitSema(usbserv_io_sema);
	
	while (rPos < len)
	{
		// Clamp read size by buffer size
		blockSize = len - rPos;
		if (blockSize > USBSERV_BUFSIZE)
			blockSize = USBSERV_BUFSIZE;

		r = read(fd, rpc_buf, blockSize);
		printf("READ %d: POS: %d, BLOCK: %d -> %d\n", len, rPos, blockSize, r);

		// Error
		if (r <= 0)
			break;

		// Send back to EE
		dmaStruct.src = (void *)rpc_buf;
		dmaStruct.dest = eedata;
		dmaStruct.size = r;
		dmaStruct.attr = 0;

		CpuSuspendIntr(&intStatus);
		status = sceSifSetDma(&dmaStruct, 1);
		CpuResumeIntr(intStatus);

		// Increment
		eedata += r;
		rPos += r;

		// 
		if (r != blockSize)
			break;
	}

	// Wait
	while (status >= 0 && sceSifDmaStat(status) >= 0)
		DelayThread(100);

	SignalSema(usbserv_io_sema);
	
	return rPos;
}

//-------------------------------------------------------------- 
int rpcUSBseek(void)
{
	return _rpcUSBseek(&usbserv_rpc_buf);
}

//-------------------------------------------------------------- 
int _rpcUSBseek(void *rpc_buf)
{
	int r;
	g_seekParam_t *eP = (g_seekParam_t *)rpc_buf;	
			
	WaitSema(usbserv_io_sema);
	
	r = lseek(eP->fd, eP->offset, eP->whence);
	printf("seeking %d,%d,%d -> %d\n", eP->fd, eP->offset, eP->whence, r);
	
	SignalSema(usbserv_io_sema);
	
	return r;
}

//-------------------------------------------------------------------------
int _start(int argc, char** argv)
{			
	iop_sema_t smp;
				
	SifInitRpc(0);
			
	// Starting usbserv Remote Procedure Call server	
	start_RPC_server();
	
	smp.attr = 1;
	smp.initial = 1;
	smp.max = 1;
	smp.option = 0;
	usbserv_io_sema = CreateSema(&smp);	
						
	return MODULE_RESIDENT_END;
}
