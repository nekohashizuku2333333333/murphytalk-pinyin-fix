# MurphyTalk / Zaurus Qtopia 开发环境说明

本文给后续开发者或其他 AI 使用，目标是说明本地仓库、远端构建机 SDK、可用库文件、构建方式、IPK 打包格式和常见陷阱。这里的结论来自本项目已经验证过的构建与真机安装排查。

## 1. 本地仓库

当前工作目录：

```text
/home/flan/Documents/Workdir/other/murphytalk-pinyin-fix
```

主要内容：

```text
PinyinEngine.cpp / PinyinEngine.h   拼音解析、查字、查词核心
PinyinFrame.cpp / PinyinFrame.h     Qtopia 输入法窗口、按键过滤、候选栏、提交逻辑
PinyinImpl.cpp / PinyinImpl.h       Qtopia input method 插件入口
scim/                               移植自 scim-pinyin 的拼音表和单字表逻辑
phrase/                             MurphyTalk 短语词库逻辑
.sdk/qtopia-free-1.7.0/             本地保留的 Qtopia 1.7 头文件/结构
lib/libqpe.so                       目标设备对应的 Qtopia libqpe
lib/libqte.so                       目标设备对应的 Qt/E libqte
dist/                               可安装 IPK 产物
fuzz/                               独立 fuzz 测试工具，不参与正常插件构建
doc/                                排查与环境文档
```

当前推荐安装包：

```text
dist/murphytalk.pinyin_1.1.45_arm_jianpin_qtopia232_clearstate.ipk
```

参考旧包：

```text
dist/murphytalk.pinyin_0.03_arm_noshiftzaoci_fwpunct.ipk
```

不要把 fuzz 生成物、fixture、临时 so 放进 `dist/`。`dist/` 只保留旧参考包和当前最新可安装包。

## 2. 远端构建机

远端机器：

```text
root@192.168.122.187
```

连接时这台机器需要旧算法：

```sh
ssh \
  -o StrictHostKeyChecking=no \
  -o UserKnownHostsFile=/tmp/murphy_known_hosts \
  -o KexAlgorithms=+diffie-hellman-group14-sha1,diffie-hellman-group1-sha1 \
  -o HostKeyAlgorithms=+ssh-rsa \
  -o Ciphers=+aes128-cbc,3des-cbc \
  root@192.168.122.187
```

远端已知路径：

```text
/opt/cross/arm/3.4.6-xscale-softvfp-akita/runsdk.sh
/opt/native/i686/3.4.5-2.2.5/bin
/opt/cross/arm/2.95.3-2.15/bin
/tmp/qt-2.3.2
/tmp/murphy-template.ipk
```

构建前常用环境：

```sh
export PATH=/opt/native/i686/3.4.5-2.2.5/bin:/opt/cross/arm/2.95.3-2.15/bin:$PATH
```

`runsdk.sh` 存在，但这个项目最终验证下来不能直接用 3.4.6 xscale 工具链链接本仓库的 `libqpe.so` / `libqte.so`。它可用于探索环境，但最终插件构建应使用 GCC 2.95.3。

## 3. 必须使用的 ABI 组合

目标设备的 `libqpe.so` / `libqte.so` 是 GCC 2.x 老 C++ ABI。最终必须使用：

```text
arm-linux-gcc / arm-linux-g++ 2.95.3
Qt/E 2.3.2 headers
Qt2 moc: /tmp/qt-2.3.2/src/moc/moc
本仓库 lib/libqpe.so 与 lib/libqte.so
```

不要使用：

```text
armv5tel-cacko-linux-g++ 3.4.6
Qt 3.3.5 headers
Qt3 moc
Qt/E 2.3.7 headers
全局 qwsServer 对象
```

原因：

- GCC 3.4.6 会生成新 C++ ABI 符号，例如 `_ZN6QFrame...`，目标库需要旧 ABI 符号，例如 `__6QFrame...`。
- GCC 3.4.6 还可能生成 `software FP, VFP` ELF flags，和旧目标库不匹配。
- Qt3 头文件或 Qt3 `moc` 会生成 Qt3 metaobject / QWidget API 符号，设备上的 Qt2 `libqte.so.2` 无法解析。
- Qt/E 2.3.7 比目标库新，也会产生不匹配的 metaobject 符号。
- 目标库导出的是 `QWSServer::setKeyboardFilter()`、`QWSServer::sendKeyEvent()` 这类静态函数，不应让插件依赖全局 `qwsServer`。

## 4. 推荐构建命令

在远端临时构建目录中，源码根目录记为 `$BUILD`：

