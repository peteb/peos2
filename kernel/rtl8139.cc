// rtl8139 - driver for realtek 8139 NIC
//
// There can be at most 4 outstanding 1500 byte packets waiting for
// DMA. The interrupt handler will advance the current tx state and
// the producing side needs to wait if there's no more space.
//

#include <stddef.h>

#include "rtl8139.h"
#include "pci.h"
#include "debug.h"
#include "memareas.h"
#include "x86.h"
#include "protected_mode.h"
#include "filesystem.h"
#include "process.h"
#include "syscall_decls.h"
#include "syscall_utils.h"

#include "support/utils.h"
#include "support/optional.h"
#include "support/blocking_queue.h"
#include "support/ring_buffer.h"

// Registers
#define IDR0        0x00
#define MAR0        0x08
#define TSD0        0x10  // Transmit Status 0-4
#define TSAD0       0x20  // Transmit Start Address 0-4
#define RBSTART     0x30
#define CR          0x37
#define CAPR        0x38
#define IMR         0x3C
#define ISR         0x3E
#define RCR         0x44
#define CONFIG1     0x52
#define MSR         0x58

#define CR_BUFE     0x0001  // Rx Buffer Empty
#define CR_TE       0x0004  // Transmitter Enable
#define CR_RE       0x0008  // Receiver Enable
#define CR_RST      0x0010  // Reset

#define IMR_ROK     0x0001  // Receive OK
#define IMR_RER     0x0002  // Receive Error
#define IMR_TOK     0x0004  // Transmit OK
#define IMR_TER     0x0008  // Transmit Error
#define IMR_RXOVW   0x0010  // Rx Buffer Overflow
#define IMR_PUN     0x0020  // Packet Underrun
#define IMR_FOVW    0x0040  // Rx FIFO Overflow
#define IMR_LENCHG  0x2000  // Cable Length Change
#define IMR_TIMEOUT 0x4000  // Time Out
#define IMR_SERR    0x8000  // System Error

#define RCR_AAP        0x0001  // Accept All Packets
#define RCR_APM        0x0002  // Accept Physical Match packets
#define RCR_AM         0x0004  // Accept Multicast packets
#define RCR_AB         0x0008  // Accept Broadcast packets
#define RCR_AR         0x0010  // Acecpt Raunt
#define RCR_AER        0x0020  // Accept Error Packet
#define RCR_WRAP       0x0080
#define RCR_RBLEN_8K   0x0000  // Rx Buffer Length

#define MSR_RXPF       0x01  // Receive Pause
#define MSR_TXPF       0x02  // Transmit Pause
#define MSR_LINKB      0x04  // Inverse of Link status
#define MSR_SPEED_10   0x08  // Speed, 1 = 10 Mbps, 0 = 100 Mbps
#define MSR_RXFCE      0x40  // RX Flow control Enable

#define RXS_ROK      0x0001  // Receive OK
#define RXS_FAE      0x0002  // Frame Alignment Error
#define RXS_CRC      0x0004  // CRC Error
#define RXS_LONG     0x0008  // Long Packet
#define RXS_RUNT     0x0010  // Runt Packet Received
#define RXS_ISE      0x0020  // Invalid Symbol Error

#define TSR_OWN      0x00002000  // TX DMA is completed
#define TSR_TUN      0x00004000  // Transmit FIFO underrun
#define TSR_TOK      0x00008000  // Transmit OK
#define TSR_OWC      0x20000000  // Out of Window Collision
#define TSR_TABT     0x40000000  // Transmit Abort
#define TSR_CRS      0x80000000  // Carrier Sense Lost

static pci_device *dev;

// RCR_RBLEN_8K and WRAP requires space for up to one more packet
static const uint16_t rx_ring_size = 8 * 1024;
static uint8_t rx_buffer[rx_ring_size + 16 + 1500] alignas(uint32_t);
static uint16_t rx_pos;

// Tx state
static uint8_t tx_buffers[4][1516];
static volatile uint32_t tx_cur_write = 0;
static volatile uint32_t tx_cur_send = 0;

// Device driver state
static p2::opt<proc_handle> process_opened;
static p2::blocking_data_queue<10 * 1024> read_fifo;
static p2::ring_buffer<10 * 1024> pending_tx;

// Declarations
extern "C" void isr_rtl8139(isr_registers *);
static void receive(pci_device *dev);
static int transmit(pci_device *dev, const char *data, size_t length);
static void print_hwaddress();

static int open(vfs_device *device, const char *path, uint32_t flags);
static int close(int handle);
static int read(int handle, char *data, int length);
static int write(int handle, const char *data, int length);
static int control(int handle, uint32_t function, uint32_t param1, uint32_t param2);

