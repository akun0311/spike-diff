#include "mmu.h"
#include "sim.h"
#include <stdio.h>
#include <stdbool.h>
#include <sys/types.h>

//至今用到的csrs
//

enum csr_user{
  cycle = 0xc00, timer = 0xc01, instret = 0xc02
};
enum csr_machine{
    menvcfg = 0x30a,
    mvendorid = 0xf11, marchid = 0xf12, mimpid = 0xf13, mhartid = 0xf14, mconfigptr = 0xf15,
    mstatus = 0x300, misa = 0x301, medeleg = 0x302, mideleg = 0x303, mie = 0x304, mtvec = 0x305, mcounteren = 0x306,
    mscratch = 0x340, mepc = 0x341, mcause = 0x342, mtval = 0x343, mip = 0x344,
    mcycle = 0xb00, minstret = 0xb02, tselect = 0x7a0, tdata1 = 0x7a1
};
enum csr_super{
    stimecmp =0x14d,  sstatus = 0x100, sie = 0x104,     stvec = 0x105, scounteren = 0x106,
    sscratch = 0x140, sepc = 0x141,    scause = 0x142,  stval = 0x143, sip = 0x144,
    satp = 0x180
};
enum csr_pmp{
    pmpcfg0 = 0x3a0, pmpcfg1 = 0x3a1, pmpcfg2 = 0x3a2, pmpcfg3 = 0x3a3,
    pmpaddr0 = 0x3b0, pmpaddr1 = 0x3b1, pmpaddr2 = 0x3b2, pmpaddr3 = 0x3b3,
    pmpaddr4 = 0x3b4, pmpaddr5 = 0x3b5, pmpaddr6 = 0x3b6, pmpaddr7 = 0x3b7,
    pmpaddr8 = 0x3b8, pmpaddr9 = 0x3b9, pmpaddr10 = 0x3ba, pmpaddr11 = 0x3bb,
    pmpaddr12 = 0x3bc, pmpaddr13 = 0x3bd, pmpaddr14 = 0x3be, pmpaddr15 = 0x3bf
};

#define NR_GPR 32
#define NR_CSR 4096
#define CONFIG_MSIZE 0x8000000
#define __EXPORT __attribute__((visibility("default")))
enum { DIFFTEST_TO_DUT, DIFFTEST_TO_REF };

#define RV64
#ifdef RV32
  typedef uint32_t word_t;
  typedef int32_t sword_t;
  typedef uint32_t paddr_t;
#endif

#ifdef RV64
  typedef uint64_t word_t;
  typedef int64_t sword_t;
  typedef uint64_t paddr_t;
#endif


static std::vector<std::pair<reg_t, abstract_device_t*>> difftest_plugin_devices;
static std::vector<std::string> difftest_htif_args;
static std::vector<std::pair<reg_t, mem_t*>> difftest_mem(
    1, std::make_pair(reg_t(DRAM_BASE), new mem_t(CONFIG_MSIZE)));
static debug_module_config_t difftest_dm_config = {
  .progbufsize = 2,
  .max_sba_data_width = 0,
  .require_authentication = false,
  .abstract_rti = 0,
  .support_hasel = true,
  .support_abstract_csr_access = true,
  .support_abstract_fpr_access = true,
  .support_haltgroups = true,
  .support_impebreak = true
};

//这个是用于difftest的结构体
struct diff_context_t {
  uint64_t gpr[32];
  uint64_t pc;
  uint64_t csr[4096];
};
static sim_t* s = NULL;
static processor_t *p = NULL;
static state_t *state = NULL;

void sim_t::diff_init(int port) {
  p = get_core("0");
  state = p->get_state();
}

void sim_t::diff_step(uint64_t n) {
  step(n);
}



void sim_t::diff_get_regs(void* diff_context) {
  struct diff_context_t* dut = (struct diff_context_t*)diff_context;
  dut->pc = state->pc;
  for (int i = 0; i < NR_GPR; i++){
    dut->gpr[i] = state->XPR[i];
  } 
  //Machine Mode Registers
  dut->csr[mhartid]    = p->get_csr(mhartid); // 或者 state->get_csr(mhartid)
  dut->csr[mstatus]    = p->get_csr(mstatus);
  dut->csr[mepc]       = p->get_csr(mepc);
  dut->csr[medeleg]    = p->get_csr(medeleg);
  dut->csr[mideleg]    = p->get_csr(mideleg);
  dut->csr[mie]        = p->get_csr(mie);
  dut->csr[menvcfg]    = p->get_csr(menvcfg);
  dut->csr[mcounteren] = p->get_csr(mcounteren);  
  // Supervisor Mode Registers
  dut->csr[satp]       = p->get_csr(satp);
  dut->csr[sie]        = p->get_csr(sie);
  dut->csr[stimecmp]   = p->get_csr(stimecmp);
  // PMP Registers
  dut->csr[pmpaddr0]   = p->get_csr(pmpaddr0);
  dut->csr[pmpcfg0]    = p->get_csr(pmpcfg0);
  //user 
  dut->csr[timer]      = p->get_csr(timer);
}

