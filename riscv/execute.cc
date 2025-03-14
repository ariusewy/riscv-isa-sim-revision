// See LICENSE for license details.

#include "config.h"
#include "processor.h"
#include "mmu.h"
#include "disasm.h"
#include "decode_macros.h"
#include <cassert>
#include <sstream>
#include <iomanip>

static void commit_log_reset(processor_t* p)
{
  p->get_state()->log_reg_write.clear();
  p->get_state()->log_mem_read.clear();
  p->get_state()->log_mem_write.clear();
}

static void commit_log_stash_privilege(processor_t* p)
{
  state_t* state = p->get_state();
  state->last_inst_priv = state->prv;
  state->last_inst_xlen = p->get_xlen();
  state->last_inst_flen = p->get_flen();
}

static void commit_log_print_value(FILE *log_file, int width, const void *data)
{
  assert(log_file);

  switch (width) {
    case 8:
      fprintf(log_file, "0x%02" PRIx8, *(const uint8_t *)data);
      break;
    case 16:
      fprintf(log_file, "0x%04" PRIx16, *(const uint16_t *)data);
      break;
    case 32:
      fprintf(log_file, "0x%08" PRIx32, *(const uint32_t *)data);
      break;
    case 64:
      fprintf(log_file, "0x%016" PRIx64, *(const uint64_t *)data);
      break;
    default:
      // max lengh of vector
      if (((width - 1) & width) == 0) {
        const uint64_t *arr = (const uint64_t *)data;

        fprintf(log_file, "0x");
        for (int idx = width / 64 - 1; idx >= 0; --idx) {
          fprintf(log_file, "%016" PRIx64, arr[idx]);
        }
      } else {
        abort();
      }
      break;
  }
}

static void commit_log_print_value(FILE *log_file, int width, uint64_t val)
{
  commit_log_print_value(log_file, width, &val);
}

static void commit_log_print_insn(processor_t *p, reg_t pc, insn_t insn)
{
  FILE *log_file = p->get_log_file();

  auto& reg = p->get_state()->log_reg_write;
  auto& load = p->get_state()->log_mem_read;
  auto& store = p->get_state()->log_mem_write;
  int priv = p->get_state()->last_inst_priv;
  int xlen = p->get_state()->last_inst_xlen;
  int flen = p->get_state()->last_inst_flen;

  // print core id on all lines so it is easy to grep
  fprintf(log_file, "core%4" PRId32 ": ", p->get_id()); // core id

  fprintf(log_file, "%1d ", priv); // privilege level
  commit_log_print_value(log_file, xlen, pc); // pc
  fprintf(log_file, " (");
  commit_log_print_value(log_file, insn.length() * 8, insn.bits());// instruction byte
  fprintf(log_file, ")");
  bool show_vec = false;

  for (auto item : reg) {
    if (item.first == 0)
      continue;

    char prefix;
    int size;
    int rd = item.first >> 4;
    bool is_vec = false;
    bool is_vreg = false;
    switch (item.first & 0xf) {
    case 0:// integer register
      size = xlen;
      prefix = 'x';
      break;
    case 1:// floating point register
      size = flen;
      prefix = 'f';
      break;
    case 2:// vector register
      size = p->VU.VLEN;
      prefix = 'v';
      is_vreg = true;
      break;
    case 3:// vector operation register
      is_vec = true;
      break;
    case 4:// CSR register
      size = xlen;
      prefix = 'c';
      break;
    default:
      assert("can't been here" && 0);
      break;
    }

    if (!show_vec && (is_vreg || is_vec)) {
        fprintf(log_file, " e%ld %s%ld l%ld",
                (long)p->VU.vsew,
                p->VU.vflmul < 1 ? "mf" : "m",
                p->VU.vflmul < 1 ? (long)(1 / p->VU.vflmul) : (long)p->VU.vflmul,
                (long)p->VU.vl->read());
        show_vec = true;
    }

    if (!is_vec) {
      if (prefix == 'c')
        fprintf(log_file, " c%d_%s ", rd, csr_name(rd));
      else
        fprintf(log_file, " %c%-2d ", prefix, rd);
      if (is_vreg)
        commit_log_print_value(log_file, size, &p->VU.elt<uint8_t>(rd, 0));
      else
        commit_log_print_value(log_file, size, item.second.v);
    }
  }
  // load
  for (auto item : load) {
    fprintf(log_file, " mem ");
    commit_log_print_value(log_file, xlen, std::get<0>(item));
  }
  // store
  for (auto item : store) {
    fprintf(log_file, " mem ");
    commit_log_print_value(log_file, xlen, std::get<0>(item));
    fprintf(log_file, " ");
    commit_log_print_value(log_file, std::get<2>(item) << 3, std::get<1>(item));
  }
  fprintf(log_file, "\n");
}