void rtl8139_init()
{
  // TODO: should we start and reset the device when the user opens or
  // closes the file?
  // TODO: support multiple devices

  dev = pci_find_device(0x10EC, 0x8139);
  if (!dev) {
    dbg_puts(rtl8139, "no pci device");
    return;
  }

  dbg_puts(rtl8139, "init...");


  // Enable PCI Bus Mastering
  uint16_t pci_cmd = pci_read_command(dev);
  pci_write_command(dev, pci_cmd | 4);

  assert(!dev->mmio && "supports only port-based IO");

  // Turn on device and reset
  outb(dev->iobase + CONFIG1, 0);
  outb(dev->iobase + CR, CR_RST);
  while ((inb(dev->iobase + CR) & CR_RST) != 0);

  outd(dev->iobase + RBSTART, KERNVIRT2PHYS((uintptr_t)rx_buffer));
  outw(dev->iobase + IMR, IMR_ROK|IMR_TOK);
  outd(dev->iobase + RCR, RCR_WRAP|RCR_RBLEN_8K|RCR_AAP|RCR_APM|RCR_AM|RCR_AB);
  outb(dev->iobase + CR, CR_TE|CR_RE);

  // TODO: do we have to activate auto-negotiation?
  if (inb(dev->iobase + MSR) & MSR_LINKB) {
    dbg_puts(rtl8139, "link fail");
    return;
  }

  print_hwaddress();
  int_register(IRQ_BASE_INTERRUPT + dev->irq, isr_rtl8139, KERNEL_CODE_SEL, IDT_TYPE_INTERRUPT|IDT_TYPE_D|IDT_TYPE_P);
  irq_enable(dev->irq);

  static vfs_device_driver interface = {
    .write = write,
    .read = read,
    .open = open,
    .close = close,
    .control = control,
    .seek = nullptr,
    .tell = nullptr,
    .mkdir = nullptr
  };

  vfs_node_handle mountpoint = vfs_create_node(VFS_FILESYSTEM);
  vfs_set_driver(mountpoint, &interface, nullptr);
  vfs_add_dirent(vfs_lookup("/dev/"), "eth0", mountpoint);
}

extern "C" void int_rtl8139(isr_registers */*regs*/)
{
  uint16_t status = inw(dev->iobase + ISR);
  outw(dev->iobase + ISR, status);

  if (status & IMR_TOK) {
    while (tx_cur_send < tx_cur_write) {
      uint8_t send_desc = tx_cur_send % 4;
      uint32_t send_desc_status = ind(dev->iobase + TSD0 + send_desc * 4);

      // TODO: a lot more error handling!
      if (send_desc_status & (TSR_TUN|TSR_OWC|TSR_TABT|TSR_CRS)) {
        dbg_puts(rtl8139, "descriptor %d dropped due to error %x", send_desc, send_desc_status);
      }
      else if (!(send_desc_status & TSR_OWN)) {
        // Packet not DMA'd yet
        break;
      }

      ++tx_cur_send;
    }
  }

  if (status & IMR_ROK) {
    receive(dev);
  }

  // Normalize cursors
  if (tx_cur_send >= 4) {
    tx_cur_send -= 4;
    tx_cur_write -= 4;
  }

  outd(dev->iobase + ISR, 0xFFFF);  // Clear all ints
  irq_eoi(dev->irq);
}

static void receive(pci_device *dev)
{
  while (!(inb(dev->iobase + CR) & CR_BUFE)) {
    uint32_t packet_header = *(uint32_t *)(rx_buffer + rx_pos);
    uint16_t packet_status = packet_header & 0xFFFF;
    uint16_t packet_size = packet_header >> 16;
    rx_pos += 4;  // Consume header

    // TODO: collect statistics

    if (!(packet_status & RXS_ROK) || packet_size > 1518) {
      dbg_puts(rtl8139, "rx invalid packet, packet_size=%d,rx_pos=%d,status=%x", packet_size, rx_pos, packet_status);
      rx_pos = 0;
      packet_size = 0;
      outb(dev->iobase + CR, CR_TE|CR_RE);
    }
    else {
      uint16_t data_size = packet_size - 4;  // 2 = header size, 4 = trailing CRC
      dbg_puts(rtl8139, "rx valid packet rx_pos=%d,size=%d,datasz=%d",
               rx_pos,
               packet_size,
               data_size);

      if (process_opened) {
        // TODO: check that the buffer has space for both the header and the data first
        // TODO: send a hint to the fifo that we're going to push more, so don't
        //       context switch. We shouldn't really context switch here anyway!
        if (read_fifo.push_back((const char *)&data_size, 2) <= 0)
          dbg_puts(rtl8139, "failed to write size");

        if (read_fifo.push_back((const char *)(rx_buffer + rx_pos), data_size) <= 0)
          dbg_puts(rtl8139, "failed to write content");
      }
    }

    // Our buffer is larger than the ring due to WRAP
    rx_pos = ALIGN_UP(rx_pos + packet_size, 4) % rx_ring_size;
    outw(dev->iobase + CAPR, rx_pos - 16);
  }
}

