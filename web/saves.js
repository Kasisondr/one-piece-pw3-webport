const SAVE_ROOT = '/opw3-saves-v1';

// Keep one writer per browser profile. The page owns the lock until unload.
export async function mountSaves({FS: fs, IDBFS: backend}, onError) {
  if (!navigator.locks || !backend) {
    throw new Error('This browser cannot open local save storage.');
  }

  let release;
  const hold = new Promise(resolve => { release = resolve; });
  const locked = await new Promise((resolve, reject) => {
    navigator.locks.request(SAVE_ROOT, {ifAvailable: true}, async lock => {
      resolve(Boolean(lock));
      if (lock) await hold;
    }).catch(reject);
  });
  if (!locked) throw new Error('Close the other Pirate Warriors 3 session to use its saves.');

  let saveError;
  const sync = backend.syncfs.bind(backend);
  backend.syncfs = (mount, populate, done) => sync(mount, populate, error => {
    if (!populate) {
      saveError = error;
      if (error) onError(error);
    }
    done(error);
  });
  const syncFiles = populate => new Promise((resolve, reject) => {
    fs.syncfs(populate, error => error ? reject(error) : resolve());
  });

  let mounted = false;
  try {
    fs.mkdirTree(SAVE_ROOT);
    const root = fs.mount(backend, {autoPersist: true}, SAVE_ROOT);
    mounted = true;
    await syncFiles(true);
    fs.mkdirTree(SAVE_ROOT + '/data');
    fs.mkdirTree(SAVE_ROOT + '/ux0');
    return {
      data: SAVE_ROOT + '/data',
      ux0: SAVE_ROOT + '/ux0',
      async flush() {
        // Wait for IDBFS's close-triggered writes before an explicit retry.
        while (root.mount.idbPersistState) {
          await new Promise(resolve => setTimeout(resolve, 25));
        }
        if (saveError) await syncFiles(false);
      },
    };
  } catch (error) {
    backend.syncfs = sync;
    if (mounted) fs.unmount(SAVE_ROOT);
    release();
    throw error;
  }
}