static void commit_log_print_insn_new(processor_t *p, reg_t pc, insn_t insn)
{
  FILE *log_file = p->get_log_file();
  bool csr_changed = false;
  std::stringstream s;

  auto& reg = p->get_state()->log_reg_write;
  int priv = p->get_state()->last_inst_priv;
  int xlen = p->get_state()->last_inst_xlen;
  int flen = p->get_state()->last_inst_flen;
  
  std::map<std::string, reg_t> current_csr_values;
  current_csr_values["mstatus"] = p->get_state()->mstatus->read();
  current_csr_values["frm"] = p->get_state()->frm->read();
  current_csr_values["fflags"] = p->get_state()->fflags->read();
  current_csr_values["mcause"] = p->get_state()->mcause->read();
  current_csr_values["scause"] = p->get_state()->scause->read();
  current_csr_values["medeleg"] = p->get_state()->medeleg->read();
  current_csr_values["mcounteren"] = p->get_state()->mcounteren->read();
  current_csr_values["scounteren"] = p->get_state()->scounteren->read();
  current_csr_values["dcsr"] = p->get_state()->dcsr->read();
  
  // 首先检查是否有任何CSR寄存器发生变化
  for (auto &pair : current_csr_values) {
    if (p->last_csr_values.count(pair.first) && p->last_csr_values[pair.first] != pair.second) {
      csr_changed = true;
      break;
    }
  }

  // 如果有变化，先打印标题行
  if (csr_changed) {
    fprintf(log_file, "core%4" PRId32 ": ", p->get_id()); // core id
    fprintf(log_file, "%1d ", priv); // privilege level
    commit_log_print_value(log_file, xlen, pc); // pc
    fprintf(log_file, " (");
    commit_log_print_value(log_file, insn.length() * 8, insn.bits());// instruction byte
    fprintf(log_file, ")"); 
    fprintf(log_file, " %s", p->get_disassembler()->disassemble(insn).c_str());
    fprintf(log_file, "\n");
    
    // 打印执行前的CSR寄存器值，全部使用十六进制格式
    fprintf(log_file, "before:[0x%016" PRIx64 ",0x%x,0x%02" PRIx32 ",0x%02" PRIx64 ",0x%02" PRIx64 ",0x%08" PRIx64 ",0x%08" PRIx64 ",0x%08" PRIx64 ",0x%08" PRIx64 "]",
            p->last_csr_values["mstatus"],
            (unsigned int)p->last_csr_values["frm"],
            (uint32_t)p->last_csr_values["fflags"],
            p->last_csr_values["mcause"],
            p->last_csr_values["scause"],
            p->last_csr_values["medeleg"],
            p->last_csr_values["mcounteren"],
            p->last_csr_values["scounteren"],
            p->last_csr_values["dcsr"]);
    fprintf(log_file, "\n");
    // 打印执行后的CSR寄存器值，全部使用十六进制格式
    fprintf(log_file, "after:[0x%016" PRIx64 ",0x%x,0x%02" PRIx32 ",0x%02" PRIx64 ",0x%02" PRIx64 ",0x%08" PRIx64 ",0x%08" PRIx64 ",0x%08" PRIx64 ",0x%08" PRIx64 "]",
            current_csr_values["mstatus"],
            (unsigned int)current_csr_values["frm"],
            (uint32_t)current_csr_values["fflags"],
            current_csr_values["mcause"],
            current_csr_values["scause"],
            current_csr_values["medeleg"],
            current_csr_values["mcounteren"],
            current_csr_values["scounteren"],
            current_csr_values["dcsr"]);
    
    fprintf(log_file, "\n");
  }
}

