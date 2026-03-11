import hashlib
import os
import shutil

Import('env')

build_dir = env.subst('$BUILD_DIR')
progname = env.subst('$PROGNAME')

bin_path = os.path.join(build_dir, progname + '.bin')
sha_path = os.path.join(build_dir, progname + '.sha256')

def compute_sha(in_path):
    h = hashlib.sha256()
    with open(in_path, 'rb') as f:
        for chunk in iter(lambda: f.read(4096), b''):
            h.update(chunk)
    return h.hexdigest()

def _post_build_sha(source, target, env):
    if not os.path.exists(bin_path):
        print('Firmware binary not found at', bin_path)
        return

    digest = compute_sha(bin_path)
    with open(sha_path, 'w', encoding='utf-8') as f:
        f.write(digest + '\n')
    print('Generated SHA256:', digest)

    data_dir = os.path.join(env['PROJECT_DIR'], 'firmware')
    os.makedirs(data_dir, exist_ok=True)
    shutil.copy2(bin_path, os.path.join(data_dir, 'firmware.bin'))
    shutil.copy2(sha_path, os.path.join(data_dir, 'firmware.sha256'))
    print('Copied firmware and sha to', data_dir)


env.AddPostAction(bin_path, _post_build_sha)