```sh
export PATH=/opt/native/i686/3.4.5-2.2.5/bin:/opt/cross/arm/2.95.3-2.15/bin:$PATH

touch Makefile
make clean 2>/dev/null || true
touch Makefile

make all \
  CC=arm-linux-gcc \
  CXX=arm-linux-g++ \
  LINK=arm-linux-g++ \
  QPEDIR=$BUILD/.sdk/qtopia-free-1.7.0 \
  QTDIR=/tmp/qt-2.3.2 \
  CXXFLAGS="-pipe -DQT_QWS_SL5XXX -DQT_QWS_CUSTOM -DQWS -DQT_NO_PROPERTIES -DQT_NO_DRAGANDDROP -fno-exceptions -fno-rtti -Wall -W -O2 -fPIC -DNO_DEBUG" \
  INCPATH="-I$BUILD -I$BUILD/.sdk/qtopia-free-1.7.0/include -I/tmp/qt-2.3.2/include" \
  LIBS="-L$BUILD/lib -lqpe -lqte" \
  MOC=/tmp/qt-2.3.2/src/moc/moc
```

关键宏：

```text
QT_QWS_SL5XXX
QT_QWS_CUSTOM
QWS
QT_NO_PROPERTIES
QT_NO_DRAGANDDROP
NO_DEBUG
```

`QT_NO_PROPERTIES` 与 `QT_NO_DRAGANDDROP` 很重要。缺少它们时，生成的 so 可能多出目标 Qt2 库没有的 metaobject 或 drag/drop QWidget 虚函数符号。

构建完成后，默认输出：

```text
$BUILD/DIST/murphypinyin.tar
```

其中包含 Makefile 默认命名的 so：

```text
libmurphypinyin.so
libmurphypinyin.so.0
libmurphypinyin.so.0.0
libmurphypinyin.so.0.0.2
```

最终打包时取 `libmurphypinyin.so.0.0.2`，替换旧包里的：

```text
opt/Qtopia/plugins/inputmethods/libmurphypinyin.so.0.03
```

## 5. IPK 打包格式

Sharp Zaurus / 老 ipkg 使用的包格式不是现代 Debian `ar`，而是 gzip 压缩 tar。

外层必须类似：

```text
./debian-binary
./control.tar.gz
./data.tar.gz
```

`debian-binary` 内容：

```text
2.0
```

`control.tar.gz` 内部应直接包含：

```text
./control
```

不要做成：

```text
./CONTROL/control
```

`data.tar.gz` 内路径应保持旧包结构：

```text
./etc/murphytalk.conf
./home/zaurus/.murphytalk/pinyin_table.txt
./home/zaurus/.murphytalk/murphytalk_phrase.dat
./home/zaurus/.murphytalk/murphytalk_phrase_idx.txt
./opt/Qtopia/plugins/inputmethods/libmurphypinyin.so
./opt/Qtopia/plugins/inputmethods/libmurphypinyin.so.0
./opt/Qtopia/plugins/inputmethods/libmurphypinyin.so.0.0
./opt/Qtopia/plugins/inputmethods/libmurphypinyin.so.0.03
```

最稳妥做法是以可安装旧包为模板，只替换 so：

```text
dist/murphytalk.pinyin_0.03_arm_noshiftzaoci_fwpunct.ipk
```

控制文件版本可改为当前软件版本：

```text
Version: 1.1.45
```

## 6. 打包流程参考

远端 `$OUT` 为最终包路径，`$SO` 为新编译出的 `libmurphypinyin.so.0.0.2`：

```sh
PKG=/tmp/murphytalk-pkg
OUT=/tmp/murphytalk.pinyin_1.1.45_arm_custom.ipk
SO=/tmp/murphy-so/libmurphypinyin.so.0.0.2

rm -rf "$PKG"
mkdir -p "$PKG"
cd "$PKG"

tar xzf /tmp/murphy-template.ipk
mkdir control data

cd control
tar xzf ../control.tar.gz
perl -0pi -e 's/^Version: .*/Version: 1.1.45/m' control
tar czf ../control.tar.gz .

cd ../data
tar xzf ../data.tar.gz
cp "$SO" opt/Qtopia/plugins/inputmethods/libmurphypinyin.so.0.03
chmod 755 opt/Qtopia/plugins/inputmethods/libmurphypinyin.so.0.03
tar czf ../data.tar.gz .

cd ..
tar czf "$OUT" ./debian-binary ./control.tar.gz ./data.tar.gz
```

注意：外层使用 `tar czf`，不是 `ar r`。

## 7. 构建后验证

本地或远端可检查：

```sh
tar tzf murphytalk.pinyin_1.1.45_arm_custom.ipk
```

应看到：

```text
./debian-binary
./control.tar.gz
./data.tar.gz
```

检查 control：

```sh
mkdir -p /tmp/murphy-check
tar xzf murphytalk.pinyin_1.1.45_arm_custom.ipk -C /tmp/murphy-check
tar xzf /tmp/murphy-check/control.tar.gz -O ./control
```

检查 so：

```sh
tar xzf /tmp/murphy-check/data.tar.gz -O ./opt/Qtopia/plugins/inputmethods/libmurphypinyin.so.0.03 > /tmp/murphy-check/libmurphypinyin.so.0.03

readelf -Ws /tmp/murphy-check/libmurphypinyin.so.0.03 | \
  egrep 'qwsServer|setKeyboardFilter|sendKeyEvent|new_metaobject|QMetaProperty|QMetaEnum|__modsi3' || true
```