inline void processor_t::update_histogram(reg_t pc)
{
  if (histogram_enabled)
    pc_histogram[pc]++;
}

// These two functions are expected to be inlined by the compiler separately in
// the processor_t::step() loop. The logged variant is used in the slow path
static inline reg_t execute_insn_fast(processor_t* p, reg_t pc, insn_fetch_t fetch) {
  return fetch.func(p, fetch.insn, pc);
}
static inline reg_t execute_insn_logged(processor_t* p, reg_t pc, insn_fetch_t fetch)
{
  if (p->get_log_commits_enabled()) {
    commit_log_reset(p);
    commit_log_stash_privilege(p);
  }

  reg_t npc;

  try {
    npc = fetch.func(p, fetch.insn, pc);// execute the instruction
    if (npc != PC_SERIALIZE_BEFORE) {
      if (p->get_log_commits_enabled()) {
        commit_log_print_insn(p, pc, fetch.insn);//print the log commit after the instruction is executed
      }
     }
  } catch (wait_for_interrupt_t &t) {
      if (p->get_log_commits_enabled()) {
        commit_log_print_insn(p, pc, fetch.insn);
      }
      throw;
  } catch(mem_trap_t& t) {
      //handle segfault in midlle of vector load/store
      if (p->get_log_commits_enabled()) {
        for (auto item : p->get_state()->log_reg_write) {
          if ((item.first & 3) == 3) {
            commit_log_print_insn(p, pc, fetch.insn);
            break;
          }
        }
      }
      throw;
  } catch(...) {
    throw;
  }
  p->update_histogram(pc);

  return npc;
}

// 新增函数
static inline reg_t execute_insn_logged_new(processor_t* p, reg_t pc, insn_fetch_t fetch)
{
  if (p->get_log_commits_enabled()) {
    commit_log_reset(p);
    commit_log_stash_privilege(p);
  }

  reg_t npc;

  try {
    npc = fetch.func(p, fetch.insn, pc);// execute the instruction
    if (npc != PC_SERIALIZE_BEFORE) {
      if (p->get_log_commits_enabled()) {
        commit_log_print_insn_new(p, pc, fetch.insn);//print the log commit after the instruction is executed
      }
     }
  } catch (wait_for_interrupt_t &t) {
      if (p->get_log_commits_enabled()) {
        commit_log_print_insn(p, pc, fetch.insn);
      }
      throw;
  } catch(mem_trap_t& t) {
      //handle segfault in midlle of vector load/store
      if (p->get_log_commits_enabled()) {
        for (auto item : p->get_state()->log_reg_write) {
          if ((item.first & 3) == 3) {
            commit_log_print_insn(p, pc, fetch.insn);
            break;
          }
        }
      }
      throw;
  } catch(...) {
    throw;
  }
  p->update_histogram(pc);

  return npc;
}

bool processor_t::slow_path()
{
  return debug || state.single_step != state.STEP_NONE || state.debug_mode ||
         log_commits_enabled || histogram_enabled || in_wfi || check_triggers_icount;
}

