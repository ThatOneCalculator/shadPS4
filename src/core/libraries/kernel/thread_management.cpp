// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <mutex>
#include <thread>
#include <semaphore.h>
#include "common/alignment.h"
#include "common/assert.h"
#include "common/error.h"
#include "common/logging/log.h"
#include "common/singleton.h"
#include "common/thread.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/kernel/thread_management.h"
#include "core/libraries/kernel/threads/threads.h"
#include "core/libraries/libs.h"
#include "core/linker.h"
#ifdef _WIN64
#include <windows.h>
#else
#include <sys/mman.h>
#endif

namespace Libraries::Kernel {

thread_local ScePthread g_pthread_self{};
PThreadCxt* g_pthread_cxt = nullptr;

void init_pthreads() {
    g_pthread_cxt = new PThreadCxt{};
    // default mutex init
    ScePthreadMutexattr default_mutexattr = nullptr;
    scePthreadMutexattrInit(&default_mutexattr);
    g_pthread_cxt->setDefaultMutexattr(default_mutexattr);
    // default cond init
    ScePthreadCondattr default_condattr = nullptr;
    scePthreadCondattrInit(&default_condattr);
    g_pthread_cxt->setDefaultCondattr(default_condattr);
    // default attr init
    ScePthreadAttr default_attr = nullptr;
    scePthreadAttrInit(&default_attr);
    g_pthread_cxt->SetDefaultAttr(default_attr);
    // default rw init
    OrbisPthreadRwlockattr default_rwattr = nullptr;
    scePthreadRwlockattrInit(&default_rwattr);
    g_pthread_cxt->setDefaultRwattr(default_rwattr);

    g_pthread_cxt->SetPthreadPool(new PThreadPool);
}

void pthreadInitSelfMainThread() {
    auto* pthread_pool = g_pthread_cxt->GetPthreadPool();
    g_pthread_self = pthread_pool->Create();
    scePthreadAttrInit(&g_pthread_self->attr);
    g_pthread_self->pth = pthread_self();
    g_pthread_self->name = "Main_Thread";
}

int PS4_SYSV_ABI scePthreadAttrInit(ScePthreadAttr* attr) {
    *attr = new PthreadAttrInternal{};

    int result = pthread_attr_init(&(*attr)->pth_attr);

    (*attr)->affinity = 0x7f;
    (*attr)->guard_size = 0x1000;

    SceKernelSchedParam param{};
    param.sched_priority = 700;

    result = (result == 0 ? scePthreadAttrSetinheritsched(attr, 4) : result);
    result = (result == 0 ? scePthreadAttrSetschedparam(attr, &param) : result);
    result = (result == 0 ? scePthreadAttrSetschedpolicy(attr, SCHED_OTHER) : result);
    result = (result == 0 ? scePthreadAttrSetdetachstate(attr, PTHREAD_CREATE_JOINABLE) : result);

    switch (result) {
    case 0:
        return SCE_OK;
    case ENOMEM:
        return SCE_KERNEL_ERROR_ENOMEM;
    default:
        return SCE_KERNEL_ERROR_EINVAL;
    }
}

int PS4_SYSV_ABI scePthreadAttrDestroy(ScePthreadAttr* attr) {

    int result = pthread_attr_destroy(&(*attr)->pth_attr);

    delete *attr;
    *attr = nullptr;

    if (result == 0) {
        return SCE_OK;
    }
    return SCE_KERNEL_ERROR_EINVAL;
}

int PS4_SYSV_ABI scePthreadAttrSetguardsize(ScePthreadAttr* attr, size_t guard_size) {
    if (attr == nullptr || *attr == nullptr) {
        return SCE_KERNEL_ERROR_EINVAL;
    }

    (*attr)->guard_size = guard_size;

    return SCE_OK;
}

int PS4_SYSV_ABI scePthreadAttrGetguardsize(const ScePthreadAttr* attr, size_t* guard_size) {
    if (guard_size == nullptr || attr == nullptr || *attr == nullptr) {
        return SCE_KERNEL_ERROR_EINVAL;
    }

    *guard_size = (*attr)->guard_size;

    return SCE_OK;
}

int PS4_SYSV_ABI scePthreadAttrGetinheritsched(const ScePthreadAttr* attr, int* inherit_sched) {

    if (inherit_sched == nullptr || attr == nullptr || *attr == nullptr) {
        return SCE_KERNEL_ERROR_EINVAL;
    }

    int result = pthread_attr_getinheritsched(&(*attr)->pth_attr, inherit_sched);

    switch (*inherit_sched) {
    case PTHREAD_EXPLICIT_SCHED:
        *inherit_sched = 0;
        break;
    case PTHREAD_INHERIT_SCHED:
        *inherit_sched = 4;
        break;
    default:
        UNREACHABLE();
    }

    return (result == 0 ? SCE_OK : SCE_KERNEL_ERROR_EINVAL);
}

int PS4_SYSV_ABI scePthreadAttrGetdetachstate(const ScePthreadAttr* attr, int* state) {
    if (state == nullptr || attr == nullptr || *attr == nullptr) {
        return SCE_KERNEL_ERROR_EINVAL;
    }

    // int result = pthread_attr_getdetachstate(&(*attr)->pth_attr, state);
    int result = 0;
    *state = ((*attr)->detached ? PTHREAD_CREATE_DETACHED : PTHREAD_CREATE_JOINABLE);

    switch (*state) {
    case PTHREAD_CREATE_JOINABLE:
        *state = 0;
        break;
    case PTHREAD_CREATE_DETACHED:
        *state = 1;
        break;
    default:
        UNREACHABLE();
    }

    return (result == 0 ? SCE_OK : SCE_KERNEL_ERROR_EINVAL);
}

int PS4_SYSV_ABI scePthreadAttrSetdetachstate(ScePthreadAttr* attr, int detachstate) {
    if (attr == nullptr || *attr == nullptr) {
        return SCE_KERNEL_ERROR_EINVAL;
    }

    int pstate = PTHREAD_CREATE_JOINABLE;
    switch (detachstate) {
    case 0:
        pstate = PTHREAD_CREATE_JOINABLE;
        break;
    case 1:
        pstate = PTHREAD_CREATE_DETACHED;
        break;
    default:
        UNREACHABLE_MSG("Invalid detachstate: {}", detachstate);
    }

    // int result = pthread_attr_setdetachstate(&(*attr)->pth_attr, pstate);
    int result = 0;
    (*attr)->detached = (pstate == PTHREAD_CREATE_DETACHED);
    return result == 0 ? SCE_OK : SCE_KERNEL_ERROR_EINVAL;
}

int PS4_SYSV_ABI scePthreadAttrSetinheritsched(ScePthreadAttr* attr, int inheritSched) {
    if (attr == nullptr || *attr == nullptr) {
        return SCE_KERNEL_ERROR_EINVAL;
    }

    int pinherit_sched = PTHREAD_INHERIT_SCHED;
    switch (inheritSched) {
    case 0:
        pinherit_sched = PTHREAD_EXPLICIT_SCHED;
        break;
    case 4:
        pinherit_sched = PTHREAD_INHERIT_SCHED;
        break;
    default:
        UNREACHABLE_MSG("Invalid inheritSched: {}", inheritSched);
    }

    int result = pthread_attr_setinheritsched(&(*attr)->pth_attr, pinherit_sched);

    return result == 0 ? SCE_OK : SCE_KERNEL_ERROR_EINVAL;
}

int PS4_SYSV_ABI scePthreadAttrGetschedparam(const ScePthreadAttr* attr,
                                             SceKernelSchedParam* param) {

    if (param == nullptr || attr == nullptr || *attr == nullptr) {
        return SCE_KERNEL_ERROR_EINVAL;
    }

    int result = pthread_attr_getschedparam(&(*attr)->pth_attr, param);

    if (param->sched_priority <= -2) {
        param->sched_priority = 767;
    } else if (param->sched_priority >= +2) {
        param->sched_priority = 256;
    } else {
        param->sched_priority = 700;
    }

    return result == 0 ? SCE_OK : SCE_KERNEL_ERROR_EINVAL;
}

int PS4_SYSV_ABI scePthreadAttrSetschedparam(ScePthreadAttr* attr,
                                             const SceKernelSchedParam* param) {
    if (param == nullptr || attr == nullptr || *attr == nullptr) {
        return SCE_KERNEL_ERROR_EINVAL;
    }

    SceKernelSchedParam pparam{};
    if (param->sched_priority <= 478) {
        pparam.sched_priority = +2;
    } else if (param->sched_priority >= 733) {
        pparam.sched_priority = -2;
    } else {
        pparam.sched_priority = 0;
    }

    // We always use SCHED_OTHER for now, so don't call this for now.
    // int result = pthread_attr_setschedparam(&(*attr)->pth_attr, &pparam);
    int result = 0;
    return result == 0 ? SCE_OK : SCE_KERNEL_ERROR_EINVAL;
}

int PS4_SYSV_ABI scePthreadAttrGetschedpolicy(const ScePthreadAttr* attr, int* policy) {
    if (policy == nullptr || attr == nullptr || *attr == nullptr) {
        return SCE_KERNEL_ERROR_EINVAL;
    }

    int result = pthread_attr_getschedpolicy(&(*attr)->pth_attr, policy);

    switch (*policy) {
    case SCHED_OTHER:
        *policy = (*attr)->policy;
        break;
    case SCHED_FIFO:
        *policy = 1;
        break;
    case SCHED_RR:
        *policy = 3;
        break;
    default:
        UNREACHABLE();
    }

    return result == 0 ? SCE_OK : SCE_KERNEL_ERROR_EINVAL;
}

int PS4_SYSV_ABI scePthreadAttrSetschedpolicy(ScePthreadAttr* attr, int policy) {
    if (attr == nullptr || *attr == nullptr) {
        return SCE_KERNEL_ERROR_EINVAL;
    }

    int ppolicy = SCHED_OTHER; // winpthreads only supports SCHED_OTHER
    if (policy != SCHED_OTHER) {
        LOG_ERROR(Kernel_Pthread, "policy={} not supported by winpthreads", policy);
    }

    (*attr)->policy = policy;
    int result = pthread_attr_setschedpolicy(&(*attr)->pth_attr, ppolicy);
    return result == 0 ? SCE_OK : SCE_KERNEL_ERROR_EINVAL;
}

ScePthread PS4_SYSV_ABI scePthreadSelf() {
    return g_pthread_self;
}

int PS4_SYSV_ABI scePthreadAttrSetaffinity(ScePthreadAttr* pattr,
                                           const /*SceKernelCpumask*/ u64 mask) {
    LOG_INFO(Kernel_Pthread, "called");

    if (pattr == nullptr || *pattr == nullptr) {
        return SCE_KERNEL_ERROR_EINVAL;
    }

    (*pattr)->affinity = mask;
    return SCE_OK;
}

int PS4_SYSV_ABI scePthreadAttrGetaffinity(const ScePthreadAttr* pattr,
                                           /* SceKernelCpumask*/ u64* mask) {
    if (pattr == nullptr || *pattr == nullptr) {
        return SCE_KERNEL_ERROR_EINVAL;
    }

    *mask = (*pattr)->affinity;

    return SCE_OK;
}

int PS4_SYSV_ABI scePthreadAttrGetstackaddr(const ScePthreadAttr* attr, void** stack_addr) {

    if (stack_addr == nullptr || attr == nullptr || *attr == nullptr) {
        return SCE_KERNEL_ERROR_EINVAL;
    }

    int result = pthread_attr_getstackaddr(&(*attr)->pth_attr, stack_addr);

    return result == 0 ? SCE_OK : SCE_KERNEL_ERROR_EINVAL;
}

int PS4_SYSV_ABI scePthreadAttrGetstacksize(const ScePthreadAttr* attr, size_t* stack_size) {

    if (stack_size == nullptr || attr == nullptr || *attr == nullptr) {
        return SCE_KERNEL_ERROR_EINVAL;
    }

    int result = pthread_attr_getstacksize(&(*attr)->pth_attr, stack_size);

    return result == 0 ? SCE_OK : SCE_KERNEL_ERROR_EINVAL;
}

int PS4_SYSV_ABI scePthreadAttrSetstackaddr(ScePthreadAttr* attr, void* addr) {

    if (addr == nullptr || attr == nullptr || *attr == nullptr) {
        return SCE_KERNEL_ERROR_EINVAL;
    }

    int result = pthread_attr_setstackaddr(&(*attr)->pth_attr, addr);

    return result == 0 ? SCE_OK : SCE_KERNEL_ERROR_EINVAL;
}

int PS4_SYSV_ABI scePthreadAttrSetstacksize(ScePthreadAttr* attr, size_t stack_size) {

    if (stack_size == 0 || attr == nullptr || *attr == nullptr) {
        return SCE_KERNEL_ERROR_EINVAL;
    }

    int result = pthread_attr_setstacksize(&(*attr)->pth_attr, stack_size);

    return result == 0 ? SCE_OK : SCE_KERNEL_ERROR_EINVAL;
}

int PS4_SYSV_ABI posix_pthread_attr_init(ScePthreadAttr* attr) {
    int result = scePthreadAttrInit(attr);
    if (result < 0) {
        int rt = result > SCE_KERNEL_ERROR_UNKNOWN && result <= SCE_KERNEL_ERROR_ESTOP
                     ? result + -SCE_KERNEL_ERROR_UNKNOWN
                     : POSIX_EOTHER;
        return rt;
    }
    return result;
}

int PS4_SYSV_ABI posix_pthread_attr_setstacksize(ScePthreadAttr* attr, size_t stacksize) {
    int result = scePthreadAttrSetstacksize(attr, stacksize);
    if (result < 0) {
        int rt = result > SCE_KERNEL_ERROR_UNKNOWN && result <= SCE_KERNEL_ERROR_ESTOP
                     ? result + -SCE_KERNEL_ERROR_UNKNOWN
                     : POSIX_EOTHER;
        return rt;
    }
    return result;
}

int PS4_SYSV_ABI scePthreadSetaffinity(ScePthread thread, const /*SceKernelCpumask*/ u64 mask) {
    LOG_INFO(Kernel_Pthread, "called");

    if (thread == nullptr) {
        return SCE_KERNEL_ERROR_ESRCH;
    }

    auto result = scePthreadAttrSetaffinity(&thread->attr, mask);

    return result;
}

ScePthreadMutex* createMutex(ScePthreadMutex* addr) {
    if (addr == nullptr || *addr != nullptr) {
        return addr;
    }
    static std::mutex mutex;
    std::scoped_lock lk{mutex};
    if (*addr != nullptr) {
        return addr;
    }
    const VAddr vaddr = reinterpret_cast<VAddr>(addr);
    std::string name = fmt::format("mutex{:#x}", vaddr);
    scePthreadMutexInit(addr, nullptr, name.c_str());
    return addr;
}

int PS4_SYSV_ABI scePthreadMutexInit(ScePthreadMutex* mutex, const ScePthreadMutexattr* attr,
                                     const char* name) {
    if (mutex == nullptr) {
        return SCE_KERNEL_ERROR_EINVAL;
    }
    if (attr == nullptr) {
        attr = g_pthread_cxt->getDefaultMutexattr();
    }

    *mutex = new PthreadMutexInternal{};
    if (name != nullptr) {
        (*mutex)->name = name;
    } else {
        (*mutex)->name = "nonameMutex";
    }

    int result = pthread_mutex_init(&(*mutex)->pth_mutex, &(*attr)->pth_mutex_attr);

    static auto mutex_loc = MUTEX_LOCATION("mutex");
    (*mutex)->tracy_lock = std::make_unique<tracy::LockableCtx>(&mutex_loc);

    if (name != nullptr) {
        (*mutex)->tracy_lock->CustomName(name, std::strlen(name));
        LOG_INFO(Kernel_Pthread, "name={}, result={}", name, result);
    }

    switch (result) {
    case 0:
        return SCE_OK;
    case EAGAIN:
        return SCE_KERNEL_ERROR_EAGAIN;
    case EINVAL:
        return SCE_KERNEL_ERROR_EINVAL;
    case ENOMEM:
        return SCE_KERNEL_ERROR_ENOMEM;
    default:
        return SCE_KERNEL_ERROR_EINVAL;
    }
}

int PS4_SYSV_ABI scePthreadMutexDestroy(ScePthreadMutex* mutex) {

    if (mutex == nullptr || *mutex == nullptr) {
        return SCE_KERNEL_ERROR_EINVAL;
    }

    int result = pthread_mutex_destroy(&(*mutex)->pth_mutex);

    LOG_INFO(Kernel_Pthread, "name={}, result={}", (*mutex)->name, result);

    delete *mutex;
    *mutex = nullptr;

    switch (result) {
    case 0:
        return SCE_OK;
    case EBUSY:
        return SCE_KERNEL_ERROR_EBUSY;
    case EINVAL:
    default:
        return SCE_KERNEL_ERROR_EINVAL;
    }
}
int PS4_SYSV_ABI scePthreadMutexattrInit(ScePthreadMutexattr* attr) {
    *attr = new PthreadMutexattrInternal{};

    int result = pthread_mutexattr_init(&(*attr)->pth_mutex_attr);

    result = (result == 0 ? scePthreadMutexattrSettype(attr, 1) : result);
    result = (result == 0 ? scePthreadMutexattrSetprotocol(attr, 0) : result);

    switch (result) {
    case 0:
        return SCE_OK;
    case ENOMEM:
        return SCE_KERNEL_ERROR_ENOMEM;
    default:
        return SCE_KERNEL_ERROR_EINVAL;
    }
}

int PS4_SYSV_ABI scePthreadMutexattrSettype(ScePthreadMutexattr* attr, int type) {
    int ptype = PTHREAD_MUTEX_DEFAULT;
    switch (type) {
    case 1:
        ptype = PTHREAD_MUTEX_ERRORCHECK;
        break;
    case 2:
        ptype = PTHREAD_MUTEX_RECURSIVE;
        break;
    case 3:
    case 4:
        ptype = PTHREAD_MUTEX_NORMAL;
        break;
    default:
        UNREACHABLE_MSG("Invalid type: {}", type);
    }

    int result = pthread_mutexattr_settype(&(*attr)->pth_mutex_attr, ptype);

    return result == 0 ? SCE_OK : SCE_KERNEL_ERROR_EINVAL;
}

int PS4_SYSV_ABI scePthreadMutexattrSetprotocol(ScePthreadMutexattr* attr, int protocol) {
    int pprotocol = PTHREAD_PRIO_NONE;
    switch (protocol) {
    case 0:
        pprotocol = PTHREAD_PRIO_NONE;
        break;
    case 1:
        pprotocol = PTHREAD_PRIO_INHERIT;
        break;
    case 2:
        pprotocol = PTHREAD_PRIO_PROTECT;
        break;
    default:
        UNREACHABLE_MSG("Invalid protocol: {}", protocol);
    }

#if _WIN64
    int result = 0;
#else
    int result = pthread_mutexattr_setprotocol(&(*attr)->pth_mutex_attr, pprotocol);
#endif
    (*attr)->pprotocol = pprotocol;
    return result == 0 ? SCE_OK : SCE_KERNEL_ERROR_EINVAL;
}

int PS4_SYSV_ABI scePthreadMutexLock(ScePthreadMutex* mutex) {
    mutex = createMutex(mutex);
    if (mutex == nullptr) {
        return SCE_KERNEL_ERROR_EINVAL;
    }

    (*mutex)->tracy_lock->BeforeLock();

    int result = pthread_mutex_lock(&(*mutex)->pth_mutex);
    if (result != 0) {
        LOG_TRACE(Kernel_Pthread, "Locked name={}, result={}", (*mutex)->name, result);
    }

    (*mutex)->tracy_lock->AfterLock();

    switch (result) {
    case 0:
        return SCE_OK;
    case EAGAIN:
        return SCE_KERNEL_ERROR_EAGAIN;
    case EINVAL:
        return SCE_KERNEL_ERROR_EINVAL;
    case EDEADLK:
        return SCE_KERNEL_ERROR_EDEADLK;
    default:
        return SCE_KERNEL_ERROR_EINVAL;
    }
}

int PS4_SYSV_ABI scePthreadMutexUnlock(ScePthreadMutex* mutex) {
    mutex = createMutex(mutex);
    if (mutex == nullptr) {
        return SCE_KERNEL_ERROR_EINVAL;
    }

    int result = pthread_mutex_unlock(&(*mutex)->pth_mutex);
    if (result != 0) {
        LOG_TRACE(Kernel_Pthread, "Unlocking name={}, result={}", (*mutex)->name, result);
    }

    (*mutex)->tracy_lock->AfterUnlock();

    switch (result) {
    case 0:
        return SCE_OK;
    case EINVAL:
        return SCE_KERNEL_ERROR_EINVAL;
    case EPERM:
        return SCE_KERNEL_ERROR_EPERM;
    default:
        return SCE_KERNEL_ERROR_EINVAL;
    }
}

int PS4_SYSV_ABI scePthreadMutexattrDestroy(ScePthreadMutexattr* attr) {
    int result = pthread_mutexattr_destroy(&(*attr)->pth_mutex_attr);

    delete *attr;
    *attr = nullptr;

    switch (result) {
    case 0:
        return SCE_OK;
    case ENOMEM:
        return SCE_KERNEL_ERROR_ENOMEM;
    default:
        return SCE_KERNEL_ERROR_EINVAL;
    }
}

ScePthreadCond* createCond(ScePthreadCond* addr) {
    if (addr == nullptr || *addr != nullptr) {
        return addr;
    }
    static std::mutex mutex;
    std::scoped_lock lk{mutex};
    if (*addr != nullptr) {
        return addr;
    }
    const VAddr vaddr = reinterpret_cast<VAddr>(addr);
    std::string name = fmt::format("cond{:#x}", vaddr);
    scePthreadCondInit(static_cast<ScePthreadCond*>(addr), nullptr, name.c_str());
    return addr;
}

int PS4_SYSV_ABI scePthreadCondInit(ScePthreadCond* cond, const ScePthreadCondattr* attr,
                                    const char* name) {
    if (cond == nullptr) {
        return SCE_KERNEL_ERROR_EINVAL;
    }

    if (attr == nullptr) {
        attr = g_pthread_cxt->getDefaultCondattr();
    }

    *cond = new PthreadCondInternal{};

    if (name != nullptr) {
        (*cond)->name = name;
    } else {
        (*cond)->name = "nonameCond";
    }

    int result = pthread_cond_init(&(*cond)->cond, &(*attr)->cond_attr);

    if (name != nullptr) {
        LOG_INFO(Kernel_Pthread, "name={}, result={}", (*cond)->name, result);
    }

    switch (result) {
    case 0:
        return SCE_OK;
    case EAGAIN:
        return SCE_KERNEL_ERROR_EAGAIN;
    case EINVAL:
        return SCE_KERNEL_ERROR_EINVAL;
    case ENOMEM:
        return SCE_KERNEL_ERROR_ENOMEM;
    default:
        return SCE_KERNEL_ERROR_EINVAL;
    }
}

int PS4_SYSV_ABI scePthreadCondattrInit(ScePthreadCondattr* attr) {
    *attr = new PthreadCondAttrInternal{};

    int result = pthread_condattr_init(&(*attr)->cond_attr);

    switch (result) {
    case 0:
        return SCE_OK;
    case ENOMEM:
        return SCE_KERNEL_ERROR_ENOMEM;
    default:
        return SCE_KERNEL_ERROR_EINVAL;
    }
}

int PS4_SYSV_ABI scePthreadCondBroadcast(ScePthreadCond* cond) {
    cond = createCond(cond);
    if (cond == nullptr) {
        return SCE_KERNEL_ERROR_EINVAL;
    }

    int result = pthread_cond_broadcast(&(*cond)->cond);

    LOG_TRACE(Kernel_Pthread, "called name={}, result={}", (*cond)->name, result);

    return (result == 0 ? SCE_OK : SCE_KERNEL_ERROR_EINVAL);
}

int PS4_SYSV_ABI scePthreadCondTimedwait(ScePthreadCond* cond, ScePthreadMutex* mutex, u64 usec) {
    cond = createCond(cond);
    if (cond == nullptr) {
        return SCE_KERNEL_ERROR_EINVAL;
    }
    if (mutex == nullptr || *mutex == nullptr) {
        return SCE_KERNEL_ERROR_EINVAL;
    }
    timespec time{};
    time.tv_sec = usec / 1000000;
    time.tv_nsec = ((usec % 1000000) * 1000);
    int result = pthread_cond_timedwait(&(*cond)->cond, &(*mutex)->pth_mutex, &time);

    // LOG_INFO(Kernel_Pthread, "scePthreadCondTimedwait, result={}", result);

    switch (result) {
    case 0:
        return SCE_OK;
    case ETIMEDOUT:
        return SCE_KERNEL_ERROR_ETIMEDOUT;
    case EINTR:
        return SCE_KERNEL_ERROR_EINTR;
    case EAGAIN:
        return SCE_KERNEL_ERROR_EAGAIN;
    default:
        return SCE_KERNEL_ERROR_EINVAL;
    }
}

int PS4_SYSV_ABI scePthreadCondDestroy(ScePthreadCond* cond) {
    if (cond == nullptr) {
        return SCE_KERNEL_ERROR_EINVAL;
    }
    int result = pthread_cond_destroy(&(*cond)->cond);

    LOG_INFO(Kernel_Pthread, "scePthreadCondDestroy, result={}", result);

    switch (result) {
    case 0:
        return SCE_OK;
    case EBUSY:
        return SCE_KERNEL_ERROR_EBUSY;
    default:
        return SCE_KERNEL_ERROR_EINVAL;
    }
}

int PS4_SYSV_ABI posix_pthread_mutex_init(ScePthreadMutex* mutex, const ScePthreadMutexattr* attr) {
    // LOG_INFO(Kernel_Pthread, "posix pthread_mutex_init redirect to scePthreadMutexInit");
    int result = scePthreadMutexInit(mutex, attr, nullptr);
    if (result < 0) {
        int rt = result > SCE_KERNEL_ERROR_UNKNOWN && result <= SCE_KERNEL_ERROR_ESTOP
                     ? result + -SCE_KERNEL_ERROR_UNKNOWN
                     : POSIX_EOTHER;
        return rt;
    }
    return result;
}

int PS4_SYSV_ABI posix_pthread_mutex_lock(ScePthreadMutex* mutex) {
    // LOG_INFO(Kernel_Pthread, "posix pthread_mutex_lock redirect to scePthreadMutexLock");
    int result = scePthreadMutexLock(mutex);
    if (result < 0) {
        int rt = result > SCE_KERNEL_ERROR_UNKNOWN && result <= SCE_KERNEL_ERROR_ESTOP
                     ? result + -SCE_KERNEL_ERROR_UNKNOWN
                     : POSIX_EOTHER;
        return rt;
    }
    return result;
}

int PS4_SYSV_ABI posix_pthread_mutex_unlock(ScePthreadMutex* mutex) {
    // LOG_INFO(Kernel_Pthread, "posix pthread_mutex_unlock redirect to scePthreadMutexUnlock");
    int result = scePthreadMutexUnlock(mutex);
    if (result < 0) {
        int rt = result > SCE_KERNEL_ERROR_UNKNOWN && result <= SCE_KERNEL_ERROR_ESTOP
                     ? result + -SCE_KERNEL_ERROR_UNKNOWN
                     : POSIX_EOTHER;
        return rt;
    }
    return result;
}

int PS4_SYSV_ABI posix_pthread_mutex_destroy(ScePthreadMutex* mutex) {
    int result = scePthreadMutexDestroy(mutex);
    if (result < 0) {
        int rt = result > SCE_KERNEL_ERROR_UNKNOWN && result <= SCE_KERNEL_ERROR_ESTOP
                     ? result + -SCE_KERNEL_ERROR_UNKNOWN
                     : POSIX_EOTHER;
        return rt;
    }
    return result;
}

int PS4_SYSV_ABI posix_pthread_cond_wait(ScePthreadCond* cond, ScePthreadMutex* mutex) {
    int result = scePthreadCondWait(cond, mutex);
    if (result < 0) {
        UNREACHABLE();
    }
    return result;
}

int PS4_SYSV_ABI posix_pthread_cond_broadcast(ScePthreadCond* cond) {
    LOG_INFO(Kernel_Pthread,
             "posix posix_pthread_cond_broadcast redirect to scePthreadCondBroadcast");
    int result = scePthreadCondBroadcast(cond);
    if (result != 0) {
        int rt = result > SCE_KERNEL_ERROR_UNKNOWN && result <= SCE_KERNEL_ERROR_ESTOP
                     ? result + -SCE_KERNEL_ERROR_UNKNOWN
                     : POSIX_EOTHER;
        return rt;
    }
    return result;
}

int PS4_SYSV_ABI posix_pthread_mutexattr_init(ScePthreadMutexattr* attr) {
    // LOG_INFO(Kernel_Pthread, "posix pthread_mutexattr_init redirect to scePthreadMutexattrInit");
    int result = scePthreadMutexattrInit(attr);
    if (result < 0) {
        int rt = result > SCE_KERNEL_ERROR_UNKNOWN && result <= SCE_KERNEL_ERROR_ESTOP
                     ? result + -SCE_KERNEL_ERROR_UNKNOWN
                     : POSIX_EOTHER;
        return rt;
    }
    return result;
}

int PS4_SYSV_ABI posix_pthread_mutexattr_settype(ScePthreadMutexattr* attr, int type) {
    // LOG_INFO(Kernel_Pthread, "posix pthread_mutex_init redirect to scePthreadMutexInit");
    int result = scePthreadMutexattrSettype(attr, type);
    if (result < 0) {
        int rt = result > SCE_KERNEL_ERROR_UNKNOWN && result <= SCE_KERNEL_ERROR_ESTOP
                     ? result + -SCE_KERNEL_ERROR_UNKNOWN
                     : POSIX_EOTHER;
        return rt;
    }
    return result;
}

int PS4_SYSV_ABI posix_pthread_mutexattr_destroy(ScePthreadMutexattr* attr) {
    int result = scePthreadMutexattrDestroy(attr);
    if (result < 0) {
        UNREACHABLE();
    }
    return result;
}

int PS4_SYSV_ABI posix_pthread_mutexattr_setprotocol(ScePthreadMutexattr* attr, int protocol) {
    int result = scePthreadMutexattrSetprotocol(attr, protocol);
    LOG_INFO(Kernel_Pthread, "redirect to scePthreadMutexattrSetprotocol: result = {}", result);
    if (result < 0) {
        UNREACHABLE();
    }
    return result;
}

static int pthread_copy_attributes(ScePthreadAttr* dst, const ScePthreadAttr* src) {
    if (dst == nullptr || *dst == nullptr || src == nullptr || *src == nullptr) {
        return SCE_KERNEL_ERROR_EINVAL;
    }

    u64 mask = 0;
    int state = 0;
    size_t guard_size = 0;
    int inherit_sched = 0;
    SceKernelSchedParam param = {};
    int policy = 0;
    void* stack_addr = nullptr;
    size_t stack_size = 0;

    int result = 0;

    result = (result == 0 ? scePthreadAttrGetaffinity(src, &mask) : result);
    result = (result == 0 ? scePthreadAttrGetdetachstate(src, &state) : result);
    result = (result == 0 ? scePthreadAttrGetguardsize(src, &guard_size) : result);
    result = (result == 0 ? scePthreadAttrGetinheritsched(src, &inherit_sched) : result);
    result = (result == 0 ? scePthreadAttrGetschedparam(src, &param) : result);
    result = (result == 0 ? scePthreadAttrGetschedpolicy(src, &policy) : result);
    result = (result == 0 ? scePthreadAttrGetstackaddr(src, &stack_addr) : result);
    result = (result == 0 ? scePthreadAttrGetstacksize(src, &stack_size) : result);

    result = (result == 0 ? scePthreadAttrSetaffinity(dst, mask) : result);
    result = (result == 0 ? scePthreadAttrSetdetachstate(dst, state) : result);
    result = (result == 0 ? scePthreadAttrSetguardsize(dst, guard_size) : result);
    result = (result == 0 ? scePthreadAttrSetinheritsched(dst, inherit_sched) : result);
    result = (result == 0 ? scePthreadAttrSetschedparam(dst, &param) : result);
    result = (result == 0 ? scePthreadAttrSetschedpolicy(dst, policy) : result);
    if (stack_addr != nullptr) {
        result = (result == 0 ? scePthreadAttrSetstackaddr(dst, stack_addr) : result);
    }
    if (stack_size != 0) {
        result = (result == 0 ? scePthreadAttrSetstacksize(dst, stack_size) : result);
    }

    return result;
}

int PS4_SYSV_ABI scePthreadAttrGet(ScePthread thread, ScePthreadAttr* attr) {
    if (thread == nullptr || attr == nullptr || *attr == nullptr) {
        return SCE_KERNEL_ERROR_EINVAL;
    }

    return pthread_copy_attributes(attr, &thread->attr);
}

static void cleanup_thread(void* arg) {
    auto* thread = static_cast<ScePthread>(arg);
    for (const auto& [key, destructor] : thread->key_destructors) {
        if (void* value = pthread_getspecific(key); value != nullptr) {
            destructor(value);
        }
    }
    thread->is_almost_done = true;
}

static void* run_thread(void* arg) {
    auto* thread = static_cast<ScePthread>(arg);
    Common::SetCurrentThreadName(thread->name.c_str());
    auto* linker = Common::Singleton<Core::Linker>::Instance();
    linker->InitTlsForThread(false);
    void* ret = nullptr;
    g_pthread_self = thread;
    pthread_cleanup_push(cleanup_thread, thread);
    thread->is_started = true;
    ret = thread->entry(thread->arg);
    pthread_cleanup_pop(1);
    return ret;
}

int PS4_SYSV_ABI scePthreadCreate(ScePthread* thread, const ScePthreadAttr* attr,
                                  PthreadEntryFunc start_routine, void* arg, const char* name) {
    if (thread == nullptr) {
        return SCE_KERNEL_ERROR_EINVAL;
    }

    auto* pthread_pool = g_pthread_cxt->GetPthreadPool();

    if (attr == nullptr) {
        attr = g_pthread_cxt->GetDefaultAttr();
    }

    *thread = pthread_pool->Create();

    if ((*thread)->attr != nullptr) {
        scePthreadAttrDestroy(&(*thread)->attr);
    }
    scePthreadAttrInit(&(*thread)->attr);

    int result = pthread_copy_attributes(&(*thread)->attr, attr);
    ASSERT(result == 0);

    if (name != NULL) {
        (*thread)->name = name;
    } else {
        (*thread)->name = "no-name";
    }
    (*thread)->entry = start_routine;
    (*thread)->arg = arg;
    (*thread)->is_almost_done = false;
    (*thread)->is_detached = (*attr)->detached;
    (*thread)->is_started = false;

    pthread_attr_setstacksize(&(*attr)->pth_attr, 2_MB);
    result = pthread_create(&(*thread)->pth, &(*attr)->pth_attr, run_thread, *thread);

    LOG_INFO(Kernel_Pthread, "thread create name = {}", (*thread)->name);

    switch (result) {
    case 0:
        return SCE_OK;
    case ENOMEM:
        return SCE_KERNEL_ERROR_ENOMEM;
    case EAGAIN:
        return SCE_KERNEL_ERROR_EAGAIN;
    case EDEADLK:
        return SCE_KERNEL_ERROR_EDEADLK;
    case EPERM:
        return SCE_KERNEL_ERROR_EPERM;
    default:
        return SCE_KERNEL_ERROR_EINVAL;
    }
}

ScePthread PThreadPool::Create() {
    std::scoped_lock lock{m_mutex};

    for (auto* p : m_threads) {
        if (p->is_free) {
            p->is_free = false;
            return p;
        }
    }

#ifdef _WIN64
    auto* ret = new PthreadInternal{};
#else
    // TODO: Linux specific hack
    static u8* hint_address = reinterpret_cast<u8*>(0x7FFFFC000ULL);
    auto* ret = reinterpret_cast<PthreadInternal*>(
        mmap(hint_address, sizeof(PthreadInternal), PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0));
    hint_address += Common::AlignUp(sizeof(PthreadInternal), 4_KB);
#endif

    ret->is_free = false;
    ret->is_detached = false;
    ret->is_almost_done = false;
    ret->attr = nullptr;

    m_threads.push_back(ret);

    return ret;
}

void PS4_SYSV_ABI scePthreadYield() {
    sched_yield();
}

void PS4_SYSV_ABI posix_pthread_yield() {
    sched_yield();
}

int PS4_SYSV_ABI scePthreadAttrGetstack(ScePthreadAttr* attr, void** addr, size_t* size) {

    int result = pthread_attr_getstack(&(*attr)->pth_attr, addr, size);
    LOG_INFO(Kernel_Pthread, "scePthreadAttrGetstack: result = {}", result);

    if (result == 0) {
        return SCE_OK;
    }
    return SCE_KERNEL_ERROR_EINVAL;
}

int PS4_SYSV_ABI scePthreadJoin(ScePthread thread, void** res) {
    int result = pthread_join(thread->pth, res);
    LOG_INFO(Kernel_Pthread, "scePthreadJoin result = {}", result);
    thread->is_detached = false;
    return ORBIS_OK;
}

int PS4_SYSV_ABI posix_pthread_join(ScePthread thread, void** res) {
    int result = pthread_join(thread->pth, res);
    LOG_INFO(Kernel_Pthread, "posix_pthread_join result = {}", result);
    thread->is_detached = false;
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePthreadDetach(ScePthread thread) {
    LOG_INFO(Kernel_Pthread, "thread create name = {}", thread->name);
    thread->is_detached = true;
    return ORBIS_OK;
}

ScePthread PS4_SYSV_ABI posix_pthread_self() {
    return g_pthread_self;
}

int PS4_SYSV_ABI scePthreadCondSignal(ScePthreadCond* cond) {
    if (cond == nullptr) {
        return SCE_KERNEL_ERROR_EINVAL;
    }

    int result = pthread_cond_signal(&(*cond)->cond);

    // LOG_INFO(Kernel_Pthread, "scePthreadCondSignal, result={}", result);

    switch (result) {
    case 0:
        return SCE_OK;
    case EBUSY:
        return SCE_KERNEL_ERROR_EBUSY;
    default:
        return SCE_KERNEL_ERROR_EINVAL;
    }
}

int PS4_SYSV_ABI scePthreadCondWait(ScePthreadCond* cond, ScePthreadMutex* mutex) {
    cond = createCond(cond);
    if (cond == nullptr) {
        return SCE_KERNEL_ERROR_EINVAL;
    }
    if (mutex == nullptr || *mutex == nullptr) {
        return SCE_KERNEL_ERROR_EINVAL;
    }
    int result = pthread_cond_wait(&(*cond)->cond, &(*mutex)->pth_mutex);

    LOG_INFO(Kernel_Pthread, "scePthreadCondWait, result={}", result);

    switch (result) {
    case 0:
        return SCE_OK;
    case EINTR:
        return SCE_KERNEL_ERROR_EINTR;
    case EAGAIN:
        return SCE_KERNEL_ERROR_EAGAIN;
    default:
        return SCE_KERNEL_ERROR_EINVAL;
    }
}

int PS4_SYSV_ABI scePthreadCondattrDestroy(ScePthreadCondattr* attr) {
    if (attr == nullptr) {
        return SCE_KERNEL_ERROR_EINVAL;
    }
    int result = pthread_condattr_destroy(&(*attr)->cond_attr);

    LOG_INFO(Kernel_Pthread, "scePthreadCondattrDestroy: result = {} ", result);

    switch (result) {
    case 0:
        return SCE_OK;
    case ENOMEM:
        return SCE_KERNEL_ERROR_ENOMEM;
    default:
        return SCE_KERNEL_ERROR_EINVAL;
    }
}

int PS4_SYSV_ABI scePthreadMutexTrylock(ScePthreadMutex* mutex) {
    mutex = createMutex(mutex);
    if (mutex == nullptr) {
        return ORBIS_KERNEL_ERROR_EINVAL;
    }

    int result = pthread_mutex_trylock(&(*mutex)->pth_mutex);
    if (result != 0) {
        LOG_TRACE(Kernel_Pthread, "name={}, result={}", (*mutex)->name, result);
    }

    (*mutex)->tracy_lock->AfterTryLock(result == 0);

    switch (result) {
    case 0:
        return ORBIS_OK;
    case EAGAIN:
        return ORBIS_KERNEL_ERROR_EAGAIN;
    case EBUSY:
        return ORBIS_KERNEL_ERROR_EBUSY;
    case EINVAL:
    default:
        return ORBIS_KERNEL_ERROR_EINVAL;
    }
}

int PS4_SYSV_ABI scePthreadEqual(ScePthread thread1, ScePthread thread2) {
    return (thread1 == thread2 ? 1 : 0);
}

int PS4_SYSV_ABI posix_pthread_equal(ScePthread thread1, ScePthread thread2) {
    return (thread1 == thread2 ? 1 : 0);
}

struct TlsIndex {
    u64 ti_module;
    u64 ti_offset;
};

void* PS4_SYSV_ABI __tls_get_addr(TlsIndex* index) {
    auto* linker = Common::Singleton<Core::Linker>::Instance();
    return linker->TlsGetAddr(index->ti_module, index->ti_offset);
}

int PS4_SYSV_ABI posix_sched_get_priority_max() {
    return ORBIS_KERNEL_PRIO_FIFO_HIGHEST;
}

int PS4_SYSV_ABI posix_sched_get_priority_min() {
    return ORBIS_KERNEL_PRIO_FIFO_LOWEST;
}

int PS4_SYSV_ABI posix_pthread_mutex_trylock(ScePthreadMutex* mutex) {
    int result = scePthreadMutexTrylock(mutex);
    if (result < 0) {
        // UNREACHABLE();
    }
    return result;
}

int PS4_SYSV_ABI posix_pthread_attr_destroy(ScePthreadAttr* attr) {
    int result = scePthreadAttrDestroy(attr);
    if (result < 0) {
        int rt = result > SCE_KERNEL_ERROR_UNKNOWN && result <= SCE_KERNEL_ERROR_ESTOP
                     ? result + -SCE_KERNEL_ERROR_UNKNOWN
                     : POSIX_EOTHER;
        return rt;
    }
    return result;
}

int PS4_SYSV_ABI posix_pthread_attr_setdetachstate(ScePthreadAttr* attr, int detachstate) {
    // LOG_INFO(Kernel_Pthread, "posix pthread_mutexattr_init redirect to scePthreadMutexattrInit");
    int result = scePthreadAttrSetdetachstate(attr, detachstate);
    if (result < 0) {
        int rt = result > SCE_KERNEL_ERROR_UNKNOWN && result <= SCE_KERNEL_ERROR_ESTOP
                     ? result + -SCE_KERNEL_ERROR_UNKNOWN
                     : POSIX_EOTHER;
        return rt;
    }
    return result;
}

int PS4_SYSV_ABI posix_pthread_create_name_np(ScePthread* thread, const ScePthreadAttr* attr,
                                              PthreadEntryFunc start_routine, void* arg,
                                              const char* name) {
    LOG_INFO(Kernel_Pthread, "posix pthread_create redirect to scePthreadCreate: name = {}", name);

    int result = scePthreadCreate(thread, attr, start_routine, arg, name);
    if (result != 0) {
        int rt = result > SCE_KERNEL_ERROR_UNKNOWN && result <= SCE_KERNEL_ERROR_ESTOP
                     ? result + -SCE_KERNEL_ERROR_UNKNOWN
                     : POSIX_EOTHER;
        return rt;
    }
    return result;
}

int PS4_SYSV_ABI posix_pthread_create(ScePthread* thread, const ScePthreadAttr* attr,
                                      PthreadEntryFunc start_routine, void* arg) {
    return posix_pthread_create_name_np(thread, attr, start_routine, arg, "NoName");
}

using Destructor = void (*)(void*);

int PS4_SYSV_ABI posix_pthread_key_create(u32* key, Destructor func) {
    return pthread_key_create(key, func);
}

int PS4_SYSV_ABI posix_pthread_setspecific(int key, const void* value) {
    return pthread_setspecific(key, value);
}

void* PS4_SYSV_ABI posix_pthread_getspecific(int key) {
    return pthread_getspecific(key);
}

int PS4_SYSV_ABI posix_pthread_cond_init(ScePthreadCond* cond, const ScePthreadCondattr* attr) {
    // LOG_INFO(Kernel_Pthread, "posix pthread_mutex_init redirect to scePthreadMutexInit");
    int result = scePthreadCondInit(cond, attr, "NoName");
    if (result < 0) {
        int rt = result > SCE_KERNEL_ERROR_UNKNOWN && result <= SCE_KERNEL_ERROR_ESTOP
                     ? result + -SCE_KERNEL_ERROR_UNKNOWN
                     : POSIX_EOTHER;
        return rt;
    }
    return result;
}

int PS4_SYSV_ABI posix_pthread_cond_signal(ScePthreadCond* cond) {
    int result = scePthreadCondSignal(cond);
    LOG_INFO(Kernel_Pthread,
             "posix posix_pthread_cond_signal redirect to scePthreadCondSignal, result = {}",
             result);
    return result;
}

int PS4_SYSV_ABI posix_pthread_cond_destroy(ScePthreadCond* cond) {
    int result = scePthreadCondDestroy(cond);
    LOG_INFO(Kernel_Pthread,
             "posix posix_pthread_cond_destroy redirect to scePthreadCondDestroy, result = {}",
             result);
    return result;
}

int PS4_SYSV_ABI posix_pthread_setcancelstate(int state, int* oldstate) {
    return pthread_setcancelstate(state, oldstate);
}

int PS4_SYSV_ABI posix_pthread_detach(ScePthread thread) {
    return pthread_detach(thread->pth);
}

int PS4_SYSV_ABI posix_sem_init(sem_t* sem, int pshared, unsigned int value) {
    return sem_init(sem, pshared, value);
}

int PS4_SYSV_ABI posix_sem_wait(sem_t* sem) {
    return sem_wait(sem);
}

int PS4_SYSV_ABI posix_sem_post(sem_t* sem) {
    return sem_post(sem);
}

int PS4_SYSV_ABI posix_sem_getvalue(sem_t* sem, int* sval) {
    return sem_getvalue(sem, sval);
}

int PS4_SYSV_ABI scePthreadGetschedparam(ScePthread thread, int* policy,
                                         SceKernelSchedParam* param) {
    return pthread_getschedparam(thread->pth, policy, param);
}

int PS4_SYSV_ABI scePthreadSetschedparam(ScePthread thread, int policy,
                                         const SceKernelSchedParam* param) {
    LOG_ERROR(Kernel_Pthread, "(STUBBED) called policy={}, sched_priority={}", policy,
              param->sched_priority);
    return ORBIS_OK;
}

int PS4_SYSV_ABI scePthreadOnce(int* once_control, void (*init_routine)(void)) {
    return pthread_once(reinterpret_cast<pthread_once_t*>(once_control), init_routine);
}

[[noreturn]] void PS4_SYSV_ABI scePthreadExit(void* value_ptr) {
    pthread_exit(value_ptr);
    UNREACHABLE();
}

void pthreadSymbolsRegister(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("lZzFeSxPl08", "libScePosix", 1, "libkernel", 1, 1, posix_pthread_setcancelstate);
    LIB_FUNCTION("0TyVk4MSLt0", "libScePosix", 1, "libkernel", 1, 1, posix_pthread_cond_init);
    LIB_FUNCTION("2MOy+rUfuhQ", "libScePosix", 1, "libkernel", 1, 1, posix_pthread_cond_signal);
    LIB_FUNCTION("RXXqi4CtF8w", "libScePosix", 1, "libkernel", 1, 1, posix_pthread_cond_destroy);
    LIB_FUNCTION("mqULNdimTn0", "libScePosix", 1, "libkernel", 1, 1, posix_pthread_key_create);
    LIB_FUNCTION("0-KXaS70xy4", "libScePosix", 1, "libkernel", 1, 1, posix_pthread_getspecific);
    LIB_FUNCTION("WrOLvHU0yQM", "libScePosix", 1, "libkernel", 1, 1, posix_pthread_setspecific);
    LIB_FUNCTION("4+h9EzwKF4I", "libkernel", 1, "libkernel", 1, 1, scePthreadAttrSetschedpolicy);
    LIB_FUNCTION("-Wreprtu0Qs", "libkernel", 1, "libkernel", 1, 1, scePthreadAttrSetdetachstate);
    LIB_FUNCTION("eXbUSpEaTsA", "libkernel", 1, "libkernel", 1, 1, scePthreadAttrSetinheritsched);
    LIB_FUNCTION("DzES9hQF4f4", "libkernel", 1, "libkernel", 1, 1, scePthreadAttrSetschedparam);
    LIB_FUNCTION("nsYoNRywwNg", "libkernel", 1, "libkernel", 1, 1, scePthreadAttrInit);
    LIB_FUNCTION("62KCwEMmzcM", "libkernel", 1, "libkernel", 1, 1, scePthreadAttrDestroy);
    LIB_FUNCTION("onNY9Byn-W8", "libkernel", 1, "libkernel", 1, 1, scePthreadJoin);
    LIB_FUNCTION("4qGrR6eoP9Y", "libkernel", 1, "libkernel", 1, 1, scePthreadDetach);
    LIB_FUNCTION("3PtV6p3QNX4", "libkernel", 1, "libkernel", 1, 1, scePthreadEqual);
    LIB_FUNCTION("3kg7rT0NQIs", "libkernel", 1, "libkernel", 1, 1, scePthreadExit);
    LIB_FUNCTION("7Xl257M4VNI", "libScePosix", 1, "libkernel", 1, 1, posix_pthread_equal);
    LIB_FUNCTION("h9CcP3J0oVM", "libScePosix", 1, "libkernel", 1, 1, posix_pthread_join);

    LIB_FUNCTION("aI+OeCz8xrQ", "libkernel", 1, "libkernel", 1, 1, scePthreadSelf);
    LIB_FUNCTION("EotR8a3ASf4", "libkernel", 1, "libkernel", 1, 1, posix_pthread_self);
    LIB_FUNCTION("EotR8a3ASf4", "libScePosix", 1, "libkernel", 1, 1, posix_pthread_self);
    LIB_FUNCTION("3qxgM4ezETA", "libkernel", 1, "libkernel", 1, 1, scePthreadAttrSetaffinity);
    LIB_FUNCTION("8+s5BzZjxSg", "libkernel", 1, "libkernel", 1, 1, scePthreadAttrGetaffinity);
    LIB_FUNCTION("x1X76arYMxU", "libkernel", 1, "libkernel", 1, 1, scePthreadAttrGet);
    LIB_FUNCTION("FXPWHNk8Of0", "libkernel", 1, "libkernel", 1, 1, scePthreadAttrGetschedparam);
    LIB_FUNCTION("P41kTWUS3EI", "libkernel", 1, "libkernel", 1, 1, scePthreadGetschedparam);
    LIB_FUNCTION("oIRFTjoILbg", "libkernel", 1, "libkernel", 1, 1, scePthreadSetschedparam);
    LIB_FUNCTION("UTXzJbWhhTE", "libkernel", 1, "libkernel", 1, 1, scePthreadAttrSetstacksize);
    LIB_FUNCTION("vNe1w4diLCs", "libkernel", 1, "libkernel", 1, 1, __tls_get_addr);
    LIB_FUNCTION("OxhIB8LB-PQ", "libkernel", 1, "libkernel", 1, 1, posix_pthread_create);
    LIB_FUNCTION("OxhIB8LB-PQ", "libScePosix", 1, "libkernel", 1, 1, posix_pthread_create);
    LIB_FUNCTION("bt3CTBKmGyI", "libkernel", 1, "libkernel", 1, 1, scePthreadSetaffinity);
    LIB_FUNCTION("6UgtwV+0zb4", "libkernel", 1, "libkernel", 1, 1, scePthreadCreate);
    LIB_FUNCTION("T72hz6ffq08", "libkernel", 1, "libkernel", 1, 1, scePthreadYield);
    LIB_FUNCTION("B5GmVDKwpn0", "libScePosix", 1, "libkernel", 1, 1, posix_pthread_yield);

    LIB_FUNCTION("-quPa4SEJUw", "libkernel", 1, "libkernel", 1, 1, scePthreadAttrGetstack);
    LIB_FUNCTION("Ru36fiTtJzA", "libkernel", 1, "libkernel", 1, 1, scePthreadAttrGetstackaddr);
    LIB_FUNCTION("-fA+7ZlGDQs", "libkernel", 1, "libkernel", 1, 1, scePthreadAttrGetstacksize);
    LIB_FUNCTION("14bOACANTBo", "libkernel", 1, "libkernel", 1, 1, scePthreadOnce);

    // mutex calls
    LIB_FUNCTION("cmo1RIYva9o", "libkernel", 1, "libkernel", 1, 1, scePthreadMutexInit);
    LIB_FUNCTION("2Of0f+3mhhE", "libkernel", 1, "libkernel", 1, 1, scePthreadMutexDestroy);
    LIB_FUNCTION("F8bUHwAG284", "libkernel", 1, "libkernel", 1, 1, scePthreadMutexattrInit);
    LIB_FUNCTION("smWEktiyyG0", "libkernel", 1, "libkernel", 1, 1, scePthreadMutexattrDestroy);
    LIB_FUNCTION("iMp8QpE+XO4", "libkernel", 1, "libkernel", 1, 1, scePthreadMutexattrSettype);
    LIB_FUNCTION("1FGvU0i9saQ", "libkernel", 1, "libkernel", 1, 1, scePthreadMutexattrSetprotocol);
    LIB_FUNCTION("9UK1vLZQft4", "libkernel", 1, "libkernel", 1, 1, scePthreadMutexLock);
    LIB_FUNCTION("tn3VlD0hG60", "libkernel", 1, "libkernel", 1, 1, scePthreadMutexUnlock);
    LIB_FUNCTION("upoVrzMHFeE", "libkernel", 1, "libkernel", 1, 1, scePthreadMutexTrylock);
    // cond calls
    LIB_FUNCTION("2Tb92quprl0", "libkernel", 1, "libkernel", 1, 1, scePthreadCondInit);
    LIB_FUNCTION("m5-2bsNfv7s", "libkernel", 1, "libkernel", 1, 1, scePthreadCondattrInit);
    LIB_FUNCTION("JGgj7Uvrl+A", "libkernel", 1, "libkernel", 1, 1, scePthreadCondBroadcast);
    LIB_FUNCTION("WKAXJ4XBPQ4", "libkernel", 1, "libkernel", 1, 1, scePthreadCondWait);
    LIB_FUNCTION("waPcxYiR3WA", "libkernel", 1, "libkernel", 1, 1, scePthreadCondattrDestroy);
    LIB_FUNCTION("kDh-NfxgMtE", "libkernel", 1, "libkernel", 1, 1, scePthreadCondSignal);
    LIB_FUNCTION("BmMjYxmew1w", "libkernel", 1, "libkernel", 1, 1, scePthreadCondTimedwait);
    LIB_FUNCTION("g+PZd2hiacg", "libkernel", 1, "libkernel", 1, 1, scePthreadCondDestroy);

    // posix calls
    LIB_FUNCTION("wtkt-teR1so", "libScePosix", 1, "libkernel", 1, 1, posix_pthread_attr_init);
    LIB_FUNCTION("2Q0z6rnBrTE", "libScePosix", 1, "libkernel", 1, 1,
                 posix_pthread_attr_setstacksize);
    LIB_FUNCTION("ttHNfU+qDBU", "libScePosix", 1, "libkernel", 1, 1, posix_pthread_mutex_init);
    LIB_FUNCTION("7H0iTOciTLo", "libScePosix", 1, "libkernel", 1, 1, posix_pthread_mutex_lock);
    LIB_FUNCTION("2Z+PpY6CaJg", "libScePosix", 1, "libkernel", 1, 1, posix_pthread_mutex_unlock);
    LIB_FUNCTION("ltCfaGr2JGE", "libScePosix", 1, "libkernel", 1, 1, posix_pthread_mutex_destroy);
    LIB_FUNCTION("Op8TBGY5KHg", "libkernel", 1, "libkernel", 1, 1, posix_pthread_cond_wait);
    LIB_FUNCTION("Op8TBGY5KHg", "libScePosix", 1, "libkernel", 1, 1, posix_pthread_cond_wait);
    LIB_FUNCTION("mkx2fVhNMsg", "libScePosix", 1, "libkernel", 1, 1, posix_pthread_cond_broadcast);
    LIB_FUNCTION("dQHWEsJtoE4", "libScePosix", 1, "libkernel", 1, 1, posix_pthread_mutexattr_init);
    LIB_FUNCTION("mDmgMOGVUqg", "libScePosix", 1, "libkernel", 1, 1,
                 posix_pthread_mutexattr_settype);
    LIB_FUNCTION("5txKfcMUAok", "libScePosix", 1, "libkernel", 1, 1,
                 posix_pthread_mutexattr_setprotocol);
    LIB_FUNCTION("HF7lK46xzjY", "libScePosix", 1, "libkernel", 1, 1,
                 posix_pthread_mutexattr_destroy);

    // openorbis weird functions
    LIB_FUNCTION("7H0iTOciTLo", "libkernel", 1, "libkernel", 1, 1, posix_pthread_mutex_lock);
    LIB_FUNCTION("2Z+PpY6CaJg", "libkernel", 1, "libkernel", 1, 1, posix_pthread_mutex_unlock);
    LIB_FUNCTION("mkx2fVhNMsg", "libkernel", 1, "libkernel", 1, 1, posix_pthread_cond_broadcast);
    LIB_FUNCTION("K-jXhbt2gn4", "libScePosix", 1, "libkernel", 1, 1, posix_pthread_mutex_trylock);
    LIB_FUNCTION("E+tyo3lp5Lw", "libScePosix", 1, "libkernel", 1, 1,
                 posix_pthread_attr_setdetachstate);
    LIB_FUNCTION("zHchY8ft5pk", "libScePosix", 1, "libkernel", 1, 1, posix_pthread_attr_destroy);
    LIB_FUNCTION("Jmi+9w9u0E4", "libScePosix", 1, "libkernel", 1, 1, posix_pthread_create_name_np);
    LIB_FUNCTION("OxhIB8LB-PQ", "libScePosix", 1, "libkernel", 1, 1, posix_pthread_create);
    LIB_FUNCTION("+U1R4WtXvoc", "libScePosix", 1, "libkernel", 1, 1, posix_pthread_detach);
    LIB_FUNCTION("CBNtXOoef-E", "libScePosix", 1, "libkernel", 1, 1, posix_sched_get_priority_max);
    LIB_FUNCTION("m0iS6jNsXds", "libScePosix", 1, "libkernel", 1, 1, posix_sched_get_priority_min);
    LIB_FUNCTION("pDuPEf3m4fI", "libScePosix", 1, "libkernel", 1, 1, posix_sem_init);
    LIB_FUNCTION("YCV5dGGBcCo", "libScePosix", 1, "libkernel", 1, 1, posix_sem_wait);
    LIB_FUNCTION("IKP8typ0QUk", "libScePosix", 1, "libkernel", 1, 1, posix_sem_post);
    LIB_FUNCTION("Bq+LRV-N6Hk", "libScePosix", 1, "libkernel", 1, 1, posix_sem_getvalue);
    // libs
    RwlockSymbolsRegister(sym);
    SemaphoreSymbolsRegister(sym);
    KeySymbolsRegister(sym);
}

} // namespace Libraries::Kernel
