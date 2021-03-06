/* -*- Mode: C++; tab-width: 8; c-basic-offset: 2; indent-tabs-mode: nil; -*- */

#include "kernel_abi.h"

#include <stdlib.h>

#include "Task.h"

using namespace std;

namespace rr {

static const uint8_t int80_insn[] = { 0xcd, 0x80 };
static const uint8_t sysenter_insn[] = { 0x0f, 0x34 };
static const uint8_t syscall_insn[] = { 0x0f, 0x05 };

bool is_at_syscall_instruction(Task* t, remote_code_ptr ptr) {
  bool ok = true;
  vector<uint8_t> code = t->read_mem(ptr.to_data_ptr<uint8_t>(), 2, &ok);
  if (!ok) {
    return false;
  }
  switch (t->arch()) {
    case x86:
      return memcmp(code.data(), int80_insn, sizeof(int80_insn)) == 0 ||
             memcmp(code.data(), sysenter_insn, sizeof(sysenter_insn)) == 0;
    case x86_64:
      return memcmp(code.data(), syscall_insn, sizeof(syscall_insn)) == 0 ||
             memcmp(code.data(), sysenter_insn, sizeof(sysenter_insn)) == 0;
    default:
      assert(0 && "Need to define syscall instructions");
      return false;
  }
}

vector<uint8_t> syscall_instruction(SupportedArch arch) {
  switch (arch) {
    case x86:
      return vector<uint8_t>(int80_insn, int80_insn + sizeof(int80_insn));
    case x86_64:
      return vector<uint8_t>(syscall_insn, syscall_insn + sizeof(syscall_insn));
    default:
      assert(0 && "Need to define syscall instruction");
      return vector<uint8_t>();
  }
}

ssize_t syscall_instruction_length(SupportedArch arch) {
  switch (arch) {
    case x86:
    case x86_64:
      return 2;
    default:
      assert(0 && "Need to define syscall instruction length");
      return 0;
  }
}

template <typename Arch>
static void assign_sigval(typename Arch::sigval_t& to,
                          const NativeArch::sigval_t& from) {
  // si_ptr/si_int are a union and we don't know which part is valid.
  // The only case where it matters is when we're mapping 64->32, in which
  // case we can just assign the ptr first (which is bigger) and then the
  // int (to be endian-independent).
  to.sival_ptr = from.sival_ptr.rptr();
  to.sival_int = from.sival_int;
}

template <typename Arch>
static void set_arch_siginfo_arch(const siginfo_t& src, void* dest,
                                  size_t dest_size) {
  // Copying this structure field-by-field instead of just memcpy'ing
  // siginfo into si serves two purposes: performs 64->32 conversion if
  // necessary, and ensures garbage in any holes in signfo isn't copied to the
  // tracee.
  auto si = static_cast<typename Arch::siginfo_t*>(dest);
  assert(dest_size == sizeof(*si));

  union {
    NativeArch::siginfo_t native_api;
    siginfo_t linux_api;
  } u;
  u.linux_api = src;
  auto& siginfo = u.native_api;

  si->si_signo = siginfo.si_signo;
  si->si_errno = siginfo.si_errno;
  si->si_code = siginfo.si_code;
  switch (siginfo.si_code) {
    case SI_USER:
    case SI_TKILL:
      si->_sifields._kill.si_pid_ = siginfo._sifields._kill.si_pid_;
      si->_sifields._kill.si_uid_ = siginfo._sifields._kill.si_uid_;
      break;
    case SI_QUEUE:
    case SI_MESGQ:
      si->_sifields._rt.si_pid_ = siginfo._sifields._rt.si_pid_;
      si->_sifields._rt.si_uid_ = siginfo._sifields._rt.si_uid_;
      assign_sigval<Arch>(si->_sifields._rt.si_sigval_,
                          siginfo._sifields._rt.si_sigval_);
      break;
    case SI_TIMER:
      si->_sifields._timer.si_overrun_ = siginfo._sifields._timer.si_overrun_;
      si->_sifields._timer.si_tid_ = siginfo._sifields._timer.si_tid_;
      assign_sigval<Arch>(si->_sifields._timer.si_sigval_,
                          siginfo._sifields._timer.si_sigval_);
      break;
  }
  switch (siginfo.si_signo) {
    case SIGCHLD:
      si->_sifields._sigchld.si_pid_ = siginfo._sifields._sigchld.si_pid_;
      si->_sifields._sigchld.si_uid_ = siginfo._sifields._sigchld.si_uid_;
      si->_sifields._sigchld.si_status_ = siginfo._sifields._sigchld.si_status_;
      si->_sifields._sigchld.si_utime_ = siginfo._sifields._sigchld.si_utime_;
      si->_sifields._sigchld.si_stime_ = siginfo._sifields._sigchld.si_stime_;
      break;
    case SIGILL:
    case SIGBUS:
    case SIGFPE:
    case SIGSEGV:
    case SIGTRAP:
      si->_sifields._sigfault.si_addr_ =
          siginfo._sifields._sigfault.si_addr_.rptr();
      si->_sifields._sigfault.si_addr_lsb_ =
          siginfo._sifields._sigfault.si_addr_lsb_;
      break;
    case SIGIO:
      si->_sifields._sigpoll.si_band_ = siginfo._sifields._sigpoll.si_band_;
      si->_sifields._sigpoll.si_fd_ = siginfo._sifields._sigpoll.si_fd_;
      break;
    case SIGSYS:
      si->_sifields._sigsys._call_addr =
          siginfo._sifields._sigsys._call_addr.rptr();
      si->_sifields._sigsys._syscall = siginfo._sifields._sigsys._syscall;
      si->_sifields._sigsys._arch = siginfo._sifields._sigsys._arch;
      break;
  }
}

void set_arch_siginfo(const siginfo_t& siginfo, SupportedArch a, void* dest,
                      size_t dest_size) {
  RR_ARCH_FUNCTION(set_arch_siginfo_arch, a, siginfo, dest, dest_size);
}
}