// fetch/decode/execute loop
void processor_t::step(size_t n)
{
  if (!state.debug_mode) {
    if (halt_request == HR_REGULAR) {
      enter_debug_mode(DCSR_CAUSE_DEBUGINT);
    } else if (halt_request == HR_GROUP) {
      enter_debug_mode(DCSR_CAUSE_GROUP);
    } // !!!The halt bit in DCSR is deprecated.
    else if (state.dcsr->halt) {
      enter_debug_mode(DCSR_CAUSE_HALT);
    }
  }

  while (n > 0) {
    size_t instret = 0;
    reg_t pc = state.pc;
    mmu_t* _mmu = mmu;

    #define advance_pc() \
      if (unlikely(invalid_pc(pc))) { \
        switch (pc) { \
          case PC_SERIALIZE_BEFORE: state.serialized = true; break; \
          case PC_SERIALIZE_AFTER: ++instret; break; \
          default: abort(); \
        } \
        pc = state.pc; \
        break; \
      } else { \
        state.pc = pc; \
        instret++; \
      }

    try
    {
      take_pending_interrupt();

      if (unlikely(slow_path()))
      {
        // Main simulation loop, slow path.
        while (instret < n)
        {
          if (unlikely(!state.serialized && state.single_step == state.STEP_STEPPED)) {
            state.single_step = state.STEP_NONE;
            if (!state.debug_mode) {
              enter_debug_mode(DCSR_CAUSE_STEP);
              // enter_debug_mode changed state.pc, so we can't just continue.
              break;
            }
          }

          if (unlikely(state.single_step == state.STEP_STEPPING)) {
            state.single_step = state.STEP_STEPPED;
          }

          if (!state.serialized && check_triggers_icount) {
            auto match = TM.detect_icount_match();
            if (match.has_value()) {
              assert(match->timing == triggers::TIMING_BEFORE);
              throw triggers::matched_t((triggers::operation_t)0, 0, match->action, state.v);
            }
          }

          // debug mode wfis must nop
          if (unlikely(in_wfi && !state.debug_mode)) {
            throw wait_for_interrupt_t();
          }

          in_wfi = false;
          insn_fetch_t fetch = mmu->load_insn(pc);
          if (debug && !state.serialized && !coverage){
            disasm(fetch.insn); // 1st line of log
          }
          else if (coverage)
          {
            disasm_new(fetch.insn); 
          }

          if (coverage)
            pc = execute_insn_logged_new(this, pc, fetch);// 2nd line of log
          else
            pc = execute_insn_logged(this, pc, fetch);
          advance_pc();
        }
      }
      else while (instret < n)
      {
        // Main simulation loop, fast path.
        for (auto ic_entry = _mmu->access_icache(pc); ; ) {
          auto fetch = ic_entry->data;
          pc = execute_insn_fast(this, pc, fetch);
          ic_entry = ic_entry->next;
          if (unlikely(ic_entry->tag != pc))
            break;
          if (unlikely(instret + 1 == n))
            break;
          instret++;
          state.pc = pc;
        }

        advance_pc();
      }
    }
    catch(trap_t& t)
    {
      if(coverage)
        take_trap_new(t, pc);
      else
        take_trap(t, pc);
      n = instret;

      // Trigger action takes priority over single step
      auto match = TM.detect_trap_match(t);
      if (match.has_value())
        take_trigger_action(match->action, 0, state.pc, 0);
      else if (unlikely(state.single_step == state.STEP_STEPPED)) {
        state.single_step = state.STEP_NONE;
        enter_debug_mode(DCSR_CAUSE_STEP);
      }
    }
    catch (triggers::matched_t& t)
    {
      if (mmu->matched_trigger) {
        delete mmu->matched_trigger;
        mmu->matched_trigger = NULL;
      }
      take_trigger_action(t.action, t.address, pc, t.gva);
    }
    catch(trap_debug_mode&)
    {
      enter_debug_mode(DCSR_CAUSE_SWBP);
    }
    catch (wait_for_interrupt_t &t)
    {
      // Return to the outer simulation loop, which gives other devices/harts a
      // chance to generate interrupts.
      //
      // In the debug ROM this prevents us from wasting time looping, but also
      // allows us to switch to other threads only once per idle loop in case
      // there is activity.
      n = ++instret;
      in_wfi = true;
    }

    state.minstret->bump(instret);

    // Model a hart whose CPI is 1.
    state.mcycle->bump(instret);

    n -= instret;
  }
}
