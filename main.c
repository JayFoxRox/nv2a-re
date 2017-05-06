/*
 * Xbox shader server (to be ran on an Xbox, compilable with NXDK)
 *
 * Waits for TCP requests on server_port (default 9269). Requests must be 136 instructions (1x 16 bytes each), followed by 192 shader constants (4x 4 floats each)
 * The response will be 192 modified shader constants (4x 4 floats each)
 *
 * (c) 2017 Jannik Vogel
 *
 */

//FIXME: Handle larger requests by using the extra data as vertex input

//#include <cstdint>

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "lwip/api.h"
#include "lwip/arch.h"
#include "lwip/debug.h"
#include "lwip/dhcp.h"
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/opt.h"
#include "lwip/sys.h"
#include "lwip/tcpip.h"
#include "lwip/timers.h"
#include "netif/etharp.h"
#include "pktdrv.h"

#include <hal/input.h>
#include <hal/xbox.h>

#include <pbkit/pbkit.h>
#include <pbkit/outer.h>

#include <xboxkrnl/xboxkrnl.h>
#include <xboxrt/debug.h>
#include <xboxrt/string.h>


const uint16_t server_port = 9269; // XBOX on phone ;)


//FIXME: Not sure if this works #define LWIP_DBG_OFF





//#define USE_DHCP         1
#define PKT_TMR_INTERVAL 5 /* ms */
#define DEBUGGING        0


struct netif nforce_netif;
struct netif* g_pnetif;
err_t nforceif_init(struct netif *netif);


typedef struct {
  uint32_t words[4];
} ProgramInstruction;

typedef union {
  struct {
    float x;
    float y;
    float z;
    float w;
  };
  struct {
    uint32_t x;
    uint32_t y;
    uint32_t z;
    uint32_t w;
  } raw;
} ProgramConstant;

void SetVertexProgram(const uint32_t* tokens, size_t num_instructions, bool state_shader) {
    DWORD *p = pb_begin();

    pb_push(p++,NV097_SET_TRANSFORM_PROGRAM_START,1);
    *(p++)=0; //set run address of shader

    pb_push(p++,NV097_SET_TRANSFORM_EXECUTION_MODE,2);
    *(p++)=SHADER_TYPE_EXTERNAL;   //set shader vertex type: external shader = vertex program
    *(p++)=(state_shader ? SHADER_SUBTYPE_WRITE : SHADER_SUBTYPE_REGULAR); //NV097_SET_TRANSFORM_PROGRAM_CXT_WRITE_EN

    pb_push(p++,NV097_SET_TRANSFORM_PROGRAM_LOAD,1);
    *(p++)=0; //set cursor in order to load data into program area

    for (unsigned int i = 0; i < num_instructions; i++) {
        pb_push(p++, NV097_SET_TRANSFORM_PROGRAM, 4);
        memcpy(p, &tokens[i*4], 4 * 4);
        p+=4;
    }

    pb_end(p);
}

void RunStateShader() {
  debugPrint("Running shader!!!\n");

    DWORD *p = pb_begin();

    //FIXME: Transform_data
    pb_push4f(p,NV097_SET_TRANSFORM_DATA, 1.2, 3.4, 5.6, 7.8); p+=5;

    DWORD slot = 0;

    pb_push1(p,NV097_LAUNCH_TRANSFORM_PROGRAM, slot); p+=2;

    pb_end(p);

}

void ReadRam(uint32_t select, uint32_t address, uint32_t* data, uint32_t data_count) {
  // Pause pushbuffer execution and wait until GPU is idle
//FIXME: Is this enough to run the pushbuffer until the very end?
  DWORD old_dma_push = pb_wait_until_tiles_not_busy();
  pb_wait_until_gr_not_busy();

  // Retrieve data from the given offset
//FIXME: pbkit loves to be explicit: addr10=((tile_index*4+0x10)&NV_PGRAPH_RDI_INDEX_ADDRESS)|((0xEA<<16)&NV_PGRAPH_RDI_INDEX_SELECT);
  DWORD rdi_index = VIDEOREG(NV_PGRAPH_RDI_INDEX); // FIXME: Might not be necessary
  VIDEOREG(NV_PGRAPH_RDI_INDEX) = (select << 16) | (address << 2);
  while(data_count--) {
    *data++ = VIDEOREG(NV_PGRAPH_RDI_DATA);
  }
  VIDEOREG(NV_PGRAPH_RDI_INDEX) = rdi_index; // FIXME: Restore previous value, is this necessary?

  // Continue pushbuffer execution
  VIDEOREG(NV_PFIFO_CACHE1_DMA_PUSH)=old_dma_push;
  debugPrint("Data read back!!!\n");
}

