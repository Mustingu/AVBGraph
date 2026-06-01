import subprocess
import re
import os
import sys

# 检查是否传入了参数
if len(sys.argv) >= 2:
    # 获取命令行传入的参数
    parameter = sys.argv[1]
else :
    parameter = '0'

# 进入 build 目录并执行 make -j
os.chdir('build')  # 切换到 build 文件夹
print("Running 'make -j' in build directory...")
subprocess.run(['make', '-j'], check=True)

def run_command():
    # 执行命令并获取输出
    # 'taskset','-c','0-10',
    result = subprocess.run(['taskset', '-c','10-11','test/test_version_block', '--Spruce', parameter], stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
    # result = subprocess.run(['test/test_version_block', '--Spruce', parameter], stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
    output = result.stdout
    
    # 使用正则表达式提取时间
    # print(re)
    match = re.search(r'WriteLikeSpruce Elapsed time: (\d+)ms', output)
    if match:
        elapsed_time =  int(match.group(1))
        print(f"Current Elapsed Time: {elapsed_time}ms")  # 每次执行后输出时间
        return elapsed_time
    else:
        raise ValueError("Output does not contain the expected time pattern.")

def main():
    times = []
    for _ in range(10):
        elapsed_time = run_command()
        times.append(elapsed_time)

    average_time = sum(times) / len(times)
    max_time = max(times)
    min_time = min(times)

    print()
    print(f"Average Elapsed Time: {average_time}ms")
    print(f"Maximum Elapsed Time: {max_time}ms")
    print(f"Minimum Elapsed Time: {min_time}ms")

if __name__ == "__main__":
    main()