# UnrealBridge MCP stdio wrapper PR 实测报告

报告版本：v1.0

测试时间：2026-05-29 09:47:09 +08:00

工作区：

```text
C:\tmp\TornLux-UnrealBridge-mcp-pr-clean
```

基线提交：

```text
d63b334
```

相关组件版本：

```text
MCP wrapper server version: 0.1.0
Supported MCP protocol versions: 2024-11-05, 2025-03-26
Unreal Engine: 5.7.4-51494982+++UE5+Release-5.7
Test project: TEST_57
Project path: C:/Users/jingjun.wang/Documents/Unreal Projects/TEST_57/TEST_57.uproject
UnrealBridge TCP endpoint discovered: 127.0.0.1:6904
```

## 测试范围

本次实测覆盖 `mcp-stdio-wrapper` PR 的客户端层、无引擎 smoke、`bridge.py` preflight JSON 输出、MCP stdio live 调用、UnrealBridge discovery / ping / exec，以及 exec 队列超时后的延迟执行行为。

本次未重新执行 `RunUAT BuildPlugin`。上一次 clean 副本验证中 UE 5.7 普通 `RunUAT BuildPlugin` 通过，`-StrictIncludes` 暴露上游 include hygiene 问题，已记录到路线图。

## 代码状态

测试时 `git status --short`：

```text
 M .claude/skills/unreal-bridge/SKILL.md
 M .claude/skills/unreal-bridge/scripts/bridge.py
 M Plugin/UnrealBridge/Source/UnrealBridge/Private/UnrealBridgeServer.cpp
 M Plugin/UnrealBridge/Source/UnrealBridge/Public/UnrealBridgeServer.h
 M README.md
 M README.zh-CN.md
?? .claude/skills/unreal-bridge/scripts/unrealbridge_mcp_server.py
?? docs/mcp-stdio-wrapper.md
?? docs/plans/mcp-stdio-wrapper-roadmap.md
?? tools/smoke_mcp_stdio.py
```

## 通过项

### 1. MCP stdio 无引擎 smoke

命令：

```powershell
python tools\smoke_mcp_stdio.py
```

结果：

```text
MCP stdio smoke passed
```

结论：通过。

### 2. bridge.py preflight 正例

命令：

```powershell
"print('preflight ok')" | python .claude\skills\unreal-bridge\scripts\bridge.py --json preflight -
```

结果：

```json
{"ok": true, "errors": [], "warnings": []}
```

结论：通过。`warnings` 作为 list 输出，未再与 `errors` 混成 tuple。

### 3. bridge.py preflight 负例

命令内容：

```python
if True print("bad")
```

结果：

```json
{
  "ok": false,
  "errors": [
    "preflight L1: SyntaxError: invalid syntax"
  ],
  "warnings": []
}
```

结论：通过。错误返回码为 1，stdout 保持可解析 JSON，`errors` 和 `warnings` 均为 list。

### 4. 无 pycache 语法检查

检查文件：

```text
.claude/skills/unreal-bridge/scripts/bridge.py
.claude/skills/unreal-bridge/scripts/unrealbridge_mcp_server.py
tools/smoke_mcp_stdio.py
```

结果：

```text
compile ok
```

结论：通过。

### 5. UnrealBridge editor discovery

命令：

```powershell
python .claude\skills\unreal-bridge\scripts\bridge.py --json list-editors
```

结果摘要：

```json
[
  {
    "pid": 28344,
    "project": "TEST_57",
    "project_path": "C:/Users/jingjun.wang/Documents/Unreal Projects/TEST_57/TEST_57.uproject",
    "engine_version": "5.7.4-51494982+++UE5+Release-5.7",
    "tcp_bind": "127.0.0.1",
    "tcp_port": 6904,
    "token_fingerprint": ""
  }
]
```

结论：通过。

### 6. UnrealBridge ping

命令：

```powershell
python .claude\skills\unreal-bridge\scripts\bridge.py --json ping
```

结果摘要：

```json
{
  "success": true,
  "output": "pong",
  "ready": true
}
```

结论：通过。

### 7. GameThread liveness

命令：

```powershell
python .claude\skills\unreal-bridge\scripts\bridge.py --json gamethread-ping
```

结果摘要：

```json
{
  "success": true,
  "output": "alive",
  "latency_ms": 186.6018995642662,
  "ready": true
}
```

结论：通过。

### 8. MCP stdio live tool calls

