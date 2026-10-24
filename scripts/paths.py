"""Validate paths as consumed by The Forge Android asset filesystem."""
def check_paths(read):
    mounts = {}
    for line in read('PathStatement.txt').decode().splitlines():
        words = line.split()
        if not words or words[0].startswith('#'):
            continue
        bundled = words[0] == 'bundled'
        if bundled:
            words = words[1:]
        if len(words) != 3 or words[1] != '=':
            raise ValueError('Invalid PathStatement line: ' + line)
        directory, _, path = words
        if directory in mounts or not path.endswith('/'):
            raise ValueError('Duplicate directory or missing trailing slash: ' + line)
        mounts[directory] = (bundled, path)
    for directory, filename in [('RD_OTHER_FILES', 'gpu.data'), ('RD_GPU_CONFIG', 'gpu.cfg')]:
        bundled, path = mounts[directory]
        if not bundled:
            raise ValueError(directory + ' must use bundled assets')
        # AndroidFileSystem.cpp uses literal concatenation, not a path join.
        if not read(path + filename):
            raise ValueError('Empty GPU configuration: ' + filename)
