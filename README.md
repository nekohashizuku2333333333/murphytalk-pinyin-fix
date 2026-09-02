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

- `nh` 可以匹配 `ni hao`，候选中可出现“你好”。
- `zg` 可以匹配 `zhong guo`，候选中可出现“中国”。
- `xj`、`xx`、`gq` 等多字母简拼也会按“每个字母代表一个音节声母”的方式查询短语。

实现方式：

- 新增 `PinyinPhraseKey::set_initials_key()`。
- 当输入长度大于 1 且全部可作为声母时，搜索层先把它解析成一组“声母 + 空韵母”的短语键。
- 短语比较器原本已经支持空韵母作为通配符，本分支继续利用这个机制匹配完整拼音词条。
- 补充 `z/c/s` 与 `zh/ch/sh` 的声母兼容，因此 `zg` 能匹配 `zhongguo`，`cs` 能匹配 `chusheng` 等。
- 如果简拼查不到候选，会自动退回原来的完整拼音解析流程。

涉及文件：

- `PinyinEngine.cpp`
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

## Fn+Space / abc 输入法切换

`Fn+Space` 切换到 `abc` 的问题，目前判断主要是 Qtopia 系统输入法轮换行为，而不是 MurphyTalk 插件内部逻辑。

源码里没有可靠的 `Fn+Space` 专用识别逻辑，也没有稳定的设备按键码信息。因此本分支暂时不在插件里硬拦截 `Fn+Space`，以免误吞正常按键。

更稳的处理方向是在设备系统侧移除或禁用参与轮换的 `abc` 输入法插件，让 `Fn+Space` 不再轮到它。

## 验收建议

安装或编译部署后，可以重点测试：

1. 单按 `Shift` 不再进入造词模式。
2. 在中文模式下输入 `, . ; : ? ! < > ( ) [ ] { } \ ~`，应得到上表中的中文标点。
3. 输入 `nihao`，应仍能按完整拼音查到“你好”等词。
4. 输入 `nh`，应能按简拼查到“你好”等 `n h` 声母短语。
5. 输入 `zg`，应能匹配 `zhongguo` 这类 `zh g` 词条。

## 本地验证记录

当前环境没有完整的老 Qtopia/Qt 构建链，因此没有在本机完成 ARM 插件或 `.ipk` 打包。

已做的源码级验证：

- `scim/scim_pinyin.cpp` 可单独通过现代 GCC 语法编译。
- `phrase/PinyinPhrase.cpp` 可单独通过现代 GCC 语法编译。
- 临时测试确认：
  - `nh` 查询键与 `ni hao` 短语键在短语比较器中等价。
  - `zg` 查询键与 `zhong guo` 短语键在短语比较器中等价。
