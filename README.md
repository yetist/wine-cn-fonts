# 修复 Wine 中文字体方框程序

执行 `wine wine-cn-fonts.exe` 可根据 Linux 已安装的中文字体文件来修改注册表，
以解决中文字体显示为方框的问题。

## 编译方法

在 Linux 上运行以下命令:

```sh
meson setup _build --cross-file cross/linux-mingw-w64-64bit.txt
meson compile -C _build
```

cross.txt 文件内容需根据当前发行版所装的 mingw-w64 工具链情况修改。