static int transmit(pci_device *dev, const char *data, size_t length)
{
  if (tx_cur_write >= tx_cur_send + 4) {
    dbg_puts(rtl8139, "waiting for free packet slot...");

    // TODO: better wait mechanism
    while (tx_cur_write >= tx_cur_send + 4);
  }

  uint8_t write_desc = tx_cur_write % 4;
  uint32_t current_desc_status = ind(dev->iobase + TSD0 + write_desc * 4);

  // This is strange, why did we receive a slot that hasn't completed DMA?
  if (!(current_desc_status & TSR_OWN)) {
    dbg_puts(rtl8139, "desc %d busy", write_desc);
    return -1;
  }

  uint8_t *buffer = tx_buffers[write_desc];
  memcpy(buffer, data, p2::min<size_t>(length, 1500));

  // Pad the buffer, NIC will add 4 bytes for CRC
  while (length < 60) {
    buffer[length++] = '\0';
  }

  dbg_puts(rtl8139, "writing to desc %d", write_desc);
  uint32_t new_desc_status = length & 0x1FFF;
  new_desc_status |= (9 << 16);  // TX FIFO thresh 264 bytes
  outd(dev->iobase + TSAD0 + write_desc * 4, KERNVIRT2PHYS((uintptr_t)buffer));
  outd(dev->iobase + TSD0 + write_desc * 4, new_desc_status);
  ++tx_cur_write;

  return 0;
}

static void print_hwaddress()
{
  uint32_t mac0 = ind(dev->iobase + IDR0);
  uint16_t mac1 = inw(dev->iobase + IDR0 + 4);

  uint32_t parts[6] = {
    (mac0 >> 0) & 0xFFu,
    (mac0 >> 8) & 0xFFu,
    (mac0 >> 16) & 0xFFu,
    (mac0 >> 24) & 0xFFu,
    (mac1 >> 0) & 0xFFu,
    (mac1 >> 8) & 0xFFu
  };

  // TODO: hex output
  log(rtl8139, "initialized with hwaddr=%d:%d:%d:%d:%d:%d",
      parts[0], parts[1], parts[2], parts[3], parts[4], parts[5]);
}

static int open(vfs_device */*device*/, const char */*path*/, uint32_t /*flags*/)
{
  if (process_opened)
    return EBUSY;

  process_opened = proc_current_pid();
  return 0;
}

static int close(int /*handle*/)
{
  if (!process_opened || *process_opened != *proc_current_pid())
    return EINVVAL;

  // TODO: empty queue
  process_opened = {};
  return 0;
}

static int read(int /*handle*/, char *data, int length)
{
  return read_fifo.pop_front(data, length);
}

static int write(int /*handle*/, const char *data, int length)
{
  static char buf[1600];  // TODO/NB: global state
  assert(length > 0);

  // pending_tx contains an unfinished outgoing packet, the packet is
  // prefixed with a 16 bit length (like what the user writes on the
  // fd)
  int bytes_writable = p2::min<int>(pending_tx.capacity(), length);
  if (!pending_tx.write(data, bytes_writable)) {
    dbg_puts(rtl8139, "failed to write bytes to pending tx buffer, writable=%d", bytes_writable);
    return 0;
  }

  dbg_puts(rtl8139, "wrote %d bytes to out buffer", bytes_writable);

  while (pending_tx.size() >= sizeof(uint16_t)) {
    // We *might* have a full packet, so try consuming. TODO: move this
    // to interrupt? But which one?

    uint16_t packet_size = 0;

    if (pending_tx.read((char *)&packet_size, 0, sizeof(packet_size)) != sizeof(packet_size))
      break;

    // Check that there's enough to read for the whole packet
    if (pending_tx.size() < sizeof(packet_size) + packet_size)
      break;

    // OK someone has written enough for a packet, consume
    size_t consumed_header = pending_tx.consume(sizeof(packet_size));
    assert(consumed_header == sizeof(packet_size) && "failed to consume pending tx packet size");

    // TODO: can we get rid of the copy below? Yes, we can reference
    // into the ring buffer immediately if the range doesn't wrap
    // around
    size_t consumed_payload = pending_tx.read_front(buf, packet_size);
    assert(consumed_payload == packet_size && "failed to consume pending tx payload");

    dbg_puts(rtl8139, "sending packet size=%d", packet_size);
    transmit(dev, buf, packet_size);
  }

  return bytes_writable;
}

static int control(int /*handle*/, uint32_t function, uint32_t param1, uint32_t /*param2*/)
{
  if (function == CTRL_NET_HW_ADDR) {
    uint8_t *dest = (uint8_t *)param1;
    verify_ptr(rtl8139, dest);

    uint32_t mac0 = ind(dev->iobase + IDR0);
    uint16_t mac1 = inw(dev->iobase + IDR0 + 4);

    dest[0] = (mac0 >> 0) & 0xFFu;
    dest[1] = (mac0 >> 8) & 0xFFu;
    dest[2] = (mac0 >> 16) & 0xFFu;
    dest[3] = (mac0 >> 24) & 0xFFu;
    dest[4] = (mac1 >> 0) & 0xFFu;
    dest[5] = (mac1 >> 8) & 0xFFu;
    return 0;
  }

  return -1;
}
