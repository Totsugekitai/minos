#include <stdint.h>
#include <util/util.h>
#include <device/device.h>
#include <device/sata.h>

#define SATA_SIG_ATA 0x00000101   // SATA drive
#define SATA_SIG_ATAPI 0xEB140101 // SATAPI drive
#define SATA_SIG_SEMB 0xC33C0101  // Enclosure management bridge
#define SATA_SIG_PM 0x96690101    // Port multiplier

#define AHCI_DEV_NULL 0
#define AHCI_DEV_SATA 1
#define AHCI_DEV_SEMB 2
#define AHCI_DEV_PM 3
#define AHCI_DEV_SATAPI 4

#define HBA_PORT_IPM_ACTIVE 1
#define HBA_PORT_DET_PRESENT 3

#define ATA_DEV_BUSY 0x80
#define ATA_DEV_DRQ 0x08

#define SIZE_8KB (8 * 1024 - 1)

// AHCI Address Base
// It is defined at pci.c
extern uint64_t *abar;

void print_hba_memory_register(void)
{
    struct HBA_MEM *hba_memreg = (struct HBA_MEM *)abar;
    //puts_serial("HBA Memory Register\n");

    puts_serial("status of Generic Host Control\n");
    put_str_num_serial("cap: ", (uint64_t)hba_memreg->cap);
    put_str_num_serial("ghc: ", (uint64_t)hba_memreg->ghc);
    put_str_num_serial("is: ", (uint64_t)hba_memreg->is);
    put_str_num_serial("pi: ", (uint64_t)hba_memreg->pi);
    put_str_num_serial("vs: ", (uint64_t)hba_memreg->vs);
    put_str_num_serial("ccc_ctl: ", (uint64_t)hba_memreg->ccc_ctl);
    put_str_num_serial("ccc_pts: ", (uint64_t)hba_memreg->ccc_pts);
    put_str_num_serial("em_loc: ", (uint64_t)hba_memreg->em_loc);
    put_str_num_serial("em_ctl: ", (uint64_t)hba_memreg->em_ctl);
    put_str_num_serial("cap2: ", (uint64_t)hba_memreg->cap2);
    put_str_num_serial("bohc: ", (uint64_t)hba_memreg->bohc);
    puts_serial("\n");
}

void print_port_status(struct HBA_PORT *port)
{
    puts_serial("status of HBA Port\n");
    put_str_num_serial("clb: ", (uint64_t)port->clb);
    put_str_num_serial("clbu: ", (uint64_t)port->clbu);
    put_str_num_serial("fb: ", (uint64_t)port->fb);
    put_str_num_serial("fbu: ", (uint64_t)port->fbu);
    put_str_num_serial("is: ", (uint64_t)port->is);
    put_str_num_serial("ie: ", (uint64_t)port->ie);
    put_str_num_serial("cmd: ", (uint64_t)port->cmd);
    put_str_num_serial("tfd: ", (uint64_t)port->tfd);
    put_str_num_serial("sig: ", (uint64_t)port->sig);
    put_str_num_serial("ssts: ", (uint64_t)port->ssts);
    put_str_num_serial("sctl: ", (uint64_t)port->sctl);
    put_str_num_serial("serr: ", (uint64_t)port->serr);
    put_str_num_serial("sact: ", (uint64_t)port->sact);
    put_str_num_serial("ci: ", (uint64_t)port->ci);
    put_str_num_serial("sntf: ", (uint64_t)port->sntf);
    put_str_num_serial("fbs: ", (uint64_t)port->fbs);
    puts_serial("\n");
}

// Check device type
static int check_type(struct HBA_PORT *port)
{
    uint32_t ssts = port->ssts;

    uint8_t ipm = (ssts >> 8) & 0x0F;
    uint8_t det = ssts & 0x0F;

    if (det != HBA_PORT_DET_PRESENT) { // Check drive status
        return AHCI_DEV_NULL;
    }
    if (ipm != HBA_PORT_IPM_ACTIVE) {
        return AHCI_DEV_NULL;
    }

    switch (port->sig) {
    case SATA_SIG_ATAPI:
        return AHCI_DEV_SATAPI;
    case SATA_SIG_SEMB:
        return AHCI_DEV_SEMB;
    case SATA_SIG_PM:
        return AHCI_DEV_PM;
    default:
        return AHCI_DEV_SATA;
    }
}

void probe_port(void)
{
    // Search disk in implemented ports
    struct HBA_MEM *hba_memreg = (struct HBA_MEM *)abar;
    uint32_t pi = hba_memreg->pi;
    int i = 0;
    while (i < 32) {
        if (pi & 1) {
            int dt = check_type(&hba_memreg->ports[i]);
            if (dt == AHCI_DEV_SATA) {
                put_str_num_serial("SATA drive found at port ", i);
            } else if (dt == AHCI_DEV_SATAPI) {
                put_str_num_serial("SATAPI drive found at port ", i);
            } else if (dt == AHCI_DEV_SEMB) {
                put_str_num_serial("SEMB drive found at port ", i);
            } else if (dt == AHCI_DEV_PM) {
                put_str_num_serial("PM drive found at port ", i);
            } else {
                put_str_num_serial("No drive found at port ", i);
            }
        }

        pi >>= 1;
        i++;
    }
}

