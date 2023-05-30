import os
import fnmatch

def count_lines(directory):
    line_count = 0
    for dirpath, dirs, files in os.walk(directory):
        if 'node_modules' in dirs:
            dirs.remove('node_modules')  # don't visit node_modules directories
        for extension in ['*.js', '*.html', '*.css', '*.py', '*.json', '*.ino']:
            for filename in fnmatch.filter(files, extension):
                if filename == 'main.js' or filename == 'package-lock.json' or filename == 'count_lines.py':
                    continue
                with open(os.path.join(dirpath, filename)) as f:
                    print(f'Counting lines in {filename}')
                    for line in f:
                        if line.strip():
                            line_count += 1
    return line_count

directory = '/home/sakkov/github-local/MAPLE_APP/App'  # replace with your directory path
print(f'Total lines of code: {count_lines(directory)}')