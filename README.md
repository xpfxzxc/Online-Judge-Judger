# Online Judge Judger

_This project can only judge simple case of a program and isn't completed. It may be redeveloped in 20xx._

## Sample test set

```
/：测试数据集根目录
|-- config.yml： 评测配置文件
|-- input：放置所有标准测试输入文件的目录
   |-- input0.txt
   |-- input1.txt
   |-- …
|-- output：放置所有标准测试输出文件的目录
   |-- output0.txt
   |-- output1.txt
   |-- …
```

### File config.yml format

```
timeLimit：1 # CPU时间限制，单位为s。真实运行时间限制为该3倍
memoryLimit：65535 # 单位为KiB
testPoints: # 所有测试点得分之和必须为100
  - score: 20 # 各个测试点的分数
    cases: [1, 2, 3] # 可为数字或字符串，将替换下面inputFile、outputFile中 # 字符
  - score: 40
    cases: ['4']

inputFile: 'input#.txt' # 目录input下的输入文件名
outputFile: 'output#.txt' # 目录output中的输出文件名

# 所有文件名的字符集为 [-._a-zA-Z0-9]，不超过250个字符
```
