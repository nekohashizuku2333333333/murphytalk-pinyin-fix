# murphytalk.pinyin 0.03 —— 让 Shift 输出中文上档符号（禁用造词模式）补丁分析

## 结论（先看这里）
- 造词模式的**唯一触发点**就是 **Shift 键本身**（Qt 键码 `0x1020 = Key_Shift`）。
- 原程序里，一按 Shift 就去切换 `QPinyinFrame` 对象里偏移 `+0x141` 的"造词标志位"，把 Shift 事件吞掉，于是后续上档符号逻辑走不到。
- 好消息：**全角标点映射表本来就存在且正确**（在 `send_hanzi_mark` 函数里）：
  `<`→《、`>`→》、`,`→，、`.`→。、`?`→？、`:`→：
- 只要让 Shift 键**不进入造词模式**，随后的 `Shift+符号键`（如 space 右边那个键 `<` = 键码 `0x3c`）就会自然落到 `send_hanzi_mark`，输出「《」。
- **补丁只改了 1 个字节**，把"进入造词模式"那条分支变成"直接吞掉 Shift、什么都不做"。

## 文件结构
这是个 Qtopia 输入法插件包，核心是编译好的 ARM 共享库（未 strip，保留符号，便于分析）：

```
/opt/Qtopia/plugins/inputmethods/libmurphypinyin.so.0.03   ← 主程序（本次打补丁对象）
    + 三个符号链接 .so / .so.0 / .so.0.0 → .so.0.03
/etc/murphytalk.conf                                        ← 字体配置
/home/zaurus/.murphytalk/*.txt/.dat                         ← 拼音表与词库
```

## 按键处理流程（逆向还原）
入口 `QPinyinFrame::filter(unicode, keycode, modifiers, isPress, ...)`：
- **完全没有使用 modifiers 参数**（第 3 个参数一进函数就被覆盖）。
- 只把 `unicode`、`keycode` 传给主处理函数 `GetKey`。
> 含义：这台机器上 Shift 组合键是**直接以"上档键码"送达的**（例如 Shift+逗号 直接给出键码 `0x3c` = `<`），输入法完全靠键码本身来区分上档符号。

主处理 `QPinyinFrame::GetKey(this, unicode, keycode)`（按 keycode 分派）：

| keycode | 含义 | 处理 |
|---|---|---|
| `0x30–0x39` | 数字 0–9 | 选候选字 `commit_selection` |
| `0x61–0x7a` | 小写 a–z | 加入拼音串 |
| `0x41–0x5a` | 大写 A–Z | 转小写后加入拼音串 |
| `0x20` | 空格 | 有候选则选第一个，否则透传 |
| `0x1012/0x1013` | Left/Up | 上一页 |
| `0x1014/0x1015` | Right/Down | 下一页 |
| `0x1003` | Backspace | 删拼音串末字符 |
| **`0x1020`** | **Shift** | **切换造词模式 `this+0x141`← 问题所在** |
| `0x1001` | Tab | 切换另一模式 `this+0x140` |
| `0x1000` | Esc | 复位 |
| 其它 | 标点等 | `send_hanzi_mark(keycode)` → 全角标点 |

## 问题定位（造词分支）
`GetKey` 内 `0x17490` 起的代码块，当 `keycode == 0x1020 (Shift)` 时：
- 若造词标志位 `+0x141` 已置位 → 调 `append_phrase` 收词、复位；
- 若未置位（`0x17594`）：
  - 拼音串非空 → 什么都不做（吞掉）；
  - **拼音串为空 → `0x175f8~0x17600` 置位 `+0x141`，进入造词模式**。

关键跳转在 `0x175a0`：
```
17594: ldr  r3,[r4,#0x184]     ; 拼音串指针
17598: ldr  r2,[r3,#-0x10]     ; 拼音串长度
1759c: cmp  r2,#0
175a0: bne  0x17734            ; 非空→跳到结尾(吞掉)；空→继续往下"进入造词"
175a4: ... 进入造词模式的代码（置位 +0x141）...
```

## 补丁（单字节）
把 `0x175a0` 的条件跳转 `bne`（仅拼音串非空时跳过造词）改成**无条件跳转** `b`：

```
文件偏移 0x175A3:  0x1A (bne)  →  0xEA (b)
指令:  bne 0x17734  →  b 0x17734
```

效果：无论拼音串空不空，按 Shift 都直接跳到结尾——**"进入造词模式"的代码（置位 +0x141）变成永远走不到的死代码**。由于全程再没有任何地方把 `+0x141` 置 1，造词模式被彻底关闭。

按键行为（打补丁后）：
- 单按 **Shift** → 被安静吞掉，不再进造词、不输出任何东西；
- **Shift + space右侧键**（`<` = 0x3c）→ 走 `send_hanzi_mark` → 输出「**《**」；
- **Shift + 右侧第二键**（`>` = 0x3e）→ 输出「**》**」；
- **Shift + /**（`?` = 0x3f）→「？」；**Shift + ;**（`:` = 0x3a）→「：」；
- **Shift + 2**（`@` = 0x40，不在全角表内）→ 原样透传输出「@」。

这正是标准中文输入法的上档/全角标点行为。

## 验证
- 补丁后 `.so` 仍是合法 ARM ELF；重打包后符号链接、文件属主(zaurus/500)、权限完整保留。
- 与原始 `.so` 逐字节对比：**仅 1 处不同**（偏移 0x175A3，0x1A→0xEA），与设计一致。

## 安装（Zaurus 上）
```sh
# 建议先备份原插件
cp /opt/Qtopia/plugins/inputmethods/libmurphypinyin.so.0.03 ~/libmurphypinyin.so.0.03.bak
ipkg install -force-reinstall murphytalk.pinyin_0.03_arm_noshiftzaoci.ipk
# 然后重启 Qtopia（或注销/重进），重新选中该输入法
```

## 重要说明 / 已知风险
1. 本补丁的前提：Shift+符号在本机是以"上档键码"直接送达（如 `<`=0x3c）。这一点由三条证据支撑：`send_hanzi_mark` 里明确写了 0x3c→《、0x3e→》；`filter` 根本不看 modifier；以及你本人"space 右键 Shift 应出《"的预期。若个别按键实测出的是半角/基础键的全角（比如出「，」而不是「《」），说明该键送的是基础键码+修饰位，需要改用"在 filter/GetKey 里读 modifier"的加长补丁——把实测情况告诉我即可续改。
2. 造词模式现已**整体关闭**（因为 Shift 是它唯一的入口）。若你希望保留造词、只是换到别的键触发，可以再做一版把触发键从 Shift 改成某个不冲突的键。