int find_cmdslot(struct HBA_PORT *port)
{
    uint32_t slots = (port->sact | port->ci);
    for (int i = 0; i < 32; i++) {
        if ((slots & 1) == 0) {
            return i;
        }
        slots >>= 1;
    }
    puts_serial("Cannot find free command list entry\n");
    return -1;
}

// portを通じて
// count分のセクタを
// startl:starthが指すオフセットから読み取り、
// bufへ書き込む
int read(struct HBA_PORT *port, uint32_t startl, uint32_t starth, uint32_t count, uint16_t *buf)
{
    port->is = (uint32_t)-1; // clear pending interrupt bits

    int slot = find_cmdslot(port);
    if (slot == -1) {
        return 0;
    }

    struct HBA_CMD_HEADER *cmdheader = (struct HBA_CMD_HEADER *)port->clb;
    cmdheader += slot; // 空きスロットへのアドレス
    cmdheader->cfl = sizeof(struct FIS_REG_H2D) / sizeof(uint32_t); // Command FIS size
    cmdheader->w = 0;
    cmdheader->prdtl = (uint16_t)((count - 1) >> 4) + 1; // PRDT entries count

    struct HBA_CMD_TBL *cmdtbl = (struct HBA_CMD_TBL *)(cmdheader->ctba);
    // HBA_CMD_TBL末尾にはPRDT1エントリ分が含まれているのでそれと重複させないようにしつつ
    // command tableを0で初期化する
    memset(cmdtbl, 0, sizeof(struct HBA_CMD_TBL) +
        (cmdheader->prdtl - 1) * sizeof(struct HBA_PRDT_ENTRY));

    // 8KB(16 sectors) per PRDT
    // PRDTエントリ群を設定
    int i;
    for (i = 0; i < cmdheader->prdtl - 1; i++) {
        cmdtbl->prdt_entry[i].dba = (uint32_t)buf;
        cmdtbl->prdt_entry[i].dbc = SIZE_8KB;
        cmdtbl->prdt_entry[i].i = 1; // 割り込みを通知する設定
        buf += 4 * 1024; // 4K words
        count -= 16; // 16 sectors
    }

    // Last entry
    cmdtbl->prdt_entry[i].dba = (uint32_t)buf;
    cmdtbl->prdt_entry[i].dbc = (count << 9) - 1;
    cmdtbl->prdt_entry[i].i = 1;

    // Setup Command
    struct FIS_REG_H2D *cmdfis = (struct FIS_REG_H2D *)(&cmdtbl->cfis);

    cmdfis->fis_type = FIS_TYPE_REG_H2D;
    cmdfis->c = 1; // Commandであることを示す
    cmdfis->command = 0x25; // READ DMA EXTコマンドを表す数値

    cmdfis->lba0 = (uint8_t)startl;
    cmdfis->lba1 = (uint8_t)(startl >> 8);
    cmdfis->lba2 = (uint8_t)(startl >> 16);
    cmdfis->device = 1 << 6; // LBAモード

    cmdfis->lba3 = (uint8_t)(startl >> 24);
    cmdfis->lba4 = (uint8_t)starth;
    cmdfis->lba5 = (uint8_t)(starth >> 8);

    cmdfis->countl = count && 0xff;
    cmdfis->counth = (count >> 8) && 0xff;

    // portがbusy状態を脱するまで待機
    int spin = 0; // spin lock timeout counter
    while ((port->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ)) && spin < 1000000) {
        spin++;
    }
    put_str_num_serial("spin: ", (uint64_t)spin);
    if (spin == 1000000) {
        puts_serial("Port is hung\n");
        return 0;
    }
    // HBAにコマンド送信を要請
    port->cmd |= 1; // PxCMD.ST = 1にする
    port->ci = 1 << slot; // CIを立ててHBAに要請

    // コマンド実行が終了するまでループで待つ
    while (1) {
        //put_str_num_serial("port->ci: ", (uint64_t)port->ci);
        if ((port->ci & (1 << slot)) == 0) {
            puts_serial("read finished\n");
            break;
        }
        // port->isのbit 30がTask file error statusなのでそれを確認
        if (port->is & (1 << 30)) {
            puts_serial("Read disk error\n");
            return 0;
        }
    }

    // もう一回チェック
    if (port->is & (1 << 30)) {
        puts_serial("Read disk error at final check\n");
        return 0;
    }

    return 1;
}

// bufのデータを
// portを通じて
// starth:startlが指すオフセットに書き込む
//int write(uint16_t *buf, struct HBA_PORT *port, uint32_t startl, uint32_t starth)
//{
//
//}

void check_ahci(void)
{
    print_hba_memory_register();

    struct HBA_PORT port = ((struct HBA_MEM *)abar)->ports[0];

    uint16_t buf[512] = {0};

    print_port_status(&port);
    int err = read(&port, 32, 32, 1, buf);
    print_port_status(&port);

    if (!err) {
        puts_serial("error\n");
        return;
    }

    for (int i = 0; i < 256; i++) {
        putnum_serial((uint64_t)buf[i]);
    }
}