ProgramConstant* RunShaderOnData(const ProgramInstruction* instructions,
                                 const ProgramConstant* input_constants) {

  // Upload constants
  DWORD* p=pb_begin();
  pb_push1(p,NV20_TCL_PRIMITIVE_3D_VP_UPLOAD_CONST_ID, 0); p+=2;
  for(unsigned int i = 0; i < 192; i++) {
    const ProgramConstant* input_constant = &input_constants[i];
    pb_push(p++, NV20_TCL_PRIMITIVE_3D_VP_UPLOAD_CONST_X, 4);
    memcpy(p, &input_constants[i], 4 * 4);
    p+=4;
  }
  pb_end(p);

  // Upload vertex program
  SetVertexProgram((const uint32_t*)instructions, 136, true);

  // Run Shader
  RunStateShader();

  // Dump shader constants from GPU RAM
  ProgramConstant* output_constants = malloc(192 * sizeof(ProgramConstant));
  ReadRam(0x17, 0, (uint32_t*)output_constants, 192 * sizeof(ProgramConstant) / 4);

  for(unsigned int i = 0; i < 192; i++) {
    // These are stored as WZYX in RAM, swizzle back
    ProgramConstant* output_constant = &output_constants[i];
    uint32_t* w = &output_constant->raw.x;
    uint32_t* z = &output_constant->raw.y;
    uint32_t* y = &output_constant->raw.z;
    uint32_t* x = &output_constant->raw.w;
    uint32_t tmp;
    // Swap X and W: WZYX -> XZYW
    tmp = *x;
    *x = *w;
    *w = tmp;
    // Swap Y and Z: XZYW -> XYZW
    tmp = *y;
    *y = *z;
    *z = tmp;
  }

  return output_constants; 
}

static bool ReceiveRequest(struct netconn *conn) {
  err_t err;
  debugPrint("Receiving request\n");
   
  // Block requests of wrong size
  u16_t expected_len = 136 * sizeof(ProgramInstruction) + 192 * sizeof(ProgramConstant);
  char* buf = malloc(expected_len);

  //FIXME: Disconnect if this takes too long, the lwip timeout is broken :(
  u16_t len = 0;
  while(len < expected_len) {
    struct netbuf *inbuf;

    /* Read the data from the port, blocking if nothing yet there. 
     We assume the request (the part we care about) is in one netbuf */
    err = netconn_recv(conn, &inbuf);

    // Abort if the receive failed
    if (err != ERR_OK) {
      debugPrint("Receive failed!\n");
      break;
    }

    // Receive data and avoid buffer overflows
    u16_t chunk_len = netbuf_len(inbuf);
    debugPrint("Received %d bytes!\n", chunk_len);
    if ((len + chunk_len) > expected_len) {
      netbuf_delete(inbuf);
      break;
    }
    netbuf_copy(inbuf, &buf[len], chunk_len);
    len += chunk_len;
    debugPrint("Received %d / %d!\n", len, expected_len);

    /* Delete the buffer (netconn_recv gives us ownership,
     so we have to make sure to deallocate the buffer) */
    netbuf_delete(inbuf);
  }

  // Only handle requests of proper size
  if (len != expected_len) {
    debugPrint("Expected %d. Got %d!\n", expected_len, len);
    free(buf);
    return false;
  }

  // Copy arguments into a more usable format
  char* cursor = buf;
  const ProgramInstruction* instructions = (const ProgramInstruction*)cursor;
  cursor += 136 * sizeof(ProgramInstruction);
  const ProgramConstant* input_constants = (const ProgramConstant*)cursor;
  cursor += 192 * sizeof(ProgramConstant);

  // Update the shader code
  ProgramConstant* output_constants = RunShaderOnData(instructions, input_constants);

  // Send back the result
  netconn_write(conn, output_constants, 192 * sizeof(ProgramConstant), NETCONN_COPY);

  // Clear buffers
  free(buf);
  free(output_constants);

  return true;
}

static void WaitForRequest(struct netconn *conn) {
  err_t err;

  // Get and display remote ip address and request data
  ip_addr_t naddr;
  u16_t port = 0;
  netconn_peer(conn, &naddr, &port);
  debugPrint("[Connection from %s]\n", ip4addr_ntoa(ip_2_ip4(&naddr)));

  debugPrint("Waiting for request\n");
  

  if (!ReceiveRequest(conn)) {
    debugPrint("Oops! that failed..\n");
  }

  /* Close the connection (server closes in ShaderServer) */
  netconn_close(conn);
}

static void RequestThread(void *arg) {
  struct netconn *conn, *newconn;
  err_t err;
  LWIP_UNUSED_ARG(arg);
  
  /* Create a new TCP connection handle */
  conn = netconn_new(NETCONN_TCP);
  LWIP_ERROR("ShaderServer_server: invalid conn", (conn != NULL), return;);
  
  /* Bind to port with default IP address */
  netconn_bind(conn, NULL, server_port);
  
  /* Put the connection into LISTEN state */
  netconn_listen(conn);
  
  do {
    debugPrint("\nWaiting for connection\n");
    err = netconn_accept(conn, &newconn);
    if (err == ERR_OK) {
      debugClearScreen();
      WaitForRequest(newconn);
      netconn_delete(newconn);
    }
  } while(err == ERR_OK);
  //FIXME: LWIP debug is weird.. how to use this?  LWIP_DEBUGF(LWIP_DBG_OFF, ("ShaderServer_server_netconn_thread: netconn_accept received error %d, shutting down", err));
  netconn_close(conn);
  netconn_delete(conn);
}

