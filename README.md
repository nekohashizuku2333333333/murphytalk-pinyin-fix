# MurphyTalk Pinyin 输入法

这是一个用于 [Sharp Zaurus](http://en.wikipedia.org/wiki/Sharp_Zaurus) SL-Cxx 系列和 SL-5xxx 系列设备的[中文拼音输入法](http://en.wikipedia.org/wiki/Pinyin_input_method)。它也支持其他基于 [Qtopia/Opie](http://en.wikipedia.org/wiki/Qtopia) 的掌上设备系统。

作者在 2004 年为了在自己的 Zaurus SL-C750 上输入中文而编写了这个输入法，并把第一个稳定版本分享给掌上设备社区 [Hi-PDA](http://www.hi-pda.com/forum/viewthread.php?tid=191027&extra=page%3D1)。不久之后，开始有人提出新功能需求，于是作者决定开放源码。项目最初托管在 [SourceForge](http://sourceforge.net/projects/murphypinyin/)。

后来证明，它是 Qtopia 平台上第一个开源中文输入法，因此受到不少关注，尤其是在 Qtopia 被中国许多基于 Linux 的嵌入式移动设备厂商广泛采用之后。

虽然作者多年前已经不再折腾 Zaurus，但很有意思的是，很多年后仍然有人发邮件请求新功能，或询问把它迁移到其他平台时遇到的问题该如何解决。

## 本仓库修复内容

本分支在原 MurphyTalk Pinyin 0.03 源码基础上继续修复，目标是让它更接近现代中文输入法的日常使用体验。

当前修改版显示版本为 `1.1.45`。`FILE_VERSION` 仍保留为 `0.03`，这是短语/词库文件格式版本，不等同于软件显示版本。

### 0. 全部可见界面文字改为简体

真机繁体字样可能受字体/编码影响显示为乱码，因此当前版本把插件内可见中文全部改成简体。

- 候选栏右侧入口从 `About` / 繁体“关于”统一为简体“关于”。
- 关于对话框标题改为“关于此软件”。
- 关于对话框正文改为简体，并显示 `MurphyTalk 拼音 1.1.45`。
- 候选栏拼音显示改为直接显示用户输入的原始拼音串，避免单按 `n`、`w` 等声母时只出候选而输入串不显示。
- `关于` 入口和关于对话框均使用 `QString::fromUtf8()` 构造文本，避免 Qt2 把 UTF-8 中文按本地编码解释成乱码。

### 0.1. 预编辑确认行为

当前 Qtopia 1.x `InputMethodInterface` 没有现代输入法框架那种应用内 preedit API，因此本分支使用输入法自己的候选栏作为预编辑/组字区：

- 字母键先进入输入法候选栏，不直接发送给当前应用。
- 候选存在时，数字键选择对应候选后才上屏。
- 候选存在时，回车确认当前页第一个候选并上屏。
- 普通完整拼音或已经包含空格的预编辑串，按空格确认当前页第一个候选并上屏。
- 单个声母后的第一个空格保留为分隔符，用于继续输入 `n hao` 这类混合拼音；此后再按空格会确认候选。
- 候选不存在时，回车提交原始预编辑拼音串。
- 退格只删除预编辑拼音串，不会删除应用中已经上屏的文字。
- `Esc` 整体取消当前预编辑串和候选，不影响已经上屏的文字。
- 中文标点键在有预编辑串时，会先确认当前候选，再输出对应全角标点，避免标点绕过预编辑状态。
- 当退格把预编辑拼音删到空时，会同时清空候选、页码和当前页候选计数，候选栏不会残留上一轮字词。
- 当选中追加在短语候选后面的“首音节单字候选”时，只消耗首音节拼音，剩余拼音继续留在预编辑串中并重新搜索。
- 如果整个输入本身是合法完整音节，例如 `cong`，会优先按完整音节查单字并整体消费，避免误退化成 `c` + `ong`。
- 如果输入无法解析为完整首音节，例如 `wcao`，会退回首声母候选，先显示 `w` 的单字候选。
- 查词失败后的兜底顺序为：先尝试首个完整音节，再尝试首声母。因此 `hef` 会先显示 `he` 的单字候选并消费 `he`，`nma` 则因没有合法首音节而回退到 `n`。

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

### 3. 加入完整简拼和混合拼音模式

原输入法已经支持完整拼音短语，例如 `nihao` 查询 `ni hao`。本分支新增简拼短语查询，例如：

- `w`、`q`、`n` 这类单字母声母会联想该声母下的单字候选，例如 `w` 可出现“我、为、文、无、问”等。
- `nh` 可以按 `n h` 两个声母去匹配词库里的 `ni hao`、`nan hai` 等短语。
- `zg` 可以匹配 `zhong guo` 这类词库里已有的短语。
- `xj`、`xx`、`gq` 等多字母简拼也会按“每个字母代表一个音节声母”的方式查询短语。
- `n hao`、`wo cao` 这类“声母 + 空格 + 完整拼音”或“完整拼音 + 空格 + 完整拼音”的混合输入会先按整词查询；如果词库没有整词，会先显示第一个 token 的候选，选字后保留后面的拼音继续搜索。

实现方式：

- 新增 `PinyinTable::find_chars_by_initial()`，用于单字母声母的单字联想。
- 新增 `PinyinPhraseKey::set_initials_key()`。
- 新增 `PinyinPhraseKey::set_mixed_key()`，用于解析带空格的混合输入；单字母 token 作为声母通配，完整拼音 token 作为精确音节。
- 当输入长度为 1 且是合法声母时，搜索层先汇总词表中该声母下所有完整拼音的单字候选。
- 当输入长度大于 1 且全部可作为声母时，搜索层先把它解析成一组“声母 + 空韵母”的短语键。
- 当完整拼音短语查不到时，会回退到第一个完整音节的单字候选。例如 `wocao` 若词库中没有整词，会先显示 `wo` 的候选；选中第一个字后保留剩余 `cao` 并继续搜索。
- 当带空格的混合输入查不到整词时，也会回退到第一个 token。例如 `n hao` 会先显示 `n` 的声母候选；选中一个字后，输入栏保留 `hao` 并继续显示第二个音节候选。
- 短语比较器原本已经支持空韵母作为通配符，本分支继续利用这个机制匹配完整拼音词条。
- 补充 `z/c/s` 与 `zh/ch/sh` 的声母兼容，因此 `zg` 能匹配 `zhongguo`，`cs` 能匹配 `chusheng` 等。
- 如果简拼查不到候选，会自动退回原来的完整拼音解析流程。

涉及文件：

- `PinyinEngine.cpp`
- `scim/scim_pinyin.cpp`
- `scim/scim_pinyin.h`
- `phrase/PinyinPhrase.cpp`
- `phrase/PinyinPhrase.h`

注意：简拼能否直接出现某个整词，仍取决于词库里是否有对应短语。当前保留旧包词库，因此若旧词库没有“你好”，`nh` 或 `n hao` 不会凭空生成“你好”，但会回退到第一个音节/声母候选，保证不会空白卡住。

### 3.1. 容错全拼分词、简拼切分和隔音符

音节表来源为 `scim/scim_pinyin.cpp` 内置的 `scim_pinyin_initials` 与 `scim_pinyin_finals`，并继续使用原 SCIM `PinyinKey::set_key()` 做合法性校验。

新增的容错连续拼音切分逻辑在 `PinyinEngine.cpp` 中实现：

- 对输入串做带回溯的枚举，按 `SCIM_PINYIN_KEY_MAXLEN` 从长到短尝试合法完整音节，不再只保留贪心最长匹配的一条路径。
- 除完整音节外，还接受单独声母以及 `zh/ch/sh` 这类未完成声母片段；串尾的不完整片段作为“待定输入”，串中的不完整片段按简拼处理。
- 一个连续串最多收集 32 种切分，避免老机器上输入超长串时拖慢。
- 每种切分会打分并排序：完整音节按覆盖字符数高分；串尾不完整片段轻微低于完整音节但不视为错误；串中的不完整片段降权，作为简拼候选来源。
- 预编辑内部始终保留用户输入的原始串，不会因为解析失败丢字符；候选栏第一行显示最佳切分后的可读形式，例如 `hef` 显示为 `he f`。
- 解析不了或接不上时不会自动上屏。上屏只由用户选择候选、按空格确认或按回车确认触发。
- `xian` 这类歧义串会同时产生 `xian` 和 `xi an` 方向的候选；候选统一合并展示。
- 用户输入 `'` 时作为强制音节边界，例如 `xi'an` 只能跨边界切为 `xi an`，不会把 `'` 两侧合成一个音节。
- 相同短语 offset 会去重，避免不同切分命中同一短语时重复显示。

排序融合策略：

- 对最优和次优切分分别查词后合并候选；完整音节路径优先，简拼/不完整片段路径次之。
- 多个切分命中的相同短语只显示一次，合并后继续按词库频率排序。
- 如果词库没有整词候选，会回退到最佳切分的第一个可提交片段：完整首音节优先，其次首声母。
- 选择追加在短语候选后面的首片段单字时，只消耗该片段长度，剩余原始拼音继续留在预编辑中重新搜索。

连续混合简拼也已加入：

- `bj` 会解析为 `b + j`。
- `beij` 会解析为 `bei + j`。
- `hef` 会解析为 `he + f`，先显示 `he` 的单字候选，选择后保留 `f`。
- `nma` 会优先解析为 `n + ma`，同时也允许 `n + m + a` 方向参与候选。
- `nihaoshijie` 会切为 `ni hao shi jie` 方向查词；词库缺整词时仍能回退到首音节逐字输入。
- 完整音节保留精确匹配，单字母声母使用空韵母通配。
- 对 `nm` 这类连续简拼，候选前面仍保留整词短语，例如“你们”；同时会把第一个声母 `n` 的单字候选追加在后面。选择后面的单字候选时只消耗第一个 `n`，剩余 `m` 会留在预编辑串里继续候选，便于逐字输入。

本分支不实现 B5 双拼。

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
- 后来的 `qtopia232_initial` 包加入了单声母联想查询，但完整拼音短语缺词时仍不会自动分段。
- 后来的 `qtopia232_segment` 包采用更保守的方式：以可安装旧包为模板，复用旧 `control.tar.gz`，保留旧包数据结构，只替换用 GCC 2.95.3 + Qt/E 2.3.2 头文件 + Qt2 `moc` + Sharp 裁剪宏重新编译的 so，并加入单声母联想查询和首音节分段回退。
- 后来的 `qtopia232_mixed` 包继续沿用上述兼容打包格式，并补上简体 UI、候选栏原始拼音显示、带空格混合输入解析和 `n hao` 这类输入的首 token 回退。
- 后来的 `qtopia232_preedit` 包在 `mixed` 基础上补上预编辑确认行为：确认前字母只留在输入法候选栏，数字/空格/回车确认后才上屏，退格和 `Esc` 只作用于预编辑状态。
- 后来的 `qtopia232_bcore` 包继续加入 §B 拼音解析核心的一部分：全拼多切分、隔音符强制边界、连续混合简拼，以及全拼优先于简拼的查询顺序。
- 后来的 `qtopia232_splitui` 包修复 `nm` 这类连续简拼无法逐字输入的问题，并修复 Qt2 下关于入口/关于对话框中文乱码。
- 后来的 `qtopia232_clearpreedit` 包修复退格删空预编辑拼音后候选字仍残留的问题。
- 后来的 `qtopia232_retainrest` 包修复 `nima` 这类连续全拼在选择首音节单字后误清空剩余拼音的问题。
- 后来的 `qtopia232_syllablefirst` 包修复 `wcao` 无候选和 `cong` 被误消费为 `c` 的问题：完整音节优先，无法完整解析时才退回首声母。
- 后来的 `qtopia232_fallbackorder` 包修复 `hef` 误消费 `h`、`nma` 无候选的问题，把兜底顺序明确为首完整音节优先，其次首声母。
- 最终的 `qtopia232_tolerantparse` 包把拼音切分改成容错回溯解析：原始预编辑串完整保留，候选栏显示最佳切分，解析失败不再触发自动上屏。

### 可安装兼容包

已生成的推荐测试包：

```text
dist/murphytalk.pinyin_1.1.45_arm_jianpin_qtopia232_tolerantparse.ipk
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
SHA256: 786efc61be428826ed67625285b7cd93f31fbfc564c88f3bcc20f941cac34e99
```

## 验收建议

安装或编译部署后，可以重点测试：

1. 单按 `Shift` 不再进入造词模式。
2. 在中文模式下输入 `, . ; : ? ! < > ( ) [ ] { } \ ~`，应得到上表中的中文标点。
3. 输入 `nihao`，应仍能按完整拼音查到“你好”等词。
4. 输入 `w`，应能出现“我、为、文、无、问”等单声母联想候选。
5. 输入 `nh`，应能按简拼查词库中已有的 `n h` 声母短语；若旧词库没有目标词，属于词库内容限制。
6. 输入 `zg`，应能匹配 `zhongguo` 这类 `zh g` 词条。
7. 输入一个词库里没有的完整拼音组合，例如 `wocao`，应先显示 `wo` 的候选；选中后保留 `cao` 继续显示第二个音节候选。
8. 输入带空格的混合拼音，例如 `n hao`，如果整词查不到，应先显示 `n` 的声母候选；选中后保留 `hao` 继续显示第二个音节候选。
9. 单按 `n`、`w` 等声母时，候选栏第一行应显示实际输入的拼音字母，而不是只显示“中”。
10. 输入拼音但不确认时，应用正文不应新增字符；按退格只删候选栏里的拼音。
11. 输入拼音后按 `Esc`，候选栏清空，应用正文不应变化。
12. 输入拼音后按回车，应确认当前页第一个候选；若没有候选，则提交原始拼音串。
13. 输入 `nm`，前面可出现“你们”等整词候选，翻到后面也应能看到 `n` 的单字候选；选择后面的单字后，应保留 `m` 继续候选。
14. 打开关于对话框，标题、正文和按钮以设备字体可显示的简体中文呈现，不应再出现 UTF-8 乱码。
15. 输入 `n` 出候选后按退格删空拼音，候选栏应立即清空，不应残留“我、握、窝”等上一轮候选。
16. 输入 `nima` 后选择后面追加的 `ni` 单字候选“你”，应上屏“你”，预编辑栏保留 `ma` 并继续显示 `ma` 的候选。
17. 输入 `wcao`，应先显示 `w` 的单字候选，不应空白。
18. 输入 `cong` 并选“从”，应整体消费 `cong`，不应只消费 `c` 后留下 `ong`。
19. 输入 `hef`，应先显示 `he` 的单字候选；选“何/和”等候选后，应保留 `f` 而不是留下 `ef`。
20. 输入 `nma`，应显示 `n` 的单字候选，不应空白。
21. 输入 `beij`，候选栏应显示类似 `bei j` 的切分，并优先给出 `bei` 相关候选。
22. 输入 `xian`，应能按完整音节 `xian` 查询，同时保留 `xi an` 方向的短语候选参与合并。
23. 输入 `nihaoshijie`，应切成 `ni hao shi jie` 方向查询；如果旧词库没有整词，也应能逐段继续输入，不应丢尾巴或自动上屏。

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
  - 当前旧词库里没有 `nihao` / “你好”短语；混合输入 `n hao` 会走首 token 回退，不会再出现无候选空白。

### 独立 fuzz 测试

新增了不依赖 Qtopia UI 的解析/查词 fuzz harness：

```sh
make -f tests/Makefile fuzz
```

测试程序直接链接 `PinyinEngine.cpp`、`scim/scim_pinyin.cpp`、`phrase/PinyinPhrase.cpp` 和 `public.cpp`，并通过 `tests/stubs/qstring.h` 提供最小 `QString/QChar` stub，因此不需要 Qtopia 窗口系统。`tests/Makefile` 会从最终 IPK 解出 `murphytalk_phrase.dat` 和 `murphytalk_phrase_idx.txt` 到 `tests/fixtures/`，并在 `MURPHY_TEST_PHRASE_FILE` 中把测试词库路径传给短语表；真实 Qtopia 构建不受影响。

覆盖内容：

- 固定用例覆盖单独零声母、单字母/声母、`hef` 家族、`nma` 家族、`xian`/`jinan` 等边界歧义、隔音符、`lv/nv` 与 `lu/nu`、恶意输入和长串性能用例。
- 随机用例：随机拼接合法音节、任意前缀、声母、单字母和隔音符，长度 1 到 12。
- 不变量：
  - `get_raw_pinyin()` 必须与原始输入逐字符一致。
  - 每个输入要么有候选，要么被测试明确视为待定/不完整。
  - 解析和查词阶段不触发任何 commit。
  - 每个用例的每一个前缀都必须满足上述条件。
  - 退格回环后得到的状态必须与直接输入对应前缀完全一致。
  - 同一输入连续解析两次必须得到同样的 raw、display、候选数、pending 和首短语。
  - 每个输入的单次解析耗时不得超过 25 ms，覆盖 `nnnnnnnnnnnnnnnnnnnn` 这类防指数爆炸长串。
- 固定展示断言：
  - `hef -> he f`
  - `nma -> n ma`
  - `xian -> xian`
  - `beij -> bei j`
  - `nihaoshijie -> ni hao shi jie`
  - `hefei` 的短语候选必须包含“合肥”。

违反不变量时，程序会打印输入串、切分显示、候选数、pending 状态和 commit 计数，并把新失败追加到 `tests/fuzz_regressions.txt`。

最近一次验证结果：

```text
FIXED input=hef display=he f candidates=103 pending=yes
FIXED input=nma display=n ma candidates=485 pending=no
FIXED input=xian display=xian candidates=202 pending=no
FIXED input=beij display=bei j candidates=76 pending=yes
FIXED input=nihaoshijie display=ni hao shi jie candidates=80 pending=no
cases=20103 failures=0 new_failures=0 max_ms=25 regression_file=tests/fuzz_regressions.txt
```

连续两轮 fuzz 均为 `failures=0`、`new_failures=0`；最终失败集合为空，`tests/fuzz_regressions.txt` 只保留说明注释。

已做的远端构建验证：

- 使用 GCC 2.95.3 成功交叉编译 ARM `libmurphypinyin.so`。
- 使用 Qt/E 2.3.2 头文件、Qt2 `moc`、`QT_NO_PROPERTIES`、`QT_NO_DRAGANDDROP` 重新构建，避免 Qt3/Qt2 配置不一致导致的插件加载失败。
- 生成的 so 与可安装旧包 ABI 风格一致：
  - ARM flags 为 `0x2`。
  - 依赖为 `libqpe.so.1`、`libqte.so.2`、`libm.so.6`、`libc.so.6`。
  - Qt/C++ 符号为 GCC 2.x 老 ABI。
- 已按老 Zaurus/ipkg 外层 tar.gz 格式生成兼容 ipk。
- 本次最终包外层检查结果为 gzip tar，内容为 `./debian-binary`、`./control.tar.gz`、`./data.tar.gz`。
- `control` 中 `Version` 已更新为 `1.1.45`。
- 插件 so 的字符串检查未再匹配到 `About` 或已知繁体 UI 关键词。