void sim_t::diff_set_regs(void* diff_context) {
  struct diff_context_t* dut = (struct diff_context_t*)diff_context;
  state->pc = dut->pc;
  for (int i = 0; i < NR_GPR; i++) {
    state->XPR.write(i, (sword_t)dut->gpr[i]);
  }
    
// // Machine Mode Registers
//   p->put_csr(mhartid,    dut->csr[mhartid]);
//   p->put_csr(mstatus,    dut->csr[mstatus]);
//   p->put_csr(mepc,       dut->csr[mepc]);
//   p->put_csr(medeleg,    dut->csr[medeleg]);
//   p->put_csr(mideleg,    dut->csr[mideleg]);
//   p->put_csr(mie,        dut->csr[mie]);
//   p->put_csr(menvcfg,    dut->csr[menvcfg]);
//   p->put_csr(mcounteren, dut->csr[mcounteren]);  

//   // Supervisor Mode Registers
//   p->put_csr(satp,       dut->csr[satp]);
//   p->put_csr(sie,        dut->csr[sie]);
//   p->put_csr(stimecmp,   dut->csr[stimecmp]);

//   // PMP Registers
//   p->put_csr(pmpaddr0,   dut->csr[pmpaddr0]);
//   p->put_csr(pmpcfg0,    dut->csr[pmpcfg0]);

//   // User Mode Registers
//   p->put_csr(timer,      dut->csr[timer]);
}

void sim_t::diff_memcpy(reg_t dest, void* src, size_t n) {
  mmu_t* mmu = p->get_mmu();
  for (size_t i = 0; i < n; i++) {
    mmu->store<uint8_t>(dest+i, *((uint8_t*)src+i));
  }
}

void sim_t::diff_get_mem(reg_t src, void* dest, size_t n) {
  mmu_t* mmu = p->get_mmu();
  for (size_t i = 0; i < n; i++) {
    *((uint8_t*)dest + i) = mmu->load<uint8_t>(src + i);
  }
}

extern "C" {
  __EXPORT void difftest_memcpy(paddr_t addr, void *buf, size_t n, bool direction) {  
  //将处理器的内存情况写入到spike里面
  if (direction == DIFFTEST_TO_REF) {
    s->diff_memcpy(addr, buf, n);
  } 
  //将spike的内存情况写入到处理器里面，这种情况不存在，所以不会执行
  else{
    s->diff_get_mem(addr, buf, n);
  }
}

__EXPORT void difftest_regcpy(void* dut, bool direction) {
  //将处理器的状态写入到spike里面
  if (direction == DIFFTEST_TO_REF) {
    s->diff_set_regs(dut);
  } 
  //将spike的状态写入到处理器里面
  else {
    s->diff_get_regs(dut);
  }
  
}

__EXPORT void difftest_exec(uint64_t n) {
  s->diff_step(n);
}

__EXPORT void difftest_init(int port) {
  difftest_htif_args.push_back("");
#ifdef RV32
  const char *isa = "RV32IMAFC";
#endif
#ifdef RV64
  const char *isa = "RV64IMAFC";
#endif
  cfg_t cfg(/*default_initrd_bounds=*/    std::make_pair((reg_t)0, (reg_t)0),
            /*default_bootargs=*/         nullptr,
            /*default_isa=*/              isa,
            /*default_priv=*/             DEFAULT_PRIV,
            /*default_varch=*/            DEFAULT_VARCH,
            /*default_misaligned=*/       false,
            /*default_endianness*/        endianness_little,
            /*default_pmpregions=*/       16,
            /*default_mem_layout=*/       std::vector<mem_cfg_t>(),
            /*default_hartids=*/          std::vector<size_t>(1),
            /*default_real_time_clint=*/  false,
            /*default_trigger_count=*/4);
  s = new sim_t(
      &cfg, 
      false, 
      difftest_mem, 
      difftest_plugin_devices, 
      difftest_htif_args,
      difftest_dm_config, 
      nullptr, 
      false, 
      NULL,
      false,
      NULL,
      true);
  s->diff_init(port);
}

__EXPORT void difftest_raise_intr(uint64_t NO) {
  trap_t t(NO);
  p->take_trap_public(t, state->pc);
}
}
