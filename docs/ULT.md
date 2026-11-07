# ULT runtime bring-up

`src/hle_ult_runtime.c` implements the seven runtime/job exports imported by PCSE00638. ABI measurements came from this dump's decrypted `sce_module/libult.suprx`, extracted with the pinned armrecomp tool and inspected in IDA. The retail executable and module code remain in ignored local outputs.

| Object / field | Measured layout |
|---|---|
| Options | 128 bytes, aligned to 8 |
| Header words at 0, 4, 8 | `6c696275`, `6c746d61`, `67696300` |
| Header offset 12 | SDK version, not a structure-size argument |
| Default stack / priority / affinity | offsets 16 / 20 / 24; 4096 / `10000100` / `70000` |
| Runtime object | 1024 bytes, aligned to 8 |
| Logical job object | 256 bytes, aligned to 8 |
| TryJoin while pending | `80810008` |
| Invalid state / competing join | `80810006` / `80810007` |

RuntimeCreate takes object, name, maximum jobs and worker count in r0–r3, followed by work area, options and SDK version on the stack. JobCreate takes object, name, guest callback and callback argument in r0–r3, followed by stack, stack size, runtime, options and SDK version. The callback receives its argument in r0 and returns its result there.

For `n` maximum jobs and `w` workers, the measured work area is:

```
((32*w+135)&~7) + 256*w + ((36*(n+w)+103)&~7)
  + ((36*n+103)&~7) + 4*n
```

The game's request for 32 jobs and 3 workers therefore needs 3728 bytes. It selects a 16 KiB runtime stack and uses implicit job stacks with null job options.

The backend executes each logical job using an existing serialized guest-scheduler context and a host thread. It runs actual translated callbacks, propagates return status, blocks joins, reports pending TryJoin, bounds active jobs, and refuses destruction while jobs remain. Joined jobs recycle their owned guest stack. A small scheduler extension permits supplied guest stacks and exposes kernel thread exit status.

This does **not** reproduce a three-worker fiber pool. Kernel thread identities, TLS ownership, affinity and scheduling equivalence to Sony's workers remain unverified. There is no SceFiber export implementation. Non-default scheduling/options, cancellation, callback-aware waits and game mission workloads need further validation. Runtime/job registries are bounded to 8/64, also subject to the upstream scheduler's capacity.

`tests/ult.c` executes translated-style callbacks through dispatch, exercises blocked and pending joins, checks callback results and preserved registers/VFP state, verifies explicit stack bounds, and submits 200 jobs to detect unbounded stack growth. The retail game passes its previously blocked ULT initialization and reaches later firmware calls; that does not establish full ULT compatibility.
