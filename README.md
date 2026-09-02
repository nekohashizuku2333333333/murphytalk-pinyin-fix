# MurphyTalk Pinyin 输入法

这是一个用于 [Sharp Zaurus](http://en.wikipedia.org/wiki/Sharp_Zaurus) SL-Cxx 系列和 SL-5xxx 系列设备的[中文拼音输入法](http://en.wikipedia.org/wiki/Pinyin_input_method)。它也支持其他基于 [Qtopia/Opie](http://en.wikipedia.org/wiki/Qtopia) 的掌上设备系统。

作者在 2004 年为了在自己的 Zaurus SL-C750 上输入中文而编写了这个输入法，并把第一个稳定版本分享给掌上设备社区 [Hi-PDA](http://www.hi-pda.com/forum/viewthread.php?tid=191027&extra=page%3D1)。不久之后，开始有人提出新功能需求，于是作者决定开放源码。项目最初托管在 [SourceForge](http://sourceforge.net/projects/murphypinyin/)。

后来证明，它是 Qtopia 平台上第一个开源中文输入法，因此受到不少关注，尤其是在 Qtopia 被中国许多基于 Linux 的嵌入式移动设备厂商广泛采用之后。

虽然作者多年前已经不再折腾 Zaurus，但很有意思的是，很多年后仍然有人发邮件请求新功能，或询问把它迁移到其他平台时遇到的问题该如何解决。

## 本仓库修复内容

本分支在原 MurphyTalk Pinyin 0.03 源码基础上继续修复，目标是让它更接近现代中文输入法的日常使用体验。

### 1. 关闭造词模式

原程序里，单按 `Shift` 会进入“造词模式”。这会抢占 Shift 上档符号的行为，导致中文标点输入不正常。

现在已经关闭该入口：

- `Shift` 分支只吞掉按键，不再设置 `m_bMakingPhrase`。
- `resetState()` 不再继续累计造词拼音。
- 原来的 `append_phrase()` 函数仍保留在引擎里，但默认键盘流程不再触发造词。

涉及文件：

- `PinyinFrame.cpp`
- `PinyinFrame.h`

### 2. 修复全角标点

旧代码用 `keycode` 判断标点，但在 Zaurus/Qtopia 上，很多 Shift/Fn 组合键的 `keycode` 不变，真正变化的是 `unicode`。例如 `Shift+9` 的 `keycode` 仍可能是 `9`，但 `unicode` 是 `(`。如果继续按 `keycode` 处理，`(` 会先被当作候选选择键吃掉。

现在已改为在 `GetKey()` 入口优先按 `unicode` 查表：

```cpp
if(send_hanzi_mark(u)){
	return true;
}
```

命中中文标点后立即输出并返回，然后才进入数字选词、拼音输入、空格选词等逻辑。

当前映射：

| 输入 | 输出 |
|---|---|
| `,` | `，` |
| `.` | `。` |
| `;` | `；` |
| `:` | `：` |
| `?` | `？` |
| `!` | `！` |
| `<` | `《` |
| `>` | `》` |
| `(` | `（` |
| `)` | `）` |
| `[` | `【` |
| `]` | `】` |
| `{` | `｛` |
| `}` | `｝` |
| `\` | `、` |
| `~` | `～` |

涉及文件：

- `PinyinFrame.cpp`

### 3. 加入完整简拼短语模式

原输入法已经支持完整拼音短语，例如 `nihao` 查询 `ni hao`。本分支新增简拼短语查询，例如：

- `w`、`q`、`n` 这类单字母声母会联想该声母下的单字候选，例如 `w` 可出现“我、为、文、无、问”等。
- `nh` 可以匹配 `ni hao`，候选中可出现“你好”。
- `zg` 可以匹配 `zhong guo`，候选中可出现“中国”。
- `xj`、`xx`、`gq` 等多字母简拼也会按“每个字母代表一个音节声母”的方式查询短语。

实现方式：

- 新增 `PinyinTable::find_chars_by_initial()`，用于单字母声母的单字联想。
- 新增 `PinyinPhraseKey::set_initials_key()`。
- 当输入长度为 1 且是合法声母时，搜索层先汇总词表中该声母下所有完整拼音的单字候选。
- 当输入长度大于 1 且全部可作为声母时，搜索层先把它解析成一组“声母 + 空韵母”的短语键。
- 短语比较器原本已经支持空韵母作为通配符，本分支继续利用这个机制匹配完整拼音词条。
- 补充 `z/c/s` 与 `zh/ch/sh` 的声母兼容，因此 `zg` 能匹配 `zhongguo`，`cs` 能匹配 `chusheng` 等。
- 如果简拼查不到候选，会自动退回原来的完整拼音解析流程。

涉及文件：

- `PinyinEngine.cpp`
- `scim/scim_pinyin.cpp`
- `scim/scim_pinyin.h`
- `phrase/PinyinPhrase.cpp`
- `phrase/PinyinPhrase.h`

注意：简拼能否出现某个词，仍取决于词库里是否有对应短语。例如 `nh` 要出现“你好”，词库中必须已经有 `nihao` / `ni hao` 对应的“你好”词条。

### 4. 现代编译器兼容性补充

为了让底层模块能在现代 GCC 上通过语法编译，补充了少量标准头文件：

- `<algorithm>`
- `<string.h>`
- `<ctype.h>`

涉及文件：

- `PinyinEngine.cpp`
- `phrase/PinyinPhrase.cpp`
- `scim/scim_pinyin.cpp`
- `scim/scim_pinyin.h`

### 5. Qtopia 私有头兼容声明

部分 SDK 里没有 `qwindowsystem_qws.h`，但 `libqte.so` 实际导出了 `QWSServer` 相关符号。为让源码可以在这种 SDK 上编译，本仓库补了一个最小兼容声明头：

- `qwindowsystem_qws.h`

它只声明本输入法实际用到的接口：

- `QWSServer::KeyboardFilter`
- `QWSServer::setKeyboardFilter()`
- `QWSServer::sendKeyEvent()`

注意：最终给 Zaurus/Qtopia 1.x 构建时，应优先使用 Qt/E 2.x 自带的真实 `qwindowsystem_qws.h`。本仓库的兼容声明只用于缺头文件时的补洞，不应替代完整 Qt/E 2.x 头文件集。

## Fn+Space / abc 输入法切换

`Fn+Space` 切换到 `abc` 的问题，目前判断主要是 Qtopia 系统输入法轮换行为，而不是 MurphyTalk 插件内部逻辑。

源码里没有可靠的 `Fn+Space` 专用识别逻辑，也没有稳定的设备按键码信息。因此本分支暂时不在插件里硬拦截 `Fn+Space`，以免误吞正常按键。

更稳的处理方向是在设备系统侧移除或禁用参与轮换的 `abc` 输入法插件，让 `Fn+Space` 不再轮到它。

## 构建与打包格式

### 推荐构建工具链

必须使用与目标 Qtopia 库 ABI 匹配的老工具链和老 Qt 头文件。已验证可用的是远端环境：

```sh
sh /opt/cross/arm/3.4.6-xscale-softvfp-akita/runsdk.sh
export PATH=/opt/cross/arm/2.95.3-2.15/bin:$PATH
```

推荐使用 GCC 2.95.3：

```sh
arm-linux-g++ --version
# 2.95.3
```

同时必须使用与目标 `libqte.so.2` 匹配的 Qt/E 2.3.2 头文件和 Qt2 `moc`。这次实际排查确认，单纯换成 GCC 2.95.3 还不够；如果继续使用 Qt 3.3.5 的头文件和 `moc`，插件仍会编出 Qt3 形态的未解析符号，Qtopia 启动时可能直接跳过该输入法。

不要用 `armv5tel-cacko-linux-g++ 3.4.6` 来链接本仓库 `lib/libqpe.so`、`lib/libqte.so`。原因：

- `libqpe.so` / `libqte.so` 使用 GCC 2.x 老 C++ ABI，符号形如 `__6QFrame...`。
- GCC 3.4.6 会生成新 C++ ABI，符号形如 `_ZN6QFrame...`。
- GCC 3.4.6 默认还会生成 `software FP, VFP` ELF flags，和旧库的 ARM flags `0x2` 不一致。
- 即使用 linker 选项绕过 mismatch，生成的 so 也可能在真机加载失败。

也不要使用 Qt 3.3.5 的头文件或 Qt3 `moc`。错误特征包括：

```text
U __6QFrameP7QWidgetPCcUi
U create__7QWidgetUlbT2
U QMetaObjectCleanUp...
U contextMenuEvent__7QWidgetP17QContextMenuEvent
U imComposeEvent__7QWidgetP8QIMEvent
```

正确的 Qt/E 2.3.2 构建应出现 Qt2 符号形态，例如：

```text
U __6QFrameP7QWidgetPCcUib
U create__7QWidgetUibT2
```

兼容版本应满足：

```sh
readelf -h libmurphypinyin.so.0.03 | grep Flags
# Flags: 0x2, GNU EABI

readelf -d libmurphypinyin.so.0.03 | grep NEEDED
# libqpe.so.1
# libqte.so.2
# libm.so.6
# libc.so.6
```

不应额外依赖：

- `libstdc++.so.6`
- `libgcc_s.so.1`

### 编译命令参考

在远端构建目录中，使用本仓库附带的 `lib/libqpe.so`、`lib/libqte.so`：

```sh
make clean
make \
  CC=arm-linux-gcc \
  CXX=arm-linux-g++ \
  LINK=arm-linux-g++ \
  AR='arm-linux-ar cqs' \
  QPEDIR=/path/to/qtopia-free-1.7.0 \
  QTDIR=/tmp/qt-2.3.2 \
  CXXFLAGS='-pipe -DQT_QWS_SL5XXX -DQT_QWS_CUSTOM -DQWS -DQT_NO_PROPERTIES -DQT_NO_DRAGANDDROP -fno-exceptions -fno-rtti -Wall -W -O2 -fPIC -DNO_DEBUG' \
  INCPATH='-I. -I/path/to/qtopia-free-1.7.0/include -I/tmp/qt-2.3.2/include' \
  LIBS='-L./lib -lqpe -lqte' \
  MOC=/tmp/qt-2.3.2/src/moc/moc
```

如果 Qt/E 2.3.2 源码包里没有预编译 `moc`，可在构建机上先编宿主机版本：

```sh
cd /tmp/qt-2.3.2/src/moc
make -f Makefile.in all \
  SYSCONF_CXX=g++ \
  SYSCONF_CC=gcc \
  SYSCONF_LINK=g++ \
  SYSCONF_CXXFLAGS='-O2' \
  SYSCONF_CFLAGS='-O2' \
  SYSCONF_LFLAGS='' \
  SYSCONF_LIBS='' \
  SYSCONF_LIBS_YACC='' \
  SYSCONF_LIBS_QTAPP='' \
  SYSCONF_CXXFLAGS_QT='' \
  SYSCONF_CXXFLAGS_YACC=''
```

`Makefile` 默认会生成 `DIST/murphypinyin.tar`，其中包含：

```text
libmurphypinyin.so
libmurphypinyin.so.0
libmurphypinyin.so.0.0
libmurphypinyin.so.0.0.2
```

为了兼容原 0.03 包，最终打包时建议把实际 so 改名为：

```text
libmurphypinyin.so.0.03
```

并让三个符号链接都指向它：

```text
libmurphypinyin.so     -> libmurphypinyin.so.0.03
libmurphypinyin.so.0   -> libmurphypinyin.so.0.03
libmurphypinyin.so.0.0 -> libmurphypinyin.so.0.03
```

### 老 Zaurus ipk 外层格式

Sharp Zaurus / 老 ipkg 可安装包不是现代 Debian `ar` 外层格式，而是 gzip 压缩的 tar：

```text
murphytalk.pinyin_0.03_arm_*.ipk
└── tar.gz
    ├── ./debian-binary
    ├── ./control.tar.gz
    └── ./data.tar.gz
```

其中：

```text
debian-binary
```

内容应为：

```text
2.0
```

`control.tar.gz` 内部应直接包含：

```text
./control
```

不要打成：

```text
./CONTROL/control
```

`data.tar.gz` 内部路径应类似：

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

### 打包踩坑记录

这次实际测试中遇到过几个失败包，经验如下：

- 现代 Debian `ar` 外层 `.ipk` 不适合这台老 Zaurus/ipkg 环境。虽然桌面 Linux 会识别为 Debian binary package，但设备端老 `ipkg` 更接近原 MurphyTalk 包的 `tar.gz` 外层格式。
- `control.tar.gz` 里不能放 `./CONTROL/control`。老包里是直接放 `./control`，应按这个格式保持兼容。
- 主库名最好保持 `libmurphypinyin.so.0.03`，不要只使用 Makefile 默认生成的 `libmurphypinyin.so.0.0.2`。三个符号链接也应继续指向 `libmurphypinyin.so.0.03`。
- 用 GCC 3.4.6 生成的 so 会出现新 C++ ABI 和 `software FP, VFP` 标记，不适合这里的 `libqpe.so` / `libqte.so`。必须用 GCC 2.95.3 重新编译。
- 用 GCC 2.95.3 但搭配 Qt 3.3.5 头文件/Qt3 `moc` 仍然不行。它会生成旧 GCC ABI 但 Qt3 API 的符号，设备上的 Qt2 `libqte.so.2` 无法满足这些符号，结果就是安装后开机输入法列表不显示新插件。
- 第一次兼容包 `murphytalk.pinyin_0.03_arm_jianpin_compat.ipk` 在设备端出现 `bad tar header skipping`，随后 `ipkg_install_file: ERROR unpacking data.tar.gz`。原因判断为 tar/gzip 元数据仍不够贴近旧包。
- 后来的 `compat_fixed` 包虽然修正了外层格式，但仍因使用 Qt3 头文件/Qt3 `moc` 构建而无法被 Qtopia 正常加载。
- 后来的 `qt2abi` 包改用 Qt2 头文件和 `moc`，但 Qt/E 2.3.7 仍比这套 Sharp `libqte.so.2` 新，会产生不匹配的 metaobject 符号。
- 最终必须使用 Qt/E 2.3.2，并加 `-DQT_NO_PROPERTIES -DQT_NO_DRAGANDDROP`，否则会分别多出不匹配的 metaobject 签名和 drag/drop QWidget 虚函数。
- 源码里不要直接引用全局 `qwsServer` 对象；旧包使用的是 `QWSServer::setKeyboardFilter()` 和 `QWSServer::sendKeyEvent()` 静态函数。目标库导出了静态函数，但不应要求插件解析 `qwsServer` 全局对象。
- 后来的 `qtopia232` 包解决了插件加载问题，但单字母声母仍按完整拼音查表，`w/q/n` 这类输入没有候选。
- 最终的 `qtopia232_initial` 包采用更保守的方式：以可安装旧包为模板，复用旧 `control.tar.gz`，保留旧包数据结构，只替换用 GCC 2.95.3 + Qt/E 2.3.2 头文件 + Qt2 `moc` + Sharp 裁剪宏重新编译的 so，并加入单声母联想查询。

### 可安装兼容包

已生成的推荐测试包：

```text
dist/murphytalk.pinyin_0.03_arm_jianpin_qtopia232_initial.ipk
```

该包以可安装的 `murphytalk.pinyin_0.03_arm_noshiftzaoci_fwpunct.ipk` 为模板：

- 保留老式外层 `tar.gz` ipk 格式。
- 保留 `control.tar.gz` 内的 `./control`。
- 保留 `libmurphypinyin.so.0.03` 主库名和符号链接布局。
- 保留旧包内的词表、词库、配置和路径结构。
- 只替换为 GCC 2.95.3 + Qt/E 2.3.2 头文件 + Qt2 `moc` 重新编译的新 `libmurphypinyin.so.0.03`。
- 外层仍是老式 gzip tar，不是现代 Debian `ar` ipk。

校验：

```text
SHA256: d520a461211c40e683966ce6aed467ce0f8efea096a2de73e8ef793f256bdd6d
```

## 验收建议

安装或编译部署后，可以重点测试：

1. 单按 `Shift` 不再进入造词模式。
2. 在中文模式下输入 `, . ; : ? ! < > ( ) [ ] { } \ ~`，应得到上表中的中文标点。
3. 输入 `nihao`，应仍能按完整拼音查到“你好”等词。
4. 输入 `w`，应能出现“我、为、文、无、问”等单声母联想候选。
5. 输入 `nh`，应能按简拼查到“你好”等 `n h` 声母短语。
6. 输入 `zg`，应能匹配 `zhongguo` 这类 `zh g` 词条。

## 本地验证记录

已做的源码级验证：

- `scim/scim_pinyin.cpp` 可单独通过现代 GCC 语法编译。
- `phrase/PinyinPhrase.cpp` 可单独通过现代 GCC 语法编译。
- 临时测试确认：
  - `w` 可从单字表汇总出 656 个单声母候选，前几个候选为“我、为、文、无、问、外、位、物”。
  - `q` 可从单字表汇总出 1075 个单声母候选。
  - `n` 可从单字表汇总出 485 个单声母候选。
  - `nh` 查询键与 `ni hao` 短语键在短语比较器中等价。
  - `zg` 查询键与 `zhong guo` 短语键在短语比较器中等价。

已做的远端构建验证：

- 使用 GCC 2.95.3 成功交叉编译 ARM `libmurphypinyin.so`。
- 使用 Qt/E 2.3.2 头文件、Qt2 `moc`、`QT_NO_PROPERTIES`、`QT_NO_DRAGANDDROP` 重新构建，避免 Qt3/Qt2 配置不一致导致的插件加载失败。
- 生成的 so 与可安装旧包 ABI 风格一致：
  - ARM flags 为 `0x2`。
  - 依赖为 `libqpe.so.1`、`libqte.so.2`、`libm.so.6`、`libc.so.6`。
  - Qt/C++ 符号为 GCC 2.x 老 ABI。
- 已按老 Zaurus/ipkg 外层 tar.gz 格式生成兼容 ipk。
