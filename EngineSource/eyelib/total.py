import os

total_files = 0
total_lines = 0

for root, dirs, files in os.walk("."):
    for name in files:
        path = os.path.join(root, name)

        try:
            with open(path, "r", encoding="utf-8", errors="ignore") as f:
                total_lines += sum(1 for _ in f)
            total_files += 1
        except:
            pass

print("Total files:", total_files)
print("Total lines:", total_lines)