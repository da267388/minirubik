/* opcount.h - optional operation counters (-DOPCOUNT, host analysis only). */
#ifndef OPCOUNT_H
#define OPCOUNT_H
#include <stdint.h>
#ifdef OPCOUNT
typedef struct {
    uint64_t iters;      /* IDA* bounds tried                              */
    uint64_t children;   /* children generated (one QT update of 7 cubies) */
    uint64_t descended;  /* children that passed pruning and were expanded */
    uint64_t hlook;      /* PDB nibble lookups                             */
    uint64_t idx;        /* dense-index computations                       */
} opc_t;
extern opc_t opc;
#define OPC(f) (++opc.f)
#else
#define OPC(f) ((void)0)
#endif
#endif
