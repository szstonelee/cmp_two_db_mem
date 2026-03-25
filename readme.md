# details for the projecct
Please reference:
https://zhuanlan.zhihu.com/p/2020214097409496096

# build required
gcc g++
Linux
x86 machine

# build clean
```
make clean
```

# build all
```
make
```

# build only single app
you can build one app in the following list
* single_pg
* single_cpp_malloc
* single_cpp_new
* single_cpp_swap_new
* single_pg_swap
* single_cpp_swap_new_with_big
* single_pg_swap_with_big

like this
```
make single_pg
```

# Test Results
In my virtual OS (Ubuntu 22), machine: core 8, Intel(R) Core(TM) Ultra 9 285H   2.90 GHz

| app name                                                          | per ms  |         |         |          |      |
|-------------------------------------------------------------------|---------|---------|---------|----------|------|
|                                                                   | 第一次     | 第二次     | 第三次     | 平均       | 对比增值 |
| single_pg                                                         | 210868  | 222321  | 215018  | 216069   | 25%  |
| single_cpp_malloc                                                 | 166675  | 172699  | 178374  | 172583   |      |
|                                                                   |         |         |         |          |      |
| single_cpp_new                                                    | 7801886 | 7904058 | 8196115 | 7967353  |      |
|                                                                   |         |         |         |          |      |
| single_cpp_swap_new                                               | 175346  | 177773  | 177601  | 176907   |      |
| single_pg_swap                                                    | 218006  | 209776  | 199874  | 209219   | 18%  |
|                                                                   |         |         |         |          |      |
| single_cpp_swap_new（TCMalloc)                                     | 336046  | 349176  | 360871  | 348698   | 71%  |
| single_pg_swap（TCMalloc）                                          | 189141  | 210269  | 210939  | 203450   |      |
|                                                                   |         |         |         |          |      |
| single_cpp_swap_new_with_big（TCMalloc）                            | 28726   | 30283   | 29867   | 29625    | 9%   |
| single_pg_swap_with_big（TCMalloc）                                 | 27713   | 28000   | 25881   | 27198    |      |
| single_pg_swap_with_big                                           | 78729   | 79202   | 78719   | 78883    | 166% |
| single_cpp_swap_new_with_big                                      | 79496   | 77026   | 74528   | 77017    |      |
|                                                                   |         |         |         |          |      |
| LD_PRELOAD=""/usr/lib/x86_64-linux-gnu/libtcmalloc_minimal.so.4"" |         |         |         |          |      |

