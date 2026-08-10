### 描述

50行实现的时间片轮询调度器。全静态类型,无运行时开销

### 编译

#### 推荐使用 Meson 构建：

安装[meson](https://mesonbuild.com/)和[ninja](https://github.com/ninja-build/ninja)

```sh
meson setup build
ninja -C build/

# 运行示例
./build/schedule
```

#### CMake

```bash
# 在项目根目录运行
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -- -j
ctest --test-dir build --output-on-failure --verbose
```

### 在你的项目中使用

#### Meson

在subproject中添加`template_schedule.wrap`

```ini
[wrap-git]
url = https://github.com/wjjsn/cpp_template_schedule.git
revision = HEAD

[provide]
dependency_names = template_schedule
```

#### CMake
- 方式一：作为子目录使用（`add_subdirectory`）

```cmake
add_subdirectory(path/to/cpp_template_schedule)
target_link_libraries(my_target PRIVATE template_schedule)
```

- 方式二：使用 `FetchContent`（从远程仓库获取）

```cmake
include(FetchContent)
FetchContent_Declare(template_schedule
    GIT_REPOSITORY https://github.com/wjjsn/cpp_template_schedule.git
    GIT_TAG main
)
FetchContent_MakeAvailable(template_schedule)
target_link_libraries(my_target PRIVATE template_schedule)
```

本仓库中已提供 `CMakeLists.txt`，当作为子目录添加时会导出 `template_schedule` INTERFACE 目标，直接链接即可。