void CreateRequestThread(void) {
  sys_thread_new("RequestThread", RequestThread, NULL, DEFAULT_THREAD_STACKSIZE, DEFAULT_THREAD_PRIO);
}

static void tcpip_init_done(void *arg) {
	sys_sem_t *init_complete = arg;
	sys_sem_signal(init_complete);
}

static void packet_timer(void *arg) {
  LWIP_UNUSED_ARG(arg);
  Pktdrv_ReceivePackets();
  sys_timeout(PKT_TMR_INTERVAL, packet_timer, NULL);
}


void main(void) {
	sys_sem_t init_complete;
	const ip4_addr_t *ip;
	static ip4_addr_t ipaddr, netmask, gw;

#if DEBUGGING
	asm volatile ("jmp .");
	debug_flags = LWIP_DBG_ON;
#else
	debug_flags = 0;
#endif

#if USE_DHCP
	IP4_ADDR(&gw, 0,0,0,0);
	IP4_ADDR(&ipaddr, 0,0,0,0);
	IP4_ADDR(&netmask, 0,0,0,0);
#else

  NTSTATUS status;

  typedef union {
    DWORD v;
    uint8_t b[4];
  } IP;

  IP ip_address; // 13
  // DWORD dns_server; // 14
  IP gateway_address; // 15
  IP subnet_mask; // 16

  DWORD type;

  status = ExQueryNonVolatileSetting(13, &type, (PUCHAR)&ip_address, sizeof(ip_address), NULL);
  status = ExQueryNonVolatileSetting(14, &type, (PUCHAR)&gateway_address, sizeof(gateway_address), NULL);
  status = ExQueryNonVolatileSetting(16, &type, (PUCHAR)&subnet_mask, sizeof(subnet_mask), NULL);

  //FIXME: Assert that all of this worked..

  if (ip_address.v == 0x00000000) {
    //FIXME: Turn on DHCP?!
    ip_address.b[0] = 192;
    ip_address.b[1] = 168;
    ip_address.b[2] = 178;
    ip_address.b[3] = 80;

    subnet_mask.b[0] = 255;
    subnet_mask.b[1] = 255;
    subnet_mask.b[2] = 255;
    subnet_mask.b[3] = 0;

    gateway_address.b[0] = 192;
    gateway_address.b[1] = 168;
    gateway_address.b[2] = 178;
    gateway_address.b[3] = 1;
  }

	IP4_ADDR(&ipaddr, ip_address.b[0], ip_address.b[1], ip_address.b[2], ip_address.b[3]);
	IP4_ADDR(&netmask, subnet_mask.b[0], subnet_mask.b[1], subnet_mask.b[2], subnet_mask.b[3]);
	IP4_ADDR(&gw, gateway_address.b[0], gateway_address.b[1], gateway_address.b[2], gateway_address.b[3]);
#endif

	/* Initialize the TCP/IP stack. Wait for completion. */
	sys_sem_new(&init_complete, 0);
	tcpip_init(tcpip_init_done, &init_complete);
	sys_sem_wait(&init_complete);
	sys_sem_free(&init_complete);

	pb_init();
	pb_show_debug_screen();

	g_pnetif = netif_add(&nforce_netif, &ipaddr, &netmask, &gw, NULL, nforceif_init, ethernet_input);
	if (!g_pnetif) {
		debugPrint("netif_add failed\n");
		return;
	}

	netif_set_default(g_pnetif);
	netif_set_up(g_pnetif);

#if USE_DHCP
	dhcp_start(g_pnetif);
#endif

	packet_timer(NULL);

#if USE_DHCP
	debugPrint("Waiting for DHCP...\n");
	while (g_pnetif->dhcp->state != DHCP_STATE_BOUND) {
		NtYieldExecution();
  }
	debugPrint("DHCP bound!\n");
#endif

	debugPrint("\n");
	debugPrint("IP address.. %s\n", ip4addr_ntoa(netif_ip4_addr(g_pnetif)));
	debugPrint("Mask........ %s\n", ip4addr_ntoa(netif_ip4_netmask(g_pnetif)));
	debugPrint("Gateway..... %s\n", ip4addr_ntoa(netif_ip4_gw(g_pnetif)));
	debugPrint("\n");

	CreateRequestThread();
	while (1) {

#if 0
    MM_STATISTICS s;
    memset(&s, 0, sizeof(s));
    s.Length = sizeof(s);
    MmQueryStatistics(&s);

    debugClearScreen();

#define DP(x) debugPrint(#x ": %u\n", s.x);

    DP(Length);
    DP(TotalPhysicalPages);
    DP(AvailablePages);
    DP(VirtualMemoryBytesCommitted);
    DP(VirtualMemoryBytesReserved);
    DP(CachePagesCommitted);
    DP(PoolPagesCommitted);
    DP(StackPagesCommitted);
    DP(ImagePagesCommitted);

    static int i = 0;
    debugPrint("Iteration %u\n", i++);

    XSleep(500);
#endif

    NtYieldExecution();
  }
	Pktdrv_Quit();
	return;
}
