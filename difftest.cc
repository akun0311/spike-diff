#include "mmu.h"
#include "sim.h"
#include <stdio.h>
#include <stdbool.h>
#include <sys/types.h>


#define NR_GPR 32
#define CONFIG_MSIZE 0x8000000
#define __EXPORT __attribute__((visibility("default")))
enum { DIFFTEST_TO_DUT, DIFFTEST_TO_REF };

#define RV32
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

enum{
  x0 = 0,
  MSTATUS=0,
  MTVEC,
  MEPC,
  MCAUSE,
  ERROR_CSR_IDX,
};

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

struct diff_context_t {
  word_t gpr[32];
  word_t pc;
  word_t csr[4];
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
  struct diff_context_t* ctx = (struct diff_context_t*)diff_context;
  for (int i = 0; i < NR_GPR; i++) {
    ctx->gpr[i] = state->XPR[i];
  }
  ctx->pc = state->pc;
  ctx->csr[MSTATUS] = state->mstatus->read();
  printf("ctx->csr[MSTATUS]=%d\n", ctx->csr[MSTATUS] );
  state->mtvec->read();
  state->mcause->read();
  state->mepc->read();
}

void sim_t::diff_set_regs(void* diff_context) {
  struct diff_context_t* ctx = (struct diff_context_t*)diff_context;
  for (int i = 0; i < NR_GPR; i++) {
    state->XPR.write(i, (sword_t)ctx->gpr[i]);
  }
  state->pc               = ctx->pc;
}

void sim_t::diff_memcpy(reg_t dest, void* src, size_t n) {
  mmu_t* mmu = p->get_mmu();
  for (size_t i = 0; i < n; i++) {
    mmu->store<uint8_t>(dest+i, *((uint8_t*)src+i));
  }
}

//动态链接库
extern "C" {
__EXPORT void difftest_memcpy(paddr_t addr, void *buf, size_t n, bool direction) {
  if (direction == DIFFTEST_TO_REF) {
    s->diff_memcpy(addr, buf, n);
  } else {
    assert(0);
  }
}

//
__EXPORT void difftest_regcpy(void* dut, bool direction) {
  // `direction`为`DIFFTEST_TO_DUT`时, 获取REF的寄存器状态到`dut`;
  // `direction`为`DIFFTEST_TO_REF`时, 设置REF的寄存器状态为`dut`;
  if (direction == DIFFTEST_TO_REF) {
    s->diff_set_regs(dut);
  } else {
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
  cfg_t cfg(/*default_initrd_bounds=*/std::make_pair((reg_t)0, (reg_t)0),
            /*default_bootargs=*/nullptr,
            /*default_isa=*/isa,
            /*default_priv=*/DEFAULT_PRIV,
            /*default_varch=*/DEFAULT_VARCH,
            /*default_misaligned=*/false,
            /*default_endianness*/endianness_little,
            /*default_pmpregions=*/16,
            /*default_mem_layout=*/std::vector<mem_cfg_t>(),
            /*default_hartids=*/std::vector<size_t>(1),
            /*default_real_time_clint=*/false,
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