通过 `unrealbridge_mcp_server.py` 发起 MCP 初始化后，调用以下 tools：

```text
bridge_list_editors
bridge_ping
bridge_exec
```

`bridge_exec` 测试代码：

```python
print("mcp live exec ok")
```

结果摘要：

```json
{
  "bridge_list_editors": "ok",
  "bridge_ping": "pong",
  "bridge_exec": "mcp live exec ok"
}
```

结论：通过。MCP wrapper 能通过 stdio 调用 live UnrealBridge，并成功执行 Unreal Python。

## 未通过项

### exec 排队超时后不应延迟执行

测试方法：

1. 第一个 exec 执行 `time.sleep(3)`，占住 GameThread exec 队列。
2. 第二个 exec 使用 `--timeout 0.5`，内容为写入 marker 文件。
3. 等第一个 exec 结束后，再检查 marker 文件是否存在。

第二个 exec 返回：

```json
{
  "success": false,
  "error": "exec timeout after 0.5s",
  "ready": true
}
```

但等待后 marker 文件存在：

```text
C:\tmp\unrealbridge_timeout_should_not_exist.txt
```

marker 内容：

```text
late exec ran
```

结论：未通过。当前运行中的 UE 插件仍会在客户端 timeout 后执行尚未开始的排队项。

## 未通过原因定位

对比 clean PR 副本与当前 `TEST_57` 项目插件源码：

```text
C:\tmp\TornLux-UnrealBridge-mcp-pr-clean\Plugin\UnrealBridge\Source\UnrealBridge\Private\UnrealBridgeServer.cpp
C:\Users\jingjun.wang\Documents\Unreal Projects\TEST_57\Plugins\UnrealBridge\Source\UnrealBridge\Private\UnrealBridgeServer.cpp
```

项目插件仍是旧实现，缺少 clean 副本中的取消逻辑：

```cpp
Pending->bCancelled = true;
```

也缺少 `TickConsumeQueue` 中对 `Pending->bCancelled` 的跳过执行分支。

因此，本次未通过项不是 MCP wrapper 客户端层问题，而是当前运行中的 `TEST_57` 未加载 clean PR 副本里的最新插件实现。

## 当前结论

MCP stdio wrapper、preflight JSON 输出、live MCP tool 调用、editor discovery、ping、GameThread liveness 均通过。

`exec timeout 后尚未执行的队列项不应稍后执行` 尚未在当前引擎实例通过。必须先把 clean PR 副本里的插件源码同步到 `TEST_57/Plugins/UnrealBridge`，重新编译并重启 UE，再复测该项。

## 建议下一步

1. 将 clean PR 副本的 `Plugin/UnrealBridge` 同步到测试项目：

```text
C:\Users\jingjun.wang\Documents\Unreal Projects\TEST_57\Plugins\UnrealBridge
```

2. 重新编译插件并重启 UE。
3. 复测 `exec 排队超时后不应延迟执行`。
4. 若复测通过，再将该项标记为实机通过。

## 状态更新

2026-05-29 10:00:59 +08:00：已将 exec timeout cancellation 相关源码同步到
`TEST_57/Plugins/UnrealBridge`。当前 Unreal Editor 仍在运行，编译与实机复测待编辑器关闭后继续。

2026-05-29 10:03:38 +08:00：使用当前 `TEST_57` 插件源码执行 UE 5.7
`RunUAT BuildPlugin` 临时打包验证通过，输出目录为
`C:\tmp\UnrealBridgeTimeoutCancelBuild57_1001`。项目正在运行的插件 DLL 尚未重新生成，live 复测仍待编辑器关闭后继续。

2026-05-29 10:18:16 +08:00：关闭 Unreal Editor 后，将新编译的 UnrealBridge
DLL 同步到 `TEST_57` 项目插件目录并重新启动编辑器。复测
`exec 排队超时后不应延迟执行` 通过：第二个请求返回
`exec timeout after 0.5s`，等待第一个请求结束后
`C:\tmp\unrealbridge_timeout_should_not_exist.txt` 未生成。

附注：项目目标编译期间 `VisualStudioTools` 触发 UBT rules 解析错误。为完成本次
UnrealBridge 复测，曾临时从 `TEST_57.uproject` 移除该插件引用，编译通过后已恢复原
`.uproject` 内容与时间戳；该问题与本次 UnrealBridge timeout cancellation 变更无关。