正常结果应该只看到旧 Qt2 / Qtopia 相关未解析符号，例如：

```text
new_metaobject__11QMetaObjectPCcT1P9QMetaDataiT3iP10QClassInfoi
setKeyboardFilter__9QWSServerPQ29QWSServer14KeyboardFilter
sendKeyEvent__9QWSServeriiibT4
```

不应出现：

```text
qwsServer
QMetaProperty
QMetaEnum
__modsi3
```

如果工具链可用，也建议检查 ELF 头和依赖：

```sh
readelf -h /tmp/murphy-check/libmurphypinyin.so.0.03 | grep Flags
readelf -d /tmp/murphy-check/libmurphypinyin.so.0.03 | grep NEEDED
```

期望：

```text
Flags: 0x2, GNU EABI
NEEDED: libqpe.so.1
NEEDED: libqte.so.2
NEEDED: libm.so.6
NEEDED: libc.so.6
```

不应额外依赖：

```text
libstdc++.so.6
libgcc_s.so.1
```

## 8. fuzz 测试

fuzz 工具位于：

```text
fuzz/
```

运行：

```sh
cd fuzz
make fuzz
```

它会：

- 编译一个不依赖 Qtopia UI 的测试程序。
- 直接链接上级目录的 `PinyinEngine.cpp`、`scim/scim_pinyin.cpp`、`phrase/PinyinPhrase.cpp`、`public.cpp`。
- 用 `fuzz/stubs/qstring.h` 提供最小 `QString/QChar` stub。
- 从 `dist/murphytalk.pinyin_1.1.45_arm_jianpin_qtopia232_clearstate.ipk` 解出词库 fixture。
- 测试固定用例、随机拼音串、前缀不变量、退格回环、确定性、性能上限和消费长度。

最近通过结果：

```text
cases=20103 failures=0 new_failures=0 max_ms=25
```

失败输入会追加到：

```text
fuzz/fuzz_regressions.txt
```

正常情况下该文件只有注释，没有失败条目。

## 9. 给其他应用开发的建议

如果要用这套环境编写其他 Zaurus/Qtopia 应用：

- 优先确认目标设备运行的是 Qtopia 1.x / Qt/E 2.x，而不是桌面 Qt3/Qt4。
- 使用 GCC 2.95.3 与 Qt/E 2.3.2 头文件/`moc`，除非能证明目标库 ABI 不同。
- 链接本仓库 `lib/libqpe.so`、`lib/libqte.so` 时，不要混用 GCC 3.x C++ ABI。
- UI 类使用 Qt2/Qtopia 1.x API，不要使用 Qt3 后才稳定出现的属性系统、拖放事件、输入法事件等接口。
- 插件或应用如果需要 QWS 私有接口，优先使用目标 Qt/E 2.x 自带头文件；仓库里的 `qwindowsystem_qws.h` 只是最小兼容声明，适合补缺，不是完整 SDK。
- 构建后一定用 `readelf -Ws` 检查符号形态；能编过不等于能被设备加载。
- IPK 打包尽量以真机可安装旧包为模板修改，不要用现代 Debian 打包工具默认格式。
- 设备端若 `ipkg` 报 `bad tar header skipping`，优先怀疑外层 IPK 格式或 `control.tar.gz` / `data.tar.gz` 内路径格式不兼容。

## 10. 常见失败特征

安装成功但输入法列表不显示：

- 多半是插件 so 无法被 Qtopia 加载。
- 检查是否混用了 Qt3 `moc`、Qt3 头文件或 GCC 3.x ABI。
- 检查是否出现 `qwsServer`、`QMetaProperty`、`QMetaEnum` 等目标库没有的符号。

`ipkg` 解包时报 `bad tar header skipping`：

- 多半是 IPK 外层格式错误。
- 应使用 gzip tar 外层，并包含 `./debian-binary`、`./control.tar.gz`、`./data.tar.gz`。

能显示输入法但按键没有候选：

- 先确认词表路径是否存在：
  - `/home/zaurus/.murphytalk/pinyin_table.txt`
  - `/home/zaurus/.murphytalk/murphytalk_phrase.dat`
  - `/home/zaurus/.murphytalk/murphytalk_phrase_idx.txt`
- 再检查拼音解析是否把输入误消费成首声母或丢掉尾巴。

预编辑残留最后一个字母：

- 应检查 `resetState()` 是否同时清掉 UI 状态和 `PinyinEngine` 的 raw/display/pending 状态。

选字后剩余拼音没有候选：

- 应检查 `commit_selection()` 后是否重新 `search()`。
- 还要检查 `search()` 是否重置 `candidates_on_page` 与 `candidates_on_prev_page`，避免候选页状态残留